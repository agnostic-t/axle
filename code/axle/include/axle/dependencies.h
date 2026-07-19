#ifndef AXLE_DEPENDENCIES_H
#define AXLE_DEPENDENCIES_H

#include "axle/settings.h"
#include <stdbool.h>

/* ----- version helpers (unchanged) ----- */
int axle_dep_parsever(axle_dependency *dep, const char *version);
int axle_dep_check(const axle_dependency *dep, const char *version);

/* ----- remote dependency lifecycle -----
 *
 * All functions operate with respect to a `project_root`, which is the root
 * that owns the `.vendor/` directory. For the top-level project this is the
 * directory containing the user's `module.json`. For a remote_dep that itself
 * has sub-dependencies, `project_root` is `.vendor/<repo>/` (this is what
 * gives us isolation). */

/* `git clone` (or `git pull` if already present and `force_update`) the given
 * remote_dep into `<project_root>/.vendor/<repo_name>/`. */
int axle_dep_download(const char *url, const char *version_req,
                      const char *project_root, const char *repo_name,
                      bool force_update);

/* After downloading a repo, parse its `.as.module.json`, register every
 * exported module by creating a symlink `<project_root>/.modules/<name>`
 * -> `../.vendor/<repo_name>`, and update `<project_root>/.vendor/exported.json`.
 *
 * Version check: each exported module's declared version must satisfy the
 * `version_req` from `remote_deps`. On mismatch — fail.
 *
 * Conflict check: if an exported name is already registered to a DIFFERENT
 * repo in `exported.json` — fail (hard error, no overwrite).
 *
 * Returns 0 on success, -1 on failure. */
int axle_dep_install(const char *project_root, const char *repo_name,
                     const char *version_req);

/* Decide whether the remote_dep at `<project_root>/.vendor/<repo_name>/`
 * needs to be rebuilt. Returns:
 *   1  — yes, rebuild needed (either no .last_built, or HEAD != .last_built)
 *   0  — no, already built for the current HEAD
 *  -1  — error (not cloned, git unavailable, etc.) */
int axle_dep_needs_rebuild(const char *project_root, const char *repo_name);

/* Recursively build a remote_dep. This:
 *   1. Reads <vendor_path>/module.json.
 *   2. Recursively installs + builds any sub-`remote_deps` with base_path =
 *      <vendor_path> (isolation!).
 *   3. Resolves the repo's own local `dependencies` against
 *      <vendor_path>/.modules.
 *   4. Calls axle_build() for the repo itself with base_path = <vendor_path>.
 *   5. On success, writes the current HEAD SHA to <vendor_path>/.last_built.
 *
 * `inherited_defaults` — absolute path to a `defaults.json` inherited from
 * the calling project (the top-level project, or the parent remote_dep).
 * If the remote_dep itself has its own `defaults.json`, that one OVERRIDES
 * the inherited one (you can pass NULL if the caller has no defaults to
 * share, e.g. when there is no parent project). This is how sub-deps end
 * up using the compiler declared in their parent remote_dep's defaults.
 *
 * `depth` is used only for log indentation.
 *
 * Returns 0 on success, -1 on failure. */
int axle_dep_build(const char *project_root, const char *repo_name,
                   const char *inherited_defaults,
                   bool rebuild, bool silent, int depth);

#endif
