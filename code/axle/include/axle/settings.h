#ifndef AXLE_SETTINGS_H
#define AXLE_SETTINGS_H

#include "types.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct {
  const char *name;
  const char *version;
  int priority;
} axle_metadata;

typedef struct {
  axb_optilevel optimize;
  const char *flags;
  const char **defines;
} axle_target;

typedef struct {
  /* basicly paths in const char *, but in array:
   * const char **sources = {
   *      "./path/to/source.c",
   *      NULL // as an end of array
   * };
   */
  const char **sources;
  const char **lib_dirs;
  const char **incl_dirs;
  const char **libs;
  const char **pkgs; // libraries with .ps file
} axle_sources;

typedef struct {
  const char **pre_build;
  const char **post_build;
} axle_hooks;

typedef struct {
  axb_outtype type;
  const char *obj_path;
  const char *path;
} axle_output;

typedef struct {
  int major, middle, minor;
  const char *name;
} axle_dependency;

/* Remote dependency descriptor — comes from the `remote_deps` section of
 * module.json. `repo_name` is the key in `remote_deps` (and the folder name
 * under `.vendor/`). `url` is a git URL. `version_req` is a constraint string
 * like ">=1.0.0" or "=0.5.2". The actual version is read from the cloned
 * repo's `.as.module.json` (per exported module). */
typedef struct {
  char *repo_name;
  char *url;
  char *version_req;
} axle_remote_dep;

typedef struct {
  const char *compiler;
  const char *linker;
  axle_target target;
  axle_sources sources;
  axle_hooks hooks;
  axle_output output;

  axle_metadata metadata;

  bool only_deps;

  /* local dependencies — resolved against `<project_root>/.modules/` */
  axle_dependency *dependencies;
  size_t deps_n;

  /* remote dependencies — git-cloned into `<project_root>/.vendor/<repo>/` */
  axle_remote_dep *remote_deps;
  size_t remote_deps_n;
} axle_receipt;

#endif
