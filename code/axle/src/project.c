#include "axle/project.h"

#include "axle/cleanup.h"
#include "axle/colors.h"
#include "axle/exported.h"
#include "axle/jsonload.h"
#include "axle/path_utils.h"
#include "axle/settings.h"
#include "axle/string_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <yyjson.h>

static int axle_path_is_absolute(const char *path) {
    if (!path || !*path)
        return 0;

    if (path[0] == '/')
        return 1;

    if (path[0] && path[1] == ':')
        return 1;

    return 0;
}

static int axle_merge_paths(
    const char *module_base,
    const char **paths,
    const char ***destination
) {
    if (!paths)
        return 0;

    for (size_t i = 0; paths[i] != NULL; i++) {
        char *rebased = NULL;

        if (axle_path_is_absolute(paths[i])) {
            rebased = strdup(paths[i]);
        } else {
            rebased = uax_path_concat(
                module_base,
                paths[i]
            );
        }

        if (!rebased)
            return -1;

        if (uax_strlist_extend_ne(
                (char ***)destination,
                rebased
            ) < 0) {
            free(rebased);
            return -1;
        }

        free(rebased);
    }

    return 0;
}

static int axle_merge_strings(
    const char **values,
    const char ***destination
) {
    if (!values)
        return 0;

    for (size_t i = 0; values[i] != NULL; i++) {
        if (uax_strlist_extend_ne(
                (char ***)destination,
                values[i]
            ) < 0) {
            return -1;
        }
    }

    return 0;
}

static int axle_receipt_merge(
    axle_receipt *into,
    axle_receipt *from,
    const char *module_name,
    const char *base_dir
) {
    if (!into || !from || !module_name || !base_dir)
        return -1;

    char *base_path =
        uax_path_concat(base_dir, module_name);

    if (!base_path)
        return -1;

    /*
     * Public include interface.
     */
    if (from->sources.incl_dirs) {
        if (axle_merge_paths(
                base_path,
                from->sources.incl_dirs,
                &into->sources.incl_dirs
            ) < 0) {
            free(base_path);
            return -1;
        }
    } else {
        char *include_path =
            uax_path_concat(base_path, "include");

        if (!include_path) {
            free(base_path);
            return -1;
        }

        if (uax_strlist_extend_ne(
                (char ***)&into->sources.incl_dirs,
                include_path
            ) < 0) {
            free(include_path);
            free(base_path);
            return -1;
        }

        free(include_path);
    }

    if (axle_merge_paths(
            base_path,
            from->sources.lib_dirs,
            &into->sources.lib_dirs
        ) < 0) {
        free(base_path);
        return -1;
    }

    if (from->output.path &&
        (from->output.type == STATIC_LIBRARY ||
         from->output.type == DYN_LIBRARY)) {

        char *output_dir =
            uax_path_get_dir(from->output.path);

        if (!output_dir) {
            free(base_path);
            return -1;
        }

        char *rebased_output_dir =
            uax_path_concat(base_path, output_dir);

        free(output_dir);

        if (!rebased_output_dir) {
            free(base_path);
            return -1;
        }

        if (uax_strlist_extend_ne(
                (char ***)&into->sources.lib_dirs,
                rebased_output_dir
            ) < 0) {
            free(rebased_output_dir);
            free(base_path);
            return -1;
        }

        free(rebased_output_dir);

        const char *filename =
            uax_path_filename(from->output.path);

        if (filename) {
            const char *link_name = filename;

            if (strncmp(link_name, "lib", 3) == 0)
                link_name += 3;

            if (uax_strlist_extend_ne(
                    (char ***)&into->sources.libs,
                    link_name
                ) < 0) {
                free(base_path);
                return -1;
            }
        }
    }

    if (axle_merge_strings(
            from->sources.libs,
            &into->sources.libs
        ) < 0) {
        free(base_path);
        return -1;
    }

    /*
     * Public pkg-config deps.
     */
    if (axle_merge_strings(
            from->sources.pkgs,
            &into->sources.pkgs
        ) < 0) {
        free(base_path);
        return -1;
    }

    free(base_path);
    return 0;
}

int axle_project_prepare(const char *directory, const char *adt_defaults, const char *project_root, axle_receipt *outrecp){
    char *dir_path = uax_path_get_dir(directory);

    yyjson_read_err err;
    yyjson_doc *doc = yyjson_read_file(directory, 0, NULL, &err);

    if (!doc) {
        fprintf(stderr, "[axle] read error (%u): %s at position: %ld\n", err.code, err.msg, err.pos);
        return -1;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);

    memset(outrecp, 0, sizeof(axle_receipt));

    const char *defaults_file = yyjson_get_str(yyjson_obj_get(root, "defaults"));
    const char *target_name = yyjson_get_str(yyjson_obj_get(root, "target"));
    bool is_only_deps = yyjson_get_bool(yyjson_obj_get(root, "only-deps"));

    if (defaults_file) {
        char *def_path = uax_path_concat(dir_path, defaults_file);
        axle_load_settings(outrecp, def_path, target_name, project_root);
        free(def_path);
    }

    if (adt_defaults){
        axle_load_settings(outrecp, adt_defaults, target_name, project_root);
    }

    axle_load_settings(outrecp, directory, NULL, project_root);
    outrecp->only_deps = is_only_deps;
    yyjson_doc_free(doc);

    free(dir_path);
    return 0;
}

typedef struct {
    char **libs;
    char **lib_dirs;
    char **pkgs;
} orig_ext_libs_t;

static orig_ext_libs_t extract_orig_ext_libs(axle_receipt *recp) {
    orig_ext_libs_t orig;
    orig.libs = (char **)recp->sources.libs;
    orig.lib_dirs = (char **)recp->sources.lib_dirs;
    orig.pkgs = (char **)recp->sources.pkgs;

    recp->sources.libs = NULL;
    recp->sources.lib_dirs = NULL;
    recp->sources.pkgs = NULL;

    return orig;
}

static void restore_orig_ext_libs(axle_receipt *recp, orig_ext_libs_t orig) {
    if (orig.lib_dirs) {
        for (size_t k = 0; orig.lib_dirs[k] != NULL; k++) {
            uax_strlist_extend_ne((char***)&recp->sources.lib_dirs, orig.lib_dirs[k]);
        }
        uax_free_strlist_ne(&orig.lib_dirs);
    }
    if (orig.libs) {
        for (size_t k = 0; orig.libs[k] != NULL; k++) {
            uax_strlist_extend_ne((char***)&recp->sources.libs, orig.libs[k]);
        }
        uax_free_strlist_ne(&orig.libs);
    }
    if (orig.pkgs) {
        for (size_t k = 0; orig.pkgs[k] != NULL; k++) {
            uax_strlist_extend_ne((char***)&recp->sources.pkgs, orig.pkgs[k]);
        }
        uax_free_strlist_ne(&orig.pkgs);
    }
}

typedef struct {
    char *name;
    axle_receipt recp;
    int original_idx;
    /* is_subvendor_stub: this entry is NOT a real .modules/<name> directory
     * but a stub we synthesised for a transitive dep that lives inside one
     * of our remote_deps' isolated .vendor/. In this case we must NOT try
     * to build it (it's already built); we only merge its lib_dirs / libs
     * into the parent receipt. */
    int is_subvendor_stub;
    char *subvendor_modules_dir;  /* e.g. ".vendor/<repo>/.modules" */
} axle_transitive_mod;

static int cmp_transitive_mods(const void *a, const void *b) {
    axle_transitive_mod *modA = (axle_transitive_mod *)a;
    axle_transitive_mod *modB = (axle_transitive_mod *)b;

    int prioA = modA->recp.metadata.priority != 0 ? modA->recp.metadata.priority : 1000;
    int prioB = modB->recp.metadata.priority != 0 ? modB->recp.metadata.priority : 1000;

    if (prioA != prioB) return prioA - prioB;

    return modB->original_idx - modA->original_idx;
}

int axle_modules_prepare(const char *main_defaults, axle_receipt *main_receipt, const char *directory, axle_receipt **out_receipts, size_t *recp_n){
    if (!directory || !out_receipts || !recp_n) return -1;

    *out_receipts = NULL;
    *recp_n = 0;

    char *modules_path = uax_path_concat(directory, ".modules");

    char **queue = NULL;
    size_t queue_n = 0;

    for (size_t i = 0; i < main_receipt->deps_n; i++) {
        uax_strlist_extend(&queue, &queue_n, main_receipt->dependencies[i].name);
    }

    axle_transitive_mod *loaded_mods = NULL;
    size_t loaded_n = 0;

    size_t head = 0;
    while (head < queue_n) {
        char *mod_name = queue[head++];

        int processed = 0;
        for (size_t i = 0; i < loaded_n; i++) {
            if (strcmp(loaded_mods[i].name, mod_name) == 0) {
                processed = 1;
                break;
            }
        }
        if (processed) continue;

        axle_receipt recp;
        char *module_path_base = uax_path_concat(modules_path, mod_name);
        char *module_path = uax_path_concat(module_path_base, "module.json");

        struct stat mod_stat;

        /* If `.modules/<mod_name>` is a symlink, it points into `.vendor/`
         * — i.e. this is a remote_dep we already installed and built via
         * `axle_dep_build` in main.c. We must NOT try to rebuild it through
         * the local-modules path. We still want its lib_dirs / libs /
         * incl_dirs to be merged into the main receipt (so the final link
         * step can find its .a / .so), which is exactly what
         * `axle_receipt_merge` does for every loaded_mod below. So instead
         * of skipping it entirely, we let `axle_project_prepare` load its
         * receipt, but we mark it with `is_subvendor_stub=1` so the caller
         * (main.c) doesn't try to `axle_build` it again. */
        int is_symlink_remote_dep = 0;
        if (lstat(module_path_base, &mod_stat) == 0 &&
            S_ISLNK(mod_stat.st_mode)) {
            is_symlink_remote_dep = 1;
        }

        if (stat(module_path, &mod_stat) != 0) {
            /* Module file is not present at <project_root>/.modules/<name>/module.json.
             * This is expected when the module is a transitive dependency of a
             * remote_dep: it lives in the remote_dep's own .vendor/.modules/
             * (isolation), not in ours. We can detect this by looking the name
             * up in our own .vendor/exported.json — if it's listed, the module
             * is an export of some remote_dep we already installed and built,
             * and we should just skip it here (its artefacts are already
             * linked in via the remote_dep's receipt merge). */
            char *owning_repo = NULL;
            int found_exported = axle_exported_find_repo(directory, mod_name, &owning_repo);
            if (found_exported == 0) {
                printf("%s[axle][mod] %sskipping%s transitive dep '%s' — it's an export of remote_dep '%s' (built in its own isolated .vendor)\n",
                       xfore.cyan, xfore.magenta, xfore.normal, mod_name, owning_repo);
                free(owning_repo);
                free(module_path_base);
                free(module_path);
                continue;
            }
            if (owning_repo) free(owning_repo);

            /* Also check whether this module name appears as an export of any
             * sub-remote_dep (i.e. it lives inside .vendor/<repo>/.modules/
             * or .vendor/<repo>/.vendor/exported.json). This handles the
             * common case where the direct remote_dep we installed depends
             * on a sub-module that it itself exports — without this, our
             * transitive resolver would try to load that sub-module from our
             * own .modules/ and fail.
             *
             * When we find such a sub-vendor module, we *do not* try to
             * rebuild it (it's already built in its isolated .vendor), but
             * we DO merge its lib_dirs / libs into the main receipt, so the
             * final link step can find the .a / .so file the sub-module
             * produces. Without this, the link would fail with "undefined
             * reference" for any symbol the sub-module exports. */
            int found_in_subvendor = 0;
            char *subvendor_repo = NULL;
            {
                char *vendor_dir = uax_path_concat(directory, ".vendor");
                DIR *vd = opendir(vendor_dir);
                if (vd) {
                    struct dirent *de;
                    while ((de = readdir(vd)) != NULL) {
                        if (de->d_name[0] == '.') continue;
                        char *sub_vendor_path = uax_path_concat(vendor_dir, de->d_name);
                        struct stat sst;
                        if (stat(sub_vendor_path, &sst) != 0 || !S_ISDIR(sst.st_mode)) {
                            free(sub_vendor_path);
                            continue;
                        }
                        char *sub_modules = uax_path_concat(sub_vendor_path, ".modules");
                        char *sub_mod = uax_path_concat(sub_modules, mod_name);
                        char *sub_mod_json = uax_path_concat(sub_mod, "module.json");
                        if (stat(sub_mod_json, &sst) == 0) {
                            // printf("%s[axle][mod] %sskipping%s transitive dep '%s' — it lives inside remote_dep '%s' (isolated sub-dep; will still link its artefacts)\n",
                                   // xfore.cyan, xfore.magenta, xfore.normal,
                                   // mod_name, de->d_name);
                            found_in_subvendor = 1;
                            subvendor_repo = strdup(de->d_name);
                        }
                        free(sub_mod_json);
                        free(sub_mod);
                        free(sub_modules);
                        if (found_in_subvendor) {
                            free(sub_vendor_path);
                            break;
                        }
                        free(sub_vendor_path);
                    }
                    closedir(vd);
                }
                free(vendor_dir);
            }
            if (found_in_subvendor) {
                /* Build a tiny "stub" receipt for the sub-vendor module so
                 * that the existing axle_receipt_merge logic below picks up
                 * its lib_dirs / libs. We use the module.json that lives
                 * inside .vendor/<repo>/.modules/<mod_name>/module.json —
                 * which is actually a symlink back to the real
                 * .vendor/<repo>/.vendor/<subrepo>/module.json — and read
                 * just enough from it to construct a receipt with an output
                 * path.
                 *
                 * We also pass the sub-vendor repo's defaults.json (if any)
                 * so the stub inherits the same compiler / linker / etc. as
                 * its siblings. */
                char *vendor_dir_tmp = uax_path_concat(directory, ".vendor");
                char *sub_vendor_path = uax_path_concat(vendor_dir_tmp, subvendor_repo);
                free(vendor_dir_tmp);
                char *sub_modules_dir = uax_path_concat(sub_vendor_path, ".modules");
                char *sub_mod_dir     = uax_path_concat(sub_modules_dir, mod_name);
                char *sub_mod_json    = uax_path_concat(sub_mod_dir, "module.json");

                /* Probe for the sub-vendor repo's own defaults.json so the
                 * stub inherits its compiler / linker / etc. */
                char *sub_vendor_defaults = uax_path_concat(sub_vendor_path, "defaults.json");
                struct stat def_st;
                if (stat(sub_vendor_defaults, &def_st) != 0) {
                    free(sub_vendor_defaults);
                    sub_vendor_defaults = NULL;
                }

                axle_receipt stub;
                memset(&stub, 0, sizeof(stub));
                if (0 == axle_project_prepare(sub_mod_json, sub_vendor_defaults,
                                              sub_vendor_path, &stub)) {
                    /* Push the stub into loaded_mods so the merge pass below
                     * (which iterates loaded_mods[]) picks it up. The base
                     * dir for merge is ".vendor/<repo>/.modules" so that
                     * paths resolve correctly. */
                    axle_transitive_mod *tmp = realloc(loaded_mods,
                                                       sizeof(axle_transitive_mod) * (loaded_n + 1));
                    if (tmp) {
                        loaded_mods = tmp;
                        char rel_subvendor[512];
                        snprintf(rel_subvendor, sizeof(rel_subvendor), ".vendor/%s/.modules", subvendor_repo);

                        loaded_mods[loaded_n].name = strdup(mod_name);
                        loaded_mods[loaded_n].recp = stub;
                        loaded_mods[loaded_n].original_idx = loaded_n;
                        loaded_mods[loaded_n].is_subvendor_stub = 1;
                        loaded_mods[loaded_n].subvendor_modules_dir = strdup(rel_subvendor);
                        loaded_n++;

                        /* Walk the stub's own dependencies so transitiv
                         * sub-deps (e.g. `buffer`, `command`, ... that the
                         * stub's module.json lists as `dependencies`) get
                         * pulled into the queue and resolved in turn. Without
                         * this walk, only the first level of sub-vendor deps
                         * would be merged into the main receipt — their own
                         * sub-deps' include dirs / lib dirs would be missing,
                         * causing `#include <buffer/buffer.hpp>` to fail in
                         * the main project. */
                        for (size_t i = 0; i < stub.deps_n; i++) {
                            uax_strlist_extend(&queue, &queue_n,
                                               stub.dependencies[i].name);
                        }
                    } else {
                        clean_axle_receipt(&stub);
                    }
                }

                if (sub_vendor_defaults) free(sub_vendor_defaults);
                free(sub_mod_json);
                free(sub_mod_dir);
                free(sub_modules_dir);
                free(sub_vendor_path);
                free(subvendor_repo);
                free(module_path_base);
                free(module_path);
                continue;
            }

            fprintf(stderr, "%s[axle][mod] failed to find module.json%s for transitive dep '%s' at %s\n",
                    xfore.red, xfore.normal, mod_name, module_path);
            free(module_path_base);
            free(module_path);
            goto fail;
        }
        free(module_path_base);

        if (0 > axle_project_prepare(module_path, main_defaults, directory, &recp)){
            fprintf(stderr, "%s[axle][mod] failed to prepare receipt%s for module %s\n", xfore.red, xfore.normal, mod_name);
            free(module_path);
            goto fail;
        }
        free(module_path);

        /* If this is a symlinked remote_dep, we still want to walk its
         * transitiv dependencies (so they get merged into the main receipt
         * too), but we must NOT allow it to be rebuilt by the caller. Mark
         * it as a stub — same mechanism used for sub-vendor isolated deps. */
        if (is_symlink_remote_dep) {
            printf("%s[axle][mod] %sskipping build of%s '%s' — it's a symlinked remote_dep (already built by axle_dep_build)\n",
                   xfore.cyan, xfore.magenta, xfore.normal, mod_name);
        }

        for (size_t i = 0; i < recp.deps_n; i++) {
            uax_strlist_extend(&queue, &queue_n, recp.dependencies[i].name);
        }

        axle_transitive_mod *tmp = realloc(loaded_mods, sizeof(axle_transitive_mod) * (loaded_n + 1));
        if (!tmp) {
            fprintf(stderr, "%s[axle][mod] failed to allocate memory%s\n", xfore.red, xfore.normal);
            goto fail;
        }
        loaded_mods = tmp;
        loaded_mods[loaded_n].name = strdup(mod_name);
        loaded_mods[loaded_n].recp = recp;
        loaded_mods[loaded_n].original_idx = loaded_n;
        if (is_symlink_remote_dep) {
            /* Mark as stub so caller won't try to rebuild it, but use the
             * normal ".modules" base_dir for the merge pass — the symlink
             * really does live at `.modules/<name>` and the underlying repo
             * produces its artefacts in `<vendor_path>/bin` which is the
             * same path the symlink resolves to. */
            loaded_mods[loaded_n].is_subvendor_stub = 1;
            loaded_mods[loaded_n].subvendor_modules_dir = strdup(".modules");
        } else {
            loaded_mods[loaded_n].is_subvendor_stub = 0;
            loaded_mods[loaded_n].subvendor_modules_dir = NULL;
        }
        loaded_n++;
    }

    if (loaded_n > 0) {
        qsort(loaded_mods, loaded_n, sizeof(axle_transitive_mod), cmp_transitive_mods);
    }

    orig_ext_libs_t main_orig = extract_orig_ext_libs(main_receipt);

    for (size_t j = loaded_n; j > 0; j--) {
        const char *base_dir = loaded_mods[j - 1].is_subvendor_stub
                                   ? loaded_mods[j - 1].subvendor_modules_dir
                                   : ".modules";
        axle_receipt_merge(main_receipt, &loaded_mods[j - 1].recp,
                           loaded_mods[j - 1].name, base_dir);
    }

    restore_orig_ext_libs(main_receipt, main_orig);

    for (size_t i = 0; i < loaded_n; i++) {
        orig_ext_libs_t mod_orig = extract_orig_ext_libs(&loaded_mods[i].recp);

        char **trans_deps = NULL;
        size_t trans_n = 0;

        for (size_t d = 0; d < loaded_mods[i].recp.deps_n; d++) {
            uax_strlist_extend(&trans_deps, &trans_n, loaded_mods[i].recp.dependencies[d].name);
        }

        size_t head2 = 0;
        while (head2 < trans_n) {
            const char *curr_dep = trans_deps[head2++];

            for (size_t j = 0; j < loaded_n; j++) {
                if (strcmp(loaded_mods[j].name, curr_dep) != 0) continue;

                for (size_t d = 0; d < loaded_mods[j].recp.deps_n; d++) {
                    const char *next_dep = loaded_mods[j].recp.dependencies[d].name;
                    int found = 0;
                    for (size_t k = 0; k < trans_n; k++) {
                        if (strcmp(trans_deps[k], next_dep) != 0) continue;

                        found = 1;
                        break;
                    }
                    if (found) continue;

                    uax_strlist_extend(&trans_deps, &trans_n, next_dep);
                }
                break;
            }
        }

        for (size_t j = loaded_n; j > 0; j--) {
            const char *dep_candidate = loaded_mods[j - 1].name;
            int is_dep = 0;
            for (size_t k = 0; k < trans_n; k++) {
                if (strcmp(trans_deps[k], dep_candidate) != 0) continue;

                is_dep = 1;
                break;
            }

            if (is_dep) {
                char *dep_base_dir = NULL;
                if (loaded_mods[j - 1].is_subvendor_stub) {
                    dep_base_dir = uax_path_concat("../..", loaded_mods[j - 1].subvendor_modules_dir);
                } else {
                    dep_base_dir = strdup("..");
                }
                axle_receipt_merge(&loaded_mods[i].recp, &loaded_mods[j - 1].recp, dep_candidate, dep_base_dir);
                free(dep_base_dir);
            }
        }

        uax_free_strlist(&trans_deps, &trans_n);
        restore_orig_ext_libs(&loaded_mods[i].recp, mod_orig);
    }

    /* Count non-stub entries — stubs are sub-vendor modules we synthesised
     * above and must NOT be returned to the caller (the caller would try to
     * build them, which is both unnecessary and would fail because their
     * "base path" doesn't exist locally). */
    size_t out_n = 0;
    for (size_t i = 0; i < loaded_n; i++) {
        if (!loaded_mods[i].is_subvendor_stub) out_n++;
    }

    if (out_n > 0) {
        *out_receipts = malloc(sizeof(axle_receipt) * out_n);
        if (!*out_receipts) goto fail;

        size_t out_idx = 0;
        for (size_t i = 0; i < loaded_n; i++) {
            if (loaded_mods[i].is_subvendor_stub) {
                /* Stub: its receipt has been merged into main_receipt above
                 * and we don't need it anymore. Free its resources. */
                clean_axle_receipt(&loaded_mods[i].recp);
            } else {
                (*out_receipts)[out_idx++] = loaded_mods[i].recp;
            }
            free(loaded_mods[i].name);
            if (loaded_mods[i].subvendor_modules_dir)
                free(loaded_mods[i].subvendor_modules_dir);
        }
    }
    *recp_n = out_n;

    free(loaded_mods);
    uax_free_strlist(&queue, &queue_n);
    free(modules_path);

    return 0;

fail:
    if (loaded_mods) {
        for (size_t i = 0; i < loaded_n; i++) {
            free(loaded_mods[i].name);
            if (loaded_mods[i].subvendor_modules_dir)
                free(loaded_mods[i].subvendor_modules_dir);
        }
        free(loaded_mods);
    }
    uax_free_strlist(&queue, &queue_n);
    free(modules_path);
    return -1;
}
