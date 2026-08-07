# Axle

[![C](https://img.shields.io/badge/language-C-A8B9CC.svg)](https://www.iso.org/standard/82075.html)
[![Platform](https://img.shields.io/badge/platform-Linux-FCC624.svg)](#requirements)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
![Version](https://img.shields.io/badge/version-0.0.1-orange.svg)
![Status](https://img.shields.io/badge/status-in%20development-orange.svg)

**Axle** is a small JSON-driven build system for modular C and C++ projects. It builds executables and libraries, resolves local modules, clones and isolates Git dependencies, and provides a simple file-based test runner.

> [!IMPORTANT]
> Axle is under active development. It is already used by real projects, but its configuration format and dependency-management behavior may still change before a stable release.

## Why Axle?

Axle is designed for projects that need more structure than a shell script but do not need a large build-language ecosystem.

Its main ideas are:

- project configuration in readable JSON;
- local modules stored under `.modules/`;
- Git dependencies stored under `.vendor/`;
- isolated transitive remote dependencies;
- inherited compiler and target defaults;
- direct control over compiler and linker command lines;
- no generated project files;
- simple standalone tests where success is an exit code of `0`.

## Features

- C and C++ compilation through a configurable compiler command
- Executable, static-library, and shared-library outputs
- Incremental source compilation based on timestamps
- Forced recompilation with `--clean`
- Standard glob patterns and recursive `**` source discovery
- Configurable include paths, library paths, libraries, defines, and flags
- Named build targets with `O0`, `O1`, `O2`, `O3`, and `Ofast`
- Shared `defaults.json` configuration
- Local dependency graph under `.modules/`
- Git-based `remote_deps` under `.vendor/`
- Recursive and isolated transitive remote dependencies
- Exported module names through `.as.module.json`
- Remote dependency version checks
- Export-name conflict detection
- Remote build caching by Git commit SHA
- Test discovery, build, execution, filtering, and summary output
- Module scaffolding with `axle module`

## Requirements

Axle currently targets Linux and other sufficiently compatible POSIX environments.

Required tools and libraries:

- a C compiler supporting GNU/POSIX C APIs;
- `git` for remote dependencies;
- a POSIX shell environment;
- `pkg-config` when project configurations use `code.packages`;
- `yyjson`, provided by the repository;
- the compilers and linkers named by each project's `defaults.json`.

The current implementation uses Linux/POSIX facilities including `linux/limits.h`, `glob`, `fnmatch`, `unistd`, symbolic links, `popen`, and shell command execution. Native Windows support is not currently available.

## Building Axle from source

Clone the repository:

```bash
git clone https://github.com/agnostic-t/axle.git
cd axle
```

A script bootstrap build for the source layout shown in this repository is:

```bash
chmod +x ./scripts/build
./scripts/build
```

Install the binary in `/usr/local/bin/axle`:

```bash
chmod +x ./scripts/install
./scripts/install
```

Verify the installation:

```bash
axle help
```

## Quick start

Create a project:

```text
hello/
├── src/
│   └── main.c
├── defaults.json
└── module.json
```

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
    "name": "hello",
    "version": "0.1.0"
  },

  "compiler": {
    "binary": "executable"
  },

  "code": {
    "sources": ["src/**/*.c"],
    "includes": ["include"],
    "output": "bin/hello"
  },

  "target": "debug",
  "defaults": "defaults.json"
}
```

`src/main.c`:

```c
#include <stdio.h>

int main(void)
{
    puts("Hello from Axle");
    return 0;
}
```

Build and run:

```bash
axle build .
./bin/hello
```

`axle build` accepts either a project directory or a direct path to `module.json`:

```bash
axle build .
axle build ./module.json
```

## Command line

```text
axle build PATH [--clean] [--update]
axle module NAME
axle test [NAME] [--only-build]
axle help
```

### Build a project

```bash
axle build .
```

Flags:

- `--clean` recompiles all local source files even when their object files appear up to date;
- `--update` resets and pulls top-level remote dependency repositories before building them.

```bash
axle build . --clean
axle build . --update
axle build . --clean --update
```

### Create a local module

```bash
axle module math
```

This creates:

```text
.modules/math/
├── bin/
├── include/
├── src/
└── module.json
```

The generated module is a C static-library template using `**/*.c`.

### Run tests

```bash
axle test
```

Run one test by source filename without its extension:

```bash
axle test buffer
```

Build tests without running or deleting their binaries:

```bash
axle test --only-build
axle test buffer --only-build
```

## Local modules

Local modules live under `.modules/<name>/` and contain their own `module.json`.

Declare them through `dependencies`:

```json
{
  "dependencies": {
    "math": {
      "version": ">=0.1.0"
    }
  }
}
```

Axle resolves transitive local dependencies, sorts modules by `metadata.priority`, builds them, and merges their include and library paths into downstream modules.

Smaller priority values are built earlier. The default priority is `1000`.

```json
{
  "metadata": {
    "name": "math",
    "version": "0.1.0",
    "priority": 10
  }
}
```

## Remote dependencies

Remote repositories are declared separately from local module dependencies:

```json
{
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
  }
}
```

The `remote_deps` key identifies the repository directory under `.vendor/`. The public module name is defined by the dependency repository's `.as.module.json`.

During a build, Axle:

1. clones the repository into `.vendor/<repository-name>/`;
2. reads its `.as.module.json`;
3. registers exported modules as symbolic links in `.modules/`;
4. checks exported versions;
5. recursively installs and builds its own `remote_deps`;
6. builds its local module graph and root module;
7. records the built Git commit in `.last_built`.

Each remote repository owns its own `.vendor/` directory. This keeps transitive dependencies isolated instead of flattening the complete graph into the top-level project.

## Exporting a repository

A repository becomes consumable as a remote dependency through `.as.module.json`.

Export the repository root as `mylib`:

```json
{
  "export": {
    "modules": {
      "mylib": {
        "version": "0.1.0"
      }
    },
    "tests": false
  }
}
```

Export an internal `.modules/core` module as `mylib-core`:

```json
{
  "export": {
    "modules": {
      "core": {
        "name": "mylib-core",
        "version": "0.1.0"
      }
    },
    "tests": false
  }
}
```

Multiple modules can be exported from one repository.

The old top-level `insert` form is still parsed for compatibility, but it is deprecated and prints a warning.

## Tests

The root `module.json` selects a test directory:

```json
{
  "testsPath": ".tests"
}
```

That directory contains its own `module.json`:

```json
{
  "metadata": {
    "name": "project-tests",
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
    "mylib": {
      "version": ">=0.1.0"
    }
  },

  "defaults": "defaults.json",
  "target": "debug"
}
```

Each file matched by `code.sources` is treated as an independent test executable. A test passes when it exits with status `0`.

When tests are run normally, Axle removes their binaries after the complete test run. `--only-build` keeps them under `<testsPath>/bin/`.

## Configuration

The complete configuration reference is available in [`usage.md`](usage.md).

The main files are:

| File | Purpose |
|---|---|
| `module.json` | Project or module metadata, sources, output, dependencies, and selected target |
| `defaults.json` | Shared compiler, linker, and target definitions |
| `.as.module.json` | Public exports of a remote dependency repository |
| `.vendor/exported.json` | Axle-managed map of exported names to repositories |
| `.vendor/<repo>/.last_built` | Axle-managed Git SHA of the last successful remote build |

## Project layout

A typical modular project looks like this:

```text
.
├── module.json
├── defaults.json
├── src/
├── include/
├── .modules/
│   ├── core/
│   │   ├── module.json
│   │   ├── src/
│   │   ├── include/
│   │   └── bin/
│   └── utility/
├── .vendor/
│   ├── exported.json
│   └── remote-repository/
└── .tests/
    ├── module.json
    ├── defaults.json
    └── *.c
```

`.vendor/`, generated object directories, module binaries, and test binaries should normally be excluded from version control. Whether `.modules/` is committed depends on whether it contains project-owned local modules or generated remote-dependency links.

## Current limitations

- Axle is pre-1.0 and configuration compatibility is not guaranteed.
- The implementation is currently Linux/POSIX-specific.
- Incremental rebuild checks compare source and object timestamps; header and compiler-flag changes are not tracked.
- `--clean` forces recompilation of the main project and local modules, but it does not force a rebuild of an unchanged remote dependency.
- `--update` updates top-level remote dependencies; nested remote dependencies are not force-updated by that flag.
- Updating a remote dependency performs `git reset --hard` before `git pull`, so local edits inside `.vendor/` are discarded.
- Local dependency version strings are parsed, but version mismatches are not currently enforced during local module resolution.
- Remote version checks require an exported version in `.as.module.json`.
- Shared-library output currently uses the Unix `.so` suffix.
- `code.packages` currently contributes `pkg-config --cflags` during compilation, but its `--libs` result is not appended to the final link command.
- Test dependencies should already be built; running `axle build .` before `axle test` is recommended.
- Paths and shell commands are not yet designed for untrusted configuration input.

## Contributing

Issues, bug reports, documentation improvements, and pull requests are welcome.

Useful areas for contribution include:

- portable path and process handling;
- dependency lockfiles and reproducible revisions;
- stronger incremental build tracking;
- local dependency version enforcement;
- complete `pkg-config` link integration;
- improved diagnostics;
- automated tests for dependency graphs and exports.

## License

Axle is free and open-source software licensed under the **GNU General Public License v3.0**.

See [`LICENSE`](LICENSE) for the full license text.

Third-party components retain their respective licenses.
