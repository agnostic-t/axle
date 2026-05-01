# How to write `module.json`

All configuration consists of blocks, some of them are optional. Here goes descriptions for all existing blocks:

1. `metadata`
  - Block provides *module name* and *version* of the package:
    - `name`: name of the module
    - `version`: version of the module in X.Y.Z format
    - `priority`: number to specify build priority, modules with lower number are built firstly. **1000** by default
    
  - Used when importing modules and checking version compatability

2. `compiler`
  - Provides information about compiler and binary output:
    - `binary`: type of output file (`executable`, `static-lib`, `shared-lib` exists)
    - `cc`: compiler which will be used to compile module
    - `objects_path`: path to store *.o files

3. `code`:
  - Provides information about all files in project:
    - `output`: path to binary file. When using with `...-lib`, no extra prefixes is required (`lib...`, `.a` etc.)
    - `sources`: list of sources to compile, supports globs
    - `includes`: list of include directories, supports globs
    - `libraries`: list of used libraries (`.a` or `.so`, no extension required)
    - `lib-dirs`: list of directories with arbitrary libraries (`-L` parameter)
    - `packages`: list of pkg-config packages (with *.ps files)

4. `targets`:
  - Here you can specify any number of targets, each target has:
    - `flags`: list of compiler flags which are used when this target is chosen
    - `defines`: list of key:value pairs (`[{"KEY": "VALUE"}, {...}, ...]`) => `-D KEY=VALUE ...`
    - `optimization`: optimization for this target (`O0`, `O1`, `O2`, `O3`, `Ofast`)
    - In `targets` key is a representation of target name which can be used in `target` block

5. `target`:
  - Specifies current target for compilation from `targets`

6. `defaults`:
  - Specifies file with default settings for compilation
  - Defaults in main module are used in all mods in `.modules`

7. `dependencies`:
  - Provides information about modules that are used
  - Each dependency is presented as:

```json
{
  "dependencies": {
    "dep-name": { "version": ">=X.Y.Z" },
    "dep-name2": { "version": "<=X.Y.Z" },
    "dep-name3": { "version": "=X.Y.Z" },
    ...
  }
}
```
  - Where `version` represents required versions
  - Remote dependencies are also supported as:

```json
{
  "dependencies": {
    "remote-dep-name": { 
      "version": ">=X.Y.Z",
      "url": "https://github/author/repo.git"
    },
}
```
