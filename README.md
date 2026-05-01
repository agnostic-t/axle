# AXLE build system

Lightweight build system, which supports:

- downloading dependencies over internet via `git`
- making modular projects with `.modules` folder
- configuration via JSON files
- default settings in JSON files

## Usage

All usage is making right modules structure and writing `module.json` and `defaults.json`

To build project that uses `axle`, just:

```sh
axle build /path/to/module/with/module_json
```

How to write `module.json` and make right structure for modules is written in `docs/usage.md`

## Building project

Tested on *x86_64 Linux (6.18.25_1)*

```sh
# install gcc beforehand

chmod +x ./scripts/build
./scripts/build
```

**Installing**

```sh
chmod +x ./scripts/install

# copies binary file to /usr/local/bin
sudo ./sciprts/install
```
