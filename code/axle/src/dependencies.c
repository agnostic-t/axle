#include "axle/dependencies.h"
#include "axle/as_module.h"
#include "axle/building.h"
#include "axle/cleanup.h"
#include "axle/colors.h"
#include "axle/exported.h"
#include "axle/git_utils.h"
#include "axle/path_utils.h"
#include "axle/project.h"
#include "axle/string_utils.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <yyjson.h>

/* ------------------------------------------------------------------ */
/* version helpers                                                     */
/* ------------------------------------------------------------------ */

static int axle_ver_cmp(int maj1, int mid1, int min1, int maj2, int mid2, int min2) {
    if (maj1 != maj2) return maj1 > maj2 ? 1 : -1;
    if (mid1 != mid2) return mid1 > mid2 ? 1 : -1;
    if (min1 != min2) return min1 > min2 ? 1 : -1;
    return 0;
}

int axle_dep_parsever(axle_dependency *dep, const char *version) {
    if (!dep || !version) return -1;

    const char *ptr = version;
    while (*ptr && (*ptr < '0' || *ptr > '9')) {
        ptr++;
    }

    int major = 0, middle = 0, minor = 0;
    if (3 != sscanf(ptr, "%d.%d.%d", &major, &middle, &minor)) {
        fprintf(stderr, "[axle][depman] failed to parse version number in `X.Y.Z` format: %s\n", version);
        return -1;
    }

    dep->major = major;
    dep->middle = middle;
    dep->minor = minor;
    return 0;
}

int axle_dep_check(const axle_dependency *dep, const char *version) {
    if (!dep || !version) return -1;

    int major, middle, minor;
    int cmp_result;

    if (strncmp(version, ">=", 2) == 0) {
        if (3 != sscanf(version, ">=%d.%d.%d", &major, &middle, &minor)) {
            fprintf(stderr, "[axle][depcheck] failed to parse version number (must be in `>=X.Y.Z` format)\n");
            return -1;
        }
        cmp_result = axle_ver_cmp(dep->major, dep->middle, dep->minor, major, middle, minor);
        if (cmp_result >= 0) return 0;
        fprintf(stderr, "[axle][depcheck] dependency `%s` is not in `%s` range: %d.%d.%d\n",
                dep->name, version, dep->major, dep->middle, dep->minor);
        return -1;
    } else if (strncmp(version, "<=", 2) == 0) {
        if (3 != sscanf(version, "<=%d.%d.%d", &major, &middle, &minor)) {
            fprintf(stderr, "[axle][depcheck] failed to parse version number (must be in `<=X.Y.Z` format)\n");
            return -1;
        }
        cmp_result = axle_ver_cmp(dep->major, dep->middle, dep->minor, major, middle, minor);
        if (cmp_result <= 0) return 0;
        fprintf(stderr, "[axle][depcheck] dependency `%s` is not in `%s` range: %d.%d.%d\n",
                dep->name, version, dep->major, dep->middle, dep->minor);
        return -1;
    } else if (strncmp(version, "=", 1) == 0) {
        if (3 != sscanf(version, "=%d.%d.%d", &major, &middle, &minor)) {
            fprintf(stderr, "[axle][depcheck] failed to parse version number (must be in `=X.Y.Z` format)\n");
            return -1;
        }
        cmp_result = axle_ver_cmp(dep->major, dep->middle, dep->minor, major, middle, minor);
        if (cmp_result == 0) return 0;
        fprintf(stderr, "[axle][depcheck] dependency `%s` is not in `%s` range: %d.%d.%d\n",
                dep->name, version, dep->major, dep->middle, dep->minor);
        return -1;
    } else {
        fprintf(stderr, "[axle][depcheck] failed to understand version requirements (%s), must be `>=`, `<=` or `=` before version number\n", version);
        return -1;
    }
}

/* ------------------------------------------------------------------ */
/* logging helper with depth-based indentation                         */
/* ------------------------------------------------------------------ */

static void axle_dep_log(int depth, const char *color, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    for (int i = 0; i < depth; i++) fputs("  ", stdout);
    if (color) fputs(color, stdout);
    fputs("[axle][depman] ", stdout);
    vprintf(fmt, ap);
    if (color) fputs(xfore.normal, stdout);
    fputc('\n', stdout);
    fflush(stdout);

    va_end(ap);
}

/* ------------------------------------------------------------------ */
/* download / clone                                                    */
/* ------------------------------------------------------------------ */

int axle_dep_download(const char *url, const char *version_req,
                      const char *project_root, const char *repo_name,
                      bool force_update) {
    if (!url || !project_root || !repo_name) return -1;
    (void)version_req;  /* not used at download time; checked at install */

    if (!axle_git_available()) {
        fprintf(stderr, "%s[axle][depman] `git` is not available on PATH. "
                "Cannot clone remote dependencies.%s\n",
                xfore.red, xfore.normal);
        return -1;
    }

    struct stat st;

    char *vendor_base = uax_path_concat(project_root, ".vendor");
    if (stat(vendor_base, &st) != 0) {
        if (mkdir(vendor_base, 0755) != 0) {
            fprintf(stderr, "[axle][depman] failed to create .vendor directory: %s\n", vendor_base);
            free(vendor_base);
            return -1;
        }
    }

    char *dep_path = uax_path_concat(vendor_base, repo_name);
    free(vendor_base);

    if (stat(dep_path, &st) != 0) {
        printf("[axle][depman] cloning remote dep '%s' -> %s\n", repo_name, url);
        if (0 != axle_git_clone(url, dep_path)) {
            fprintf(stderr, "%s[axle][depman] failed to clone %s%s\n",
                    xfore.red, url, xfore.normal);
            free(dep_path);
            return -1;
        }
    } else if (force_update) {
        printf("[axle][depman] updating remote dep '%s' (git pull)\n", repo_name);
        if (0 != axle_git_pull(dep_path)) {
            fprintf(stderr, "%s[axle][depman] warning: `git pull` failed for %s%s\n",
                    xfore.yellow, repo_name, xfore.normal);
            /* Non-fatal: continue with whatever we have locally. */
        }
    } else {
        printf("[axle][depman] remote dep '%s' is already downloaded\n", repo_name);
    }

    free(dep_path);
    return 0;
}

/* ------------------------------------------------------------------ */
/* install — register exported modules                                 */
/* ------------------------------------------------------------------ */

int axle_dep_install(const char *project_root, const char *repo_name,
                     const char *version_req) {
    if (!project_root || !repo_name) return -1;

    struct stat st;

    char *vendor_base = uax_path_concat(project_root, ".vendor");
    char *vendor_module_path = uax_path_concat(vendor_base, repo_name);
    free(vendor_base);

    if (stat(vendor_module_path, &st) != 0) {
        fprintf(stderr, "%s[axle][depman] vendor module not found: %s%s\n",
                xfore.red, vendor_module_path, xfore.normal);
        free(vendor_module_path);
        return -1;
    }

    /* Parse .as.module.json from the vendor module. */
    axle_as_module as;
    if (0 > axle_as_module_parse(vendor_module_path, &as)) {
        fprintf(stderr, "%s[axle][depman] failed to parse .as.module.json for %s%s\n",
                xfore.red, repo_name, xfore.normal);
        free(vendor_module_path);
        return -1;
    }

    if (!as.valid || as.n == 0) {
        fprintf(stderr, "%s[axle][depman] %s/.as.module.json does not export any modules.%s\n",
                xfore.red, repo_name, xfore.normal);
        free(vendor_module_path);
        axle_as_module_free(&as);
        return -1;
    }

    if (!as.include_tests) {
        printf("[axle][depman] note: repo '%s' requests tests excluded\n", repo_name);
    }

    /* Make sure .modules/ exists. */
    char *modules_base = uax_path_concat(project_root, ".modules");
    if (stat(modules_base, &st) != 0) {
        mkdir(modules_base, 0755);
    }

    /* Load existing exported.json. */
    axle_exported_map exported;
    if (0 > axle_exported_load(project_root, &exported)) {
        fprintf(stderr, "%s[axle][depman] failed to load exported.json%s\n",
                xfore.red, xfore.normal);
        free(modules_base);
        free(vendor_module_path);
        axle_as_module_free(&as);
        return -1;
    }

    int failed = 0;
    for (size_t i = 0; i < as.n; i++) {
        const char *export_name = as.entries[i].name;
        const char *export_ver = as.entries[i].version;

        /* ----- version check ----- */
        if (export_ver && version_req) {
            axle_dependency dep = {0};
            dep.name = (char *)export_name;
            if (0 > axle_dep_parsever(&dep, export_ver)) {
                fprintf(stderr, "%s[axle][depman] failed to parse exported version '%s' for '%s'%s\n",
                        xfore.red, export_ver, export_name, xfore.normal);
                failed = 1;
                break;
            }
            if (0 > axle_dep_check(&dep, version_req)) {
                fprintf(stderr, "%s[axle][depman] version check failed for exported module '%s' (declared %s, required %s)%s\n",
                        xfore.red, export_name, export_ver, version_req, xfore.normal);
                failed = 1;
                break;
            }
        }

        /* ----- conflict check ----- */
        char *existing_repo = NULL;
        if (0 == axle_exported_find_repo(project_root, export_name, &existing_repo)) {
            if (strcmp(existing_repo, repo_name) != 0) {
                fprintf(stderr,
                        "%s[axle][depman] CONFLICT: exported name '%s' is already "
                        "registered to repo '%s', cannot register it for '%s'.%s\n",
                        xfore.red, export_name, existing_repo, repo_name, xfore.normal);
                free(existing_repo);
                failed = 1;
                break;
            }
            free(existing_repo);
            /* Same repo re-installing — fine, just refresh. */
        }

        /* ----- symlink ----- */
        char *modules_target = uax_path_concat(modules_base, export_name);

        char rel_target[512];
        if (as.entries[i].internal_module && strcmp(as.entries[i].internal_module, ".") != 0) {
            snprintf(rel_target, sizeof(rel_target), "../.vendor/%s/.modules/%s", repo_name, as.entries[i].internal_module);
        } else {
            snprintf(rel_target, sizeof(rel_target), "../.vendor/%s", repo_name);
        }

        /* If something already exists at modules_target, check that it's
            * either our symlink or absent — otherwise refuse. */
        if (lstat(modules_target, &st) == 0) {
            /* Check that it's a symlink pointing at the right place. */
            char buf[512];
            ssize_t n = readlink(modules_target, buf, sizeof(buf) - 1);
            if (n < 0) {
                fprintf(stderr, "%s[axle][depman] %s already exists and is not a symlink; refusing to overwrite.%s\n",
                        xfore.red, modules_target, xfore.normal);
                free(modules_target);
                failed = 1;
                break;
            }
            buf[n] = '\0';
            if (strcmp(buf, rel_target) != 0) {
                printf("[axle][depman] updating symlink %s -> %s\n", modules_target, rel_target);
                unlink(modules_target);
                if (symlink(rel_target, modules_target) != 0) {
                    fprintf(stderr, "%s[axle][depman] failed to update symlink %s -> %s%s\n",
                            xfore.red, modules_target, rel_target, xfore.normal);
                    free(modules_target);
                    failed = 1;
                    break;
                }
            }
            /* Already correctly linked — nothing to do. */
        } else {
            if (symlink(rel_target, modules_target) != 0) {
                fprintf(stderr, "%s[axle][depman] failed to create symlink %s -> %s%s\n",
                        xfore.red, modules_target, rel_target, xfore.normal);
                free(modules_target);
                failed = 1;
                break;
            }
            printf("[axle][depman] linked '%s' -> '%s'\n", modules_target, rel_target);
        }

        free(modules_target);

        /* Update in-memory map. */
        if (0 > axle_exported_map_set(&exported, export_name, repo_name)) {
            fprintf(stderr, "%s[axle][depman] failed to update exported map for '%s'%s\n",
                    xfore.red, export_name, xfore.normal);
            failed = 1;
            break;
        }

        printf("[axle][depman] registered export '%s' (v%s) from repo '%s'\n",
               export_name, export_ver ? export_ver : "?", repo_name);
    }

    /* Persist exported.json if everything succeeded. */
    if (!failed) {
        if (0 > axle_exported_save(project_root, &exported)) {
            fprintf(stderr, "%s[axle][depman] failed to save exported.json%s\n",
                    xfore.red, xfore.normal);
            failed = 1;
        }
    }

    axle_exported_map_free(&exported);
    free(modules_base);
    free(vendor_module_path);
    axle_as_module_free(&as);

    return failed ? -1 : 0;
}

/* ------------------------------------------------------------------ */
/* needs_rebuild                                                       */
/* ------------------------------------------------------------------ */

int axle_dep_needs_rebuild(const char *project_root, const char *repo_name) {
    if (!project_root || !repo_name) return -1;

    char *vendor_base = uax_path_concat(project_root, ".vendor");
    char *vendor_path = uax_path_concat(vendor_base, repo_name);
    free(vendor_base);

    struct stat st;
    if (stat(vendor_path, &st) != 0) {
        fprintf(stderr, "%s[axle][depman] vendor path not found: %s%s\n",
                xfore.red, vendor_path, xfore.normal);
        free(vendor_path);
        return -1;
    }

    char current_sha[64];
    if (0 > axle_git_head_sha(vendor_path, current_sha)) {
        fprintf(stderr, "%s[axle][depman] failed to get HEAD sha for %s%s\n",
                xfore.red, repo_name, xfore.normal);
        free(vendor_path);
        return -1;
    }

    char *last_built_path = uax_path_concat(vendor_path, ".last_built");
    FILE *fp = fopen(last_built_path, "r");
    free(vendor_path);
    free(last_built_path);

    if (!fp) {
        /* No .last_built — need to build. */
        return 1;
    }

    char saved_sha[64] = {0};
    if (!fgets(saved_sha, sizeof(saved_sha), fp)) {
        fclose(fp);
        return 1;
    }
    fclose(fp);

    /* Trim newline. */
    size_t len = strlen(saved_sha);
    while (len > 0 && (saved_sha[len - 1] == '\n' || saved_sha[len - 1] == '\r')) {
        saved_sha[--len] = '\0';
    }

    if (strcmp(saved_sha, current_sha) != 0) {
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* build — recursive                                                   */
/* ------------------------------------------------------------------ */

/* Write the current HEAD SHA of <vendor_path> into <vendor_path>/.last_built. */
static int write_last_built(const char *vendor_path) {
    char sha[64];
    if (0 > axle_git_head_sha(vendor_path, sha)) return -1;

    char *path = uax_path_concat(vendor_path, ".last_built");
    if (!path) return -1;

    FILE *fp = fopen(path, "w");
    free(path);
    if (!fp) return -1;
    fputs(sha, fp);
    fputc('\n', fp);
    fclose(fp);
    return 0;
}

int axle_dep_build(const char *project_root, const char *repo_name,
                   const char *inherited_defaults,
                   bool rebuild, bool silent, int depth) {
    if (!project_root || !repo_name) return -1;

    int result = -1;

    char *vendor_base = uax_path_concat(project_root, ".vendor");
    char *vendor_path_raw = uax_path_concat(vendor_base, repo_name);
    free(vendor_base);
    if (!vendor_path_raw) return -1;

    /* Resolve to an absolute path. We will chdir() into it before invoking
     * axle_build(), because axle_compile_sources() relies on relative paths
     * being resolvable from CWD. Using an absolute vendor_path for chdir
     * means we can return to the original CWD afterwards without ambiguity. */
    char *vendor_path = realpath(vendor_path_raw, NULL);
    free(vendor_path_raw);
    if (!vendor_path) {
        fprintf(stderr, "%s[axle][depman] realpath failed for %s/%s%s\n",
                xfore.red, project_root, repo_name, xfore.normal);
        return -1;
    }

    /* Save the original CWD so we can restore it on return. */
    char *orig_cwd = getcwd(NULL, 0);
    if (!orig_cwd) {
        free(vendor_path);
        return -1;
    }

    axle_dep_log(depth, xfore.cyan, "building remote_dep '%s' (base=%s)", repo_name, vendor_path);

    /* 1. Check if we can skip the build. */
    if (!rebuild) {
        int need = axle_dep_needs_rebuild(project_root, repo_name);
        if (need == 0) {
            char sha[64];
            if (0 == axle_git_head_sha(vendor_path, sha)) {
                axle_dep_log(depth, xfore.green, "up to date (sha %.8s)", sha);
            } else {
                axle_dep_log(depth, xfore.green, "up to date");
            }
            result = 0;
            goto cleanup_cwd;
        } else if (need < 0) {
            goto cleanup_cwd;
        }
    }

    char current_sha[64];
    if (0 == axle_git_head_sha(vendor_path, current_sha)) {
        axle_dep_log(depth, xfore.magenta, "rebuilding for sha %.8s", current_sha);
    }

    /* 2. Load the repo's own module.json. This is where we discover sub-deps
     *    (both `remote_deps` and local `dependencies`).
     *
     *    Defaults resolution precedence (highest to lowest):
     *      (a) `<vendor_path>/defaults.json`         (this repo's own)
     *      (b) `inherited_defaults`                  (passed by the caller —
     *          either the top-level project's defaults, or the parent
     *          remote_dep's effective defaults)
     *
     *    If (a) exists it wins; otherwise we use (b). This way, a sub-dep
     *    whose own defaults.json is missing still inherits the compiler /
     *    linker / etc. from its parent remote_dep. */
    char *mod_json = uax_path_concat(vendor_path, "module.json");
    if (!mod_json) goto cleanup_cwd;

    char *own_defaults = uax_path_concat(vendor_path, "defaults.json");
    {
        struct stat dst;
        if (stat(own_defaults, &dst) != 0) {
            free(own_defaults);
            own_defaults = NULL;
        } else {
            axle_dep_log(depth, xfore.gray, "using defaults: %s", own_defaults);
        }
    }
    const char *effective_defaults = own_defaults ? own_defaults : inherited_defaults;
    /* `vendor_defaults` is the pointer we own and must free at the end; if
     * we ended up using the inherited_defaults, vendor_defaults is NULL. */
    char *vendor_defaults = own_defaults;

    axle_receipt recp;
    memset(&recp, 0, sizeof(recp));
    if (0 > axle_project_prepare(mod_json, effective_defaults, vendor_path, &recp)) {
        fprintf(stderr, "%s[axle][depman] failed to prepare receipt for %s%s\n",
                xfore.red, mod_json, xfore.normal);
        free(mod_json);
        if (vendor_defaults) free(vendor_defaults);
        goto cleanup_cwd;
    }
    free(mod_json);

    /* 3. Recursively install + build sub-remote_deps. base_path = vendor_path,
     *    which gives us isolation (sub-deps clone into vendor_path/.vendor/).
     *
     *    Pass `effective_defaults` as the inherited defaults for sub-deps so
     *    that they too can use the compiler / linker declared here (or above
     *    us in the chain). */
    for (size_t i = 0; i < recp.remote_deps_n; i++) {
        const axle_remote_dep *rd = &recp.remote_deps[i];
        axle_dep_log(depth + 1, xfore.cyan, "sub-remote_dep '%s' (%s)", rd->repo_name, rd->url);

        if (0 > axle_dep_download(rd->url, rd->version_req,
                                  vendor_path, rd->repo_name, false)) {
            fprintf(stderr, "%s[axle][depman] failed to download sub-dep %s%s\n",
                    xfore.red, rd->repo_name, xfore.normal);
            goto cleanup_recp;
        }
        if (0 > axle_dep_install(vendor_path, rd->repo_name, rd->version_req)) {
            fprintf(stderr, "%s[axle][depman] failed to install sub-dep %s%s\n",
                    xfore.red, rd->repo_name, xfore.normal);
            goto cleanup_recp;
        }
        if (0 > axle_dep_build(vendor_path, rd->repo_name,
                               effective_defaults,
                               rebuild, silent, depth + 1)) {
            fprintf(stderr, "%s[axle][depman] failed to build sub-dep %s%s\n",
                    xfore.red, rd->repo_name, xfore.normal);
            goto cleanup_recp;
        }
    }

    /* 4. Resolve this repo's local dependencies (against vendor_path/.modules)
     *    and build them.
     *
     *    Pass `effective_defaults` as `main_defaults` so that every sub-module
     *    inherits the repo's defaults.json (compiler, linker, optimization
     *    target, ...) — exactly how the top-level project does it in main.c. */
    axle_receipt *mod_receipts = NULL;
    size_t mods_n = 0;
    if (0 > axle_modules_prepare(effective_defaults, &recp, vendor_path,
                                 &mod_receipts, &mods_n)) {
        fprintf(stderr, "%s[axle][depman] failed to prepare sub-modules for %s%s\n",
                xfore.red, repo_name, xfore.normal);
        goto cleanup_recp;
    }

    /* chdir into vendor_path so that the existing axle_compile_sources logic
     * (which does stat() on relative paths from CWD) works correctly. */
    if (0 != chdir(vendor_path)) {
        fprintf(stderr, "%s[axle][depman] failed to chdir into %s%s\n",
                xfore.red, vendor_path, xfore.normal);
        for (size_t i = 0; i < mods_n; i++) clean_axle_receipt(&mod_receipts[i]);
        free(mod_receipts);
        goto cleanup_recp;
    }

    {
        int success = 1;
        char *mods_base = uax_path_concat(vendor_path, ".modules");
        for (size_t i = 0; i < mods_n; i++) {
            char *mod_base_path = uax_path_concat(mods_base, mod_receipts[i].metadata.name);

            /* If this module entry is a symlink, it points into our own
             * `.vendor/` — i.e. it's a sub-remote_dep we just built above.
             * Skip the redundant rebuild; `axle_modules_prepare` has already
             * merged its incl_dirs / lib_dirs / libs into our receipt. */
            struct stat lst;
            int is_symlink = (lstat(mod_base_path, &lst) == 0 &&
                              S_ISLNK(lst.st_mode));

            axle_dep_log(depth + 1, xfore.cyan, "[%zu/%zu] building sub-dep: %s",
                         i + 1, mods_n, mod_receipts[i].metadata.name);
            if (is_symlink) {
                axle_dep_log(depth + 1, xfore.magenta,
                             "skipping (already built as sub-remote_dep)");
                free(mod_base_path);
                continue;
            }

            if (0 > axle_build(&mod_receipts[i], mod_base_path, true, rebuild, silent)) {
                fprintf(stderr, "%s[axle][depman] sub-dep build failed: %s%s\n",
                        xfore.red, mod_receipts[i].metadata.name, xfore.normal);
                success = 0;
                free(mod_base_path);
                break;
            }
            free(mod_base_path);
        }
        free(mods_base);

        /* 5. Build the repo itself. */
        if (success && !recp.only_deps) {
            if (0 > axle_build(&recp, vendor_path, true, rebuild, silent)) {
                fprintf(stderr, "%s[axle][depman] build failed for repo '%s'%s\n",
                        xfore.red, repo_name, xfore.normal);
                success = 0;
            }
        }

        if (success) {
            /* 6. Persist .last_built. */
            if (0 > write_last_built(vendor_path)) {
                fprintf(stderr, "%s[axle][depman] warning: failed to write .last_built for %s%s\n",
                        xfore.yellow, repo_name, xfore.normal);
            }
            axle_dep_log(depth, xfore.green, "done '%s'", repo_name);
            result = 0;
        }

        for (size_t i = 0; i < mods_n; i++) clean_axle_receipt(&mod_receipts[i]);
        free(mod_receipts);
    }

    /* Restore CWD regardless of outcome. */
    if (0 != chdir(orig_cwd)) {
        /* If we cannot return to the original CWD, that's a serious problem
         * for the caller. Best-effort: leave it as-is but log. */
        fprintf(stderr, "%s[axle][depman] warning: failed to restore CWD to %s%s\n",
                xfore.yellow, orig_cwd, xfore.normal);
    }

cleanup_recp:
    clean_axle_receipt(&recp);
    if (vendor_defaults) free(vendor_defaults);

cleanup_cwd:
    free(orig_cwd);
    free(vendor_path);
    return result;
}
