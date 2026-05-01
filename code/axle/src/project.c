#include "axle/project.h"

// #include "axle/cleanup.h"
// #include "axle/dependencies.h"
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
        char *lib_dir = uax_path_concat(base_path, "bin");
        uax_strlist_extend_ne((char***)&into->sources.lib_dirs, lib_dir);
        free(lib_dir);

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
    yyjson_doc_free(doc);

    free(dir_path);
    return 0;
}

typedef struct {
    char *name;
    int priority;
} axle_mod_info;

static int cmp_mods(const void *a, const void *b) {
    axle_mod_info *modA = (axle_mod_info *)a;
    axle_mod_info *modB = (axle_mod_info *)b;
    return modA->priority - modB->priority;
}

int axle_modules_prepare(const char *main_defaults, axle_receipt *main_receipt, const char *directory, axle_receipt **out_receipts, size_t *recp_n){
    if (!directory || !out_receipts) return -1;

    *out_receipts = NULL;
    *recp_n = 0;

    char *modules_path = uax_path_concat(directory, ".modules");
    char **mods = NULL;
    size_t mods_n = 0;

    if (0 > uax_expand_subdirs(modules_path, &mods, &mods_n)){
        free(modules_path);
        return 0;
    }

    if (mods_n == 0) {
        free(modules_path);
        if (mods) uax_free_strlist(&mods, &mods_n);
        return 0;
    }

    axle_mod_info *mod_list = calloc(mods_n, sizeof(axle_mod_info));

    for (size_t i = 0; i < mods_n; i++){
        mod_list[i].name = strdup(mods[i]);
        mod_list[i].priority = 1000;

        char *module_path = uax_path_concat(modules_path, mods[i]);
        char *module_json_path = uax_path_concat(module_path, "module.json");
        free(module_path);

        yyjson_read_err err;
        yyjson_doc *doc = yyjson_read_file(module_json_path, 0, NULL, &err);
        if (doc) {
            yyjson_val *md = yyjson_obj_get(yyjson_doc_get_root(doc), "metadata");
            if (md) {
                yyjson_val *prio_val = yyjson_obj_get(md, "priority");
                if (prio_val) mod_list[i].priority = yyjson_get_int(prio_val);
            }
            yyjson_doc_free(doc);
        }
        free(module_json_path);
    }

    size_t og_mods_n = mods_n;
    uax_free_strlist(&mods, &mods_n);

    mods_n = og_mods_n;
    qsort(mod_list, mods_n, sizeof(axle_mod_info), cmp_mods);

    for (size_t i = 0; i < mods_n; i++){
        const char *mod = mod_list[i].name;

        printf("[axle][mod] mod: %s\n", mod);

        axle_receipt recp;

        char *module_path_base = uax_path_concat(modules_path, mod);
        char *module_path = uax_path_concat(module_path_base, "module.json");
        free(module_path_base);
        if (0 > axle_project_prepare(module_path, main_defaults, &recp)){
            free(module_path);
            fprintf(stderr, "[axle][mod] failed to prepare receipt for module %s\n", mod);

            for (size_t j = i; j < mods_n; j++)
                free(mod_list[j].name);
            goto fail;
        }

        axle_receipt_merge(main_receipt, &recp, mod, ".modules");

        axle_receipt *recps = realloc(*out_receipts, sizeof(axle_receipt) * (1 + (*recp_n)));
        if (!recps) {
            free(module_path);
            fprintf(stderr, "[axle][mod] failed to reallocate memory for receipts\n");

            for (size_t j = i; j < mods_n; j++)
                free(mod_list[j].name);
            goto fail;
        }

        recps[*recp_n] = recp;
        *out_receipts = recps;
        (*recp_n)++;

        free(module_path);
        free(mod_list[i].name);
    }

    free(modules_path);
    free(mod_list);
    return 0;
fail:
    free(modules_path);
    free(mod_list);
    return -1;
}
