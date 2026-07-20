# Axle configuration reference

This document describes the current Axle `0.0.1` configuration and command-line behavior.

Axle uses three user-facing JSON files:

- `module.json` describes a project or module;
- `defaults.json` contains reusable compiler and target settings;
- `.as.module.json` defines what a Git dependency exports to consumers.

Axle also creates internal metadata under `.vendor/`.

## Table of contents

1. [Command-line interface](#command-line-interface)
2. [Configuration loading](#configuration-loading)
3. [`module.json`](#modulejson)
4. [`defaults.json`](#defaultsjson)
5. [Source and output behavior](#source-and-output-behavior)
6. [Local dependencies](#local-dependencies)
7. [Remote dependencies](#remote-dependencies)
8. [`.as.module.json`](#asmodulejson)
9. [Tests](#tests)
10. [Generated files and directories](#generated-files-and-directories)
11. [Complete examples](#complete-examples)
12. [Current implementation notes](#current-implementation-notes)

## Command-line interface

### Help

```bash
axle help
```

### Build

```bash
axle build PATH [--clean] [--update]
```

`PATH` may point to:

- a directory containing `module.json`; or
- a specific `module.json` file.

Examples:

```bash
axle build .
axle build ./module.json
axle build ./examples/app
```

#### `--clean`

```bash
axle build . --clean
```

Forces recompilation of source files in the main project and project-owned local modules.

It does not delete the object directory. Existing object files are overwritten as their corresponding sources are compiled.

Remote dependencies still use their Git-SHA build cache.

#### `--update`

```bash
axle build . --update
```

For each top-level `remote_deps` repository that is already present, Axle performs the equivalent of:

```bash
git -C <repo> reset --hard
git -C <repo> pull -q
```

This discards local edits inside `.vendor/<repo>/`.

The current implementation passes forced update only to top-level remote dependencies. Nested remote dependencies are reused unless their own checkout changes through another mechanism.

### Create a local module

```bash
axle module NAME
```

Example:

```bash
axle module math
```

Creates:

```text
.modules/math/
├── bin/
├── include/
├── src/
└── module.json
```

The generated `module.json` is approximately:

```json
{
  "metadata": {
    "name": "math",
    "version": "0.0.0",
    "priority": 100
  },

  "compiler": {
    "binary": "static-lib"
  },

  "code": {
    "sources": ["**/*.c"],
    "output": "./bin/math",
    "includes": ["include"]
  },

  "target": "debug"
}
```

The command always creates the module under the current working directory's `.modules/`.

### Test

```bash
axle test [NAME] [--only-build]
```

Examples:

```bash
axle test
axle test buffer
axle test --only-build
axle test buffer --only-build
```

`NAME` is the test source filename without `.c` or `.cpp`.

`axle test` currently assumes it is executed from the project root.

## Configuration loading

Axle builds an internal receipt by merging configuration from defaults and the active module.

The practical precedence is:

1. settings directly declared in the active `module.json`;
2. inherited project or repository defaults;
3. lower-level defaults selected through `defaults`.

For a top-level project, `<project-root>/defaults.json` is detected automatically when it exists.

Local modules inherit the top-level project's defaults.

For a remote repository:

1. the remote repository's own root `defaults.json` is used when present;
2. otherwise it inherits defaults from its parent project or parent remote dependency.

This inheritance continues recursively through nested `remote_deps`.

A module can also declare:

```json
{
  "defaults": "defaults.json",
  "target": "debug"
}
```

The `defaults` path is resolved relative to that module's directory.

## `module.json`

A complete example:

```json
{
  "metadata": {
    "name": "application",
    "version": "0.4.0",
    "priority": 100
  },

  "compiler": {
    "cc": "clang++",
    "linker": "ar",
    "binary": "executable",
    "objects_path": ".objs"
  },

  "code": {
    "sources": ["src/**/*.cpp"],
    "includes": ["include"],
    "libraries": ["m", "pthread"],
    "lib-dirs": ["lib"],
    "packages": ["some-package"],
    "output": "bin/application"
  },

  "dependencies": {
    "core": {
      "version": ">=0.2.0"
    }
  },

  "remote_deps": {
    "external-core-repository": {
      "url": "https://github.com/example/core.git",
      "version": ">=0.2.0"
    }
  },

  "defaults": "defaults.json",
  "target": "debug",
  "testsPath": ".tests",
  "only-deps": false
}
```

Every block is optional at the parser level, but a successful build still requires enough information to produce a valid compiler and linker command.

### `metadata`

```json
{
  "metadata": {
    "name": "core",
    "version": "1.2.3",
    "priority": 20
  }
}
```

#### `name`

Project or module name used in logs and dependency resolution.

#### `version`

Version in `X.Y.Z` form.

For exported remote modules, version requirements support:

- `>=X.Y.Z`
- `<=X.Y.Z`
- `=X.Y.Z`

#### `priority`

Controls local module build order.

Smaller values are built earlier. The default is `1000`.

When priorities are equal, Axle preserves an internal dependency-discovery order rather than treating priority as a complete topological specification. Dependencies should therefore still be declared correctly.

### `compiler`

```json
{
  "compiler": {
    "cc": "clang++",
    "linker": "ar",
    "binary": "static-lib",
    "objects_path": ".objs"
  }
}
```

#### `cc`

Compiler command used for source compilation.

The same command is used as the linker driver for:

- executables;
- shared libraries.

The value may include arguments:

```json
{
  "cc": "zig c++ -target x86_64-windows"
}
```

Axle currently builds shell command strings directly, so quoting and shell behavior are significant.

#### `linker`

Archive command used for static libraries.

Typical value:

```json
{
  "linker": "ar"
}
```

Axle invokes it as:

```text
<linker> rcs <output>.a <objects...>
```

The `linker` value is not used for executable or shared-library linking.

#### `binary`

Supported values:

| Value | Result |
|---|---|
| `executable` | Executable at the configured output path |
| `static-lib` | Static archive with `lib` prefix and `.a` suffix |
| `shared-lib` | Shared library with `lib` prefix and `.so` suffix |

Unknown values fall back to `executable` with an error message.

#### `objects_path`

Directory for generated object files.

Example:

```json
{
  "objects_path": ".objs"
}
```

Object filenames are derived from source paths by replacing `/` with `_` and replacing the source extension with `.o`.

### `code`

```json
{
  "code": {
    "sources": ["src/**/*.c", "vendor/single_file.c"],
    "includes": ["include", "generated/include"],
    "libraries": ["m", "pthread"],
    "lib-dirs": ["lib", "vendor/lib"],
    "packages": ["libexample"],
    "output": "bin/program"
  }
}
```

#### `sources`

List of source paths or patterns.

Supported behavior includes:

- literal relative paths;
- absolute paths;
- standard glob characters such as `*`, `?`, character classes, and braces;
- recursive `**`.

Examples:

```json
{
  "sources": [
    "src/*.c",
    "src/**/*.cpp",
    "generated/{parser,lexer}.c"
  ]
}
```

Recursive glob matching skips entries whose names begin with `.`.

When no files match a non-recursive glob, Axle prints a warning and continues. A project that ultimately has no sources fails unless `only-deps` is enabled.

#### `includes`

Include directories. Each value is passed to the compiler with `-I`.

```json
{
  "includes": ["include", "third_party/include"]
}
```

#### `libraries`

Logical library names. Each value is passed as `-l<name>`.

Use:

```json
{
  "libraries": ["m", "pthread", "example"]
}
```

This produces arguments similar to:

```text
-lm -lpthread -lexample
```

Do not include the `lib` prefix or `.a`/`.so` suffix when the target linker follows normal Unix `-l` naming.

#### `lib-dirs`

Library search directories. Each value is passed as `-L<path>`.

```json
{
  "lib-dirs": ["lib", "third_party/lib"]
}
```

#### `packages`

`pkg-config` package names.

```json
{
  "packages": ["glfw3"]
}
```

During compilation, Axle appends `pkg-config --cflags` output for each package.

In the current implementation, `pkg-config --libs` metadata is constructed internally but is not appended to executable or shared-library link commands. Libraries required by a package may therefore also need to be listed manually in `libraries` and `lib-dirs`, or included through compiler flags.

#### `output`

Output base path.

Executable:

```json
{
  "compiler": {
    "binary": "executable"
  },
  "code": {
    "output": "bin/app"
  }
}
```

Produces:

```text
bin/app
```

Static library:

```json
{
  "compiler": {
    "binary": "static-lib"
  },
  "code": {
    "output": "bin/core"
  }
}
```

Produces:

```text
bin/libcore.a
```

Shared library:

```json
{
  "compiler": {
    "binary": "shared-lib"
  },
  "code": {
    "output": "bin/core"
  }
}
```

Produces:

```text
bin/libcore.so
```

For library outputs, omit both the `lib` prefix and extension. Axle adds them.

If the output filename already starts with `lib`, Axle does not add a second prefix.

### `target`

Selects a named build target from the active defaults configuration:

```json
{
  "target": "debug"
}
```

### `defaults`

Selects a defaults file relative to the module directory:

```json
{
  "defaults": "defaults.json"
}
```

### `dependencies`

Local module dependencies:

```json
{
  "dependencies": {
    "core": {
      "version": ">=1.0.0"
    },
    "utility": {
      "version": "=0.5.0"
    }
  }
}
```

Local modules are resolved from:

```text
<project-root>/.modules/<dependency-name>/module.json
```

Do not place a `url` in `dependencies`. Current Axle prints a warning and ignores that field. Git repositories belong in `remote_deps`.

Version strings are parsed for local dependencies, but local module version mismatch enforcement is not currently implemented.

### `remote_deps`

Git repositories:

```json
{
  "remote_deps": {
    "core-repository": {
      "url": "https://github.com/example/core.git",
      "version": ">=1.0.0"
    }
  }
}
```

Fields:

| Field | Required | Meaning |
|---|---:|---|
| key | yes | Repository directory name under `.vendor/` |
| `url` | yes | Git clone URL |
| `version` | no | Required export version; defaults to `>=0.0.0` |

The remote repository must provide `.as.module.json`.

The remote repository key does not have to match its exported module name.

### `only-deps`

```json
{
  "only-deps": true
}
```

When enabled, Axle resolves and builds dependencies but skips compilation and linking of the current module.

This is useful for aggregate modules:

```json
{
  "metadata": {
    "name": "all-modules",
    "version": "0.1.0"
  },

  "dependencies": {
    "core": {
      "version": ">=0.1.0"
    },
    "graphics": {
      "version": ">=0.1.0"
    }
  },

  "only-deps": true
}
```

### `testsPath`

```json
{
  "testsPath": ".tests"
}
```

Path to the directory used by `axle test`.

The field name is case-sensitive.

## `defaults.json`

Example:

```json
{
  "compiler": {
    "cc": "clang++",
    "linker": "ar",
    "binary": "executable",
    "objects_path": ".objs"
  },

  "targets": {
    "debug": {
      "flags": [
        "-g",
        "-Wall",
        "-Wextra"
      ],
      "defines": [
        {
          "DEBUG": "1"
        }
      ],
      "optimization": "O0"
    },

    "release": {
      "flags": [
        "-DNDEBUG"
      ],
      "optimization": "O3"
    }
  }
}
```

### `targets`

Each key under `targets` is a selectable target name.

```json
{
  "targets": {
    "asan": {
      "flags": [
        "-g",
        "-fsanitize=address"
      ],
      "defines": [
        {
          "AXLE_ASAN": "1"
        }
      ],
      "optimization": "O1"
    }
  }
}
```

#### `flags`

Raw compiler/linker flags joined with spaces.

For executables and shared libraries, the active target flags are passed both during compilation and final linking.

For static libraries, target flags affect compilation; archiving uses the configured static linker command.

#### `defines`

Array of single-key objects:

```json
{
  "defines": [
    {
      "FEATURE_A": "1"
    },
    {
      "PROJECT_NAME": "\"demo\""
    }
  ]
}
```

These become:

```text
-DFEATURE_A=1 -DPROJECT_NAME="demo"
```

Defines are applied during compilation.

#### `optimization`

Supported values:

- `O0`
- `O1`
- `O2`
- `O3`
- `Ofast`

Unknown or omitted values fall back to `O0`.

## Source and output behavior

### Incremental compilation

For each source, Axle compares the modification time of the source file with its object file.

A source is rebuilt when:

- the object file does not exist;
- the source is newer than the object;
- `--clean` is used.

Axle does not currently track:

- included headers;
- compiler command changes;
- target flag changes;
- define changes;
- dependency artifacts.

Delete the object directory or use `--clean` after such changes.

### Object filenames

A source path such as:

```text
src/render/backend.c
```

becomes an object name similar to:

```text
.objs/src_render_backend.o
```

Leading `.` and `_` characters are trimmed from the flattened name.

### Shared-library compilation

Sources of a `shared-lib` target are compiled with `-fPIC`.

The final link command uses:

```text
<compiler> <target-flags> -shared -o <output>.so <objects> ...
```

### Static-library archiving

Static libraries use:

```text
<linker> rcs <output>.a <objects>
```

## Local dependencies

Local dependencies are project-owned modules under `.modules/`.

Example:

```text
.modules/
├── utility/
│   ├── module.json
│   ├── include/
│   ├── src/
│   └── bin/
└── core/
    ├── module.json
    ├── include/
    ├── src/
    └── bin/
```

An application can declare:

```json
{
  "dependencies": {
    "core": {
      "version": ">=0.1.0"
    }
  }
}
```

If `core` depends on `utility`, Axle discovers the transitive dependency.

### Merge behavior

For each resolved dependency, Axle merges:

- `.modules/<name>/include` into downstream include paths;
- `.modules/<name>/bin` into downstream library search paths when it exists;
- the output library name into downstream `libraries` when the module produces a static or shared library.

Executable dependencies do not contribute a `-l` entry.

The original explicit `libraries`, `lib-dirs`, and `packages` of the consuming module are restored after dependency metadata is merged.

### Build order

Axle sorts discovered modules by ascending `metadata.priority`.

Smaller values build first.

A correct dependency declaration is still required. Priority should be used to resolve intentional ordering among modules, not as a substitute for declaring dependencies.

## Remote dependencies

### Declaration

```json
{
  "remote_deps": {
    "graphics-repository": {
      "url": "https://github.com/example/graphics.git",
      "version": ">=0.3.0"
    }
  },

  "dependencies": {
    "graphics": {
      "version": ">=0.3.0"
    }
  }
}
```

The repository is cloned to:

```text
.vendor/graphics-repository/
```

Its exported module may be linked into:

```text
.modules/graphics
```

The first name identifies the repository checkout. The second name identifies the public module export.

### Installation process

For each remote dependency, Axle:

1. checks that `git` is available;
2. creates `.vendor/` if required;
3. clones the repository if it is missing;
4. optionally updates it when `--update` is active;
5. parses `.as.module.json`;
6. validates export versions;
7. checks for export-name conflicts;
8. creates or refreshes `.modules/<export-name>` symbolic links;
9. updates `.vendor/exported.json`;
10. recursively builds the repository.

### Export conflicts

Axle refuses to register an export name already owned by a different repository.

Example conflict:

```text
repo-a exports "core"
repo-b exports "core"
```

The second installation fails instead of silently replacing the first.

The ownership map is stored in:

```text
.vendor/exported.json
```

### Nested dependencies and isolation

A remote dependency may have its own `remote_deps`.

They are cloned inside that repository:

```text
.vendor/parent-repository/.vendor/child-repository/
```

Their exports are linked inside the parent's own `.modules/`.

This creates an isolated dependency hierarchy rather than one flattened global vendor directory.

Axle merges the required include and library metadata back into dependent receipts so transitive remote libraries remain linkable.

### Remote build cache

After a successful remote build, Axle writes the current 40-character Git HEAD SHA to:

```text
.vendor/<repo>/.last_built
```

A remote dependency is skipped when:

- `.last_built` exists; and
- its saved SHA equals the repository's current HEAD.

A missing or different SHA triggers a rebuild.

`--clean` currently does not override this remote SHA cache.

### Defaults inheritance

For a remote dependency:

1. its own root `defaults.json` wins when present;
2. otherwise it inherits the effective defaults of its parent.

The same rule applies recursively to child remote dependencies.

## `.as.module.json`

Remote repositories expose public modules through `.as.module.json`.

### Export the repository root

```json
{
  "export": {
    "modules": {
      "mylib": {
        "version": "1.0.0"
      }
    },
    "tests": false
  }
}
```

This creates a consumer-side link conceptually equivalent to:

```text
.modules/mylib -> ../.vendor/<repo>
```

The key `mylib` is both the exported name and the dependency name used by consumers.

### Export an internal module under another name

```json
{
  "export": {
    "modules": {
      "core": {
        "name": "mylib-core",
        "version": "1.0.0"
      }
    },
    "tests": false
  }
}
```

This exports:

```text
.vendor/<repo>/.modules/core
```

as:

```text
.modules/mylib-core
```

In this form:

- object key `core` is the internal module directory;
- `name` is the public import name.

### Export multiple internal modules

```json
{
  "export": {
    "modules": {
      "core": {
        "name": "mylib-core",
        "version": "1.0.0"
      },
      "io": {
        "name": "mylib-io",
        "version": "1.0.0"
      }
    },
    "tests": false
  }
}
```

### Export entry fields

| Field | Meaning |
|---|---|
| object key | Public name when `name` is omitted; otherwise internal `.modules/<key>` name |
| `name` | Optional public export-name override |
| `version` | Version checked against the consumer's `remote_deps` requirement |
| `priority` | Parsed export metadata; current build order normally comes from the exported module's own `module.json` |
| `tests` | Repository-level export preference; currently parsed and reported, but it does not change Git clone contents |

Use a plain `X.Y.Z` value for exported versions:

```json
{
  "version": "1.2.3"
}
```

The consumer supplies the comparison operator:

```json
{
  "version": ">=1.2.0"
}
```

### Deprecated legacy form

Axle still accepts:

```json
{
  "insert": {
    "modules": {
      "_": {
        "name": "mylib",
        "version": "1.0.0"
      }
    },
    "tests": false
  }
}
```

Only `_` is meaningful in the legacy form.

Axle prints a warning recommending migration from `insert` to `export`.

## Tests

### Root configuration

The root module selects the tests directory:

```json
{
  "testsPath": ".tests"
}
```

### Test module configuration

`.tests/module.json` describes the shared build configuration for test files:

```json
{
  "metadata": {
    "name": "application-tests",
    "version": "0.1.0"
  },

  "compiler": {
    "binary": "executable"
  },

  "code": {
    "sources": ["*.c"],
    "includes": ["include"]
  },

  "dependencies": {
    "core": {
      "version": ">=0.1.0"
    }
  },

  "defaults": "defaults.json",
  "target": "debug"
}
```

Each matched `.c` or `.cpp` source is compiled and linked independently.

For:

```text
.tests/
├── parser.c
├── paths.c
└── strings.c
```

Axle discovers tests named:

```text
parser
paths
strings
```

### Running all tests

```bash
axle test
```

For every source, Axle:

1. sets that single file as the active source list;
2. creates an executable under `<testsPath>/bin/<test-name>`;
3. runs it;
4. records its exit status;
5. removes all generated test binaries after the run;
6. prints a summary.

### Running one test

```bash
axle test paths
```

When the name is not found, Axle prints the available test names and source paths.

### Build only

```bash
axle test --only-build
```

or:

```bash
axle test paths --only-build
```

The binaries are kept on disk.

### Test dependencies

Test dependencies are resolved from the root project's:

```text
<project-root>/.modules/
```

They are not resolved from `<testsPath>/.modules/`.

The current test implementation does not actively build every dependency before linking tests. Run:

```bash
axle build .
axle test
```

when the test module links project libraries.

### Passing and failing

Axle does not require a testing framework.

A test passes when the executable exits with status `0`. Any other status is a failure.

## Generated files and directories

### `.objs` or configured object directory

Contains flattened object files for a project or module.

### `.modules`

Contains:

- project-owned local modules;
- symbolic links registered from remote dependency exports.

### `.vendor`

Contains remote Git repositories and Axle metadata.

Example:

```text
.vendor/
├── exported.json
├── graphics-repository/
│   ├── .last_built
│   ├── .modules/
│   ├── .vendor/
│   ├── module.json
│   └── .as.module.json
└── utility-repository/
```

### `.vendor/exported.json`

Axle-managed map:

```json
{
  "graphics": {
    "repo": "graphics-repository"
  },
  "utility": {
    "repo": "utility-repository"
  }
}
```

The file is written through a temporary file and rename operation.

Do not edit it manually while Axle is operating.

### `.last_built`

Contains the Git HEAD SHA used to decide whether a remote dependency requires rebuilding.

## Complete examples

### C executable

`defaults.json`:

```json
{
  "compiler": {
    "cc": "cc",
    "linker": "ar",
    "binary": "executable",
    "objects_path": ".objs"
  },

  "targets": {
    "debug": {
      "flags": ["-g", "-Wall", "-Wextra"],
      "optimization": "O0"
    },
    "release": {
      "flags": ["-DNDEBUG"],
      "optimization": "O3"
    }
  }
}
```

`module.json`:

```json
{
  "metadata": {
    "name": "sample-c",
    "version": "0.1.0"
  },

  "compiler": {
    "binary": "executable"
  },

  "code": {
    "sources": ["src/**/*.c"],
    "includes": ["include"],
    "libraries": ["m"],
    "output": "bin/sample-c"
  },

  "defaults": "defaults.json",
  "target": "debug"
}
```

### C++ static library

```json
{
  "metadata": {
    "name": "geometry",
    "version": "0.2.0",
    "priority": 20
  },

  "compiler": {
    "binary": "static-lib"
  },

  "code": {
    "sources": ["src/**/*.cpp"],
    "includes": ["include"],
    "output": "bin/geometry"
  },

  "dependencies": {
    "math": {
      "version": ">=0.1.0"
    }
  },

  "target": "release"
}
```

With a C++ compiler declared in inherited defaults, this produces:

```text
bin/libgeometry.a
```

### Aggregate module

```json
{
  "metadata": {
    "name": "complete-sdk",
    "version": "0.1.0"
  },

  "dependencies": {
    "core": {
      "version": ">=0.1.0"
    },
    "network": {
      "version": ">=0.1.0"
    },
    "graphics": {
      "version": ">=0.1.0"
    }
  },

  "only-deps": true,
  "defaults": "defaults.json",
  "target": "release"
}
```

### Remote library consumer

```json
{
  "metadata": {
    "name": "consumer",
    "version": "0.1.0"
  },

  "compiler": {
    "binary": "executable"
  },

  "code": {
    "sources": ["src/**/*.cpp"],
    "output": "bin/consumer"
  },

  "remote_deps": {
    "ygpu-repository": {
      "url": "https://github.com/striter-no/ygpu.git",
      "version": ">=0.0.0"
    }
  },

  "dependencies": {
    "ygpu": {
      "version": ">=0.0.0"
    }
  },

  "defaults": "defaults.json",
  "target": "debug"
}
```

## Current implementation notes

These notes describe current behavior rather than intended future behavior.

- Axle's version string is currently `0.0.1`.
- Build and dependency operations construct shell commands and execute them through `system`.
- Remote cloning and updating require the system `git` executable.
- A remote update performs `reset --hard` before pulling.
- Git dependency checkouts are not pinned by a lockfile.
- Remote rebuild caching is based on the checkout's current HEAD SHA.
- Local dependency version constraints are not currently enforced.
- Export version constraints support only `>=`, `<=`, and exact `=`.
- Nested remote dependencies are isolated in the parent's `.vendor/`.
- Export-name conflicts are hard errors.
- Symbolic links are used to expose remote modules through `.modules/`.
- Static libraries always use `.a`; shared libraries always use `.so`.
- Header changes do not trigger incremental source recompilation.
- `pkg-config --libs` output is currently not used during linking.
- `testsPath` uses camel case and must match exactly.
- Normal test runs remove test binaries only after all selected tests finish.
