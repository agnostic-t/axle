#include "axle/project.h"

#include "axle/colors.h"
#include "axle/jsonload.h"
#include "axle/path_utils.h"
#include "axle/settings.h"
#include "axle/string_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <yyjson.h>

static int axle_receipt_merge(axle_receipt *into, axle_receipt *from, const char *module_name, const char *base_dir){
    char *base_path = uax_path_concat(base_dir, module_name);

    char *incl_dir = uax_path_concat(base_path, "include");
    uax_strlist_extend_ne((char***)&into->sources.incl_dirs, incl_dir);
    free(incl_dir);

    char *lib_dir = uax_path_concat(base_path, "bin");
    struct stat st;
    if (stat(lib_dir, &st) == 0){
        uax_strlist_extend_ne((char***)&into->sources.lib_dirs, lib_dir);
    }
    free(lib_dir);

    const char *filename = from->output.path;
    if (filename && *filename) {
        const char *last_slash = strrchr(filename, '/');
        if (last_slash) {
            filename = last_slash + 1;
            if (*filename == '\0') {
                filename = NULL;
            }
        }
    }

    if (from->output.path) {
        char *lib_dir_again = uax_path_concat(base_path, "bin");
        uax_strlist_extend_ne((char***)&into->sources.lib_dirs, lib_dir_again);
        free(lib_dir_again);

        if (filename)
            uax_strlist_extend_ne((char***)&into->sources.libs, filename);
    }

    free(base_path);
    return 0;
}

int axle_project_prepare(const char *directory, const char *adt_defaults, axle_receipt *outrecp){
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
        printf("[axle] defaults: %s\n", def_path);
        axle_load_settings(outrecp, def_path, target_name);
        free(def_path);
    }

    if (adt_defaults){
        printf("[axle] loading additional defaults: %s\n", adt_defaults);
        axle_load_settings(outrecp, adt_defaults, target_name);
    }

    axle_load_settings(outrecp, directory, NULL);
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

        printf("[axle][mod] processing transitive module: %s\n", mod_name);

        axle_receipt recp;
        char *module_path_base = uax_path_concat(modules_path, mod_name);
        char *module_path = uax_path_concat(module_path_base, "module.json");
        free(module_path_base);

        if (0 > axle_project_prepare(module_path, main_defaults, &recp)){
            fprintf(stderr, "%s[axle][mod] failed to prepare receipt%s for module %s\n", xfore.red, xfore.normal, mod_name);
            free(module_path);
            goto fail;
        }
        free(module_path);

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
        loaded_n++;
    }

    if (loaded_n > 0) {
        qsort(loaded_mods, loaded_n, sizeof(axle_transitive_mod), cmp_transitive_mods);
    }

    orig_ext_libs_t main_orig = extract_orig_ext_libs(main_receipt);

    for (size_t j = loaded_n; j > 0; j--) {
        axle_receipt_merge(main_receipt, &loaded_mods[j - 1].recp, loaded_mods[j - 1].name, ".modules");
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
                axle_receipt_merge(&loaded_mods[i].recp, &loaded_mods[j - 1].recp, dep_candidate, "..");
            }
        }

        uax_free_strlist(&trans_deps, &trans_n);
        restore_orig_ext_libs(&loaded_mods[i].recp, mod_orig);
    }

    if (loaded_n > 0) {
        *out_receipts = malloc(sizeof(axle_receipt) * loaded_n);
        if (!*out_receipts) goto fail;

        for (size_t i = 0; i < loaded_n; i++) {
            (*out_receipts)[i] = loaded_mods[i].recp;
            free(loaded_mods[i].name);
        }
    }
    *recp_n = loaded_n;

    free(loaded_mods);
    uax_free_strlist(&queue, &queue_n);
    free(modules_path);

    return 0;

fail:
    if (loaded_mods) {
        for (size_t i = 0; i < loaded_n; i++) {
            free(loaded_mods[i].name);
        }
        free(loaded_mods);
    }
    uax_free_strlist(&queue, &queue_n);
    free(modules_path);
    return -1;
}
