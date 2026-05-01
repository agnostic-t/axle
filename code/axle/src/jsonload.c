#include "axle/jsonload.h"
#include "axle/dependencies.h"
#include "axle/path_utils.h"
#include "axle/settings.h"
#include "axle/string_utils.h"
#include <yyjson.h>
#include <string.h>

static axb_optilevel parse_optimization(const char *opt) {
    if (!opt) return O0;
    if (strcmp(opt, "O0") == 0) return O0;
    if (strcmp(opt, "O1") == 0) return O1;
    if (strcmp(opt, "O2") == 0) return O2;
    if (strcmp(opt, "O3") == 0) return O3;
    if (strcmp(opt, "Ofast") == 0 || strcmp(opt, "OFAST") == 0) return OFAST;
    return O0;
}

static const char **json_arr_to_strlist(yyjson_val *arr) {
    if (!arr || !yyjson_is_arr(arr)) return NULL;

    size_t count = yyjson_arr_size(arr);
    const char **list = calloc(count + 1, sizeof(char *));

    size_t idx = 0;
    yyjson_val *val;
    yyjson_arr_iter iter;
    yyjson_arr_iter_init(arr, &iter);

    while ((val = yyjson_arr_iter_next(&iter))) {
        if (yyjson_is_str(val)) {
            list[idx++] = strdup(yyjson_get_str(val));
        }
    }
    return list;
}

static const char **parse_defines(yyjson_val *arr) {
    if (!arr || !yyjson_is_arr(arr)) return NULL;

    size_t count = yyjson_arr_size(arr);
    const char **list = calloc(count + 1, sizeof(char *));

    size_t idx = 0;
    yyjson_val *obj;
    yyjson_arr_iter iter;
    yyjson_arr_iter_init(arr, &iter);

    while ((obj = yyjson_arr_iter_next(&iter))) {
        if (yyjson_is_obj(obj)) {
            yyjson_obj_iter o_iter;
            yyjson_obj_iter_init(obj, &o_iter);
            yyjson_val *key, *val;

            if ((key = yyjson_obj_iter_next(&o_iter))) {
                val = yyjson_obj_iter_get_val(key);

                const char *k_str = yyjson_get_str(key);
                const char *v_str = yyjson_get_str(val);

                if (k_str && v_str) {
                    size_t len = strlen(k_str) + strlen(v_str) + 2;
                    char *def_str = malloc(len);
                    snprintf(def_str, len, "%s=%s", k_str, v_str);
                    list[idx++] = def_str;
                }
            }
        }
    }
    return list;
}

int axle_load_settings(axle_receipt *recp, const char *defaults_path, const char *target_name) {
    if (!defaults_path) return -1;

    char *dir_path = uax_path_get_dir(defaults_path);

    yyjson_read_err err;
    yyjson_doc *doc = yyjson_read_file(defaults_path, 0, NULL, &err);
    if (!doc) {
        fprintf(stderr, "[warn] defaults file %s not found or invalid: %s\n", defaults_path, err.msg);
        free(dir_path);
        return -1;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *comp = yyjson_obj_get(root, "compiler");
    if (comp) {
        const char *cc = yyjson_get_str(yyjson_obj_get(comp, "cc"));
        if (cc) recp->compiler = strdup(cc);

        const char *obj_path = yyjson_get_str(yyjson_obj_get(comp, "objects_path"));
        if (obj_path) recp->output.obj_path = strdup(obj_path);

        const char *binary_type = yyjson_get_str(yyjson_obj_get(comp, "binary"));
        if (binary_type) {
            if (strcmp(binary_type, "executable") == 0){
                recp->output.type = EXECUTABLE;
            } else if (strcmp(binary_type, "shared-lib") == 0){
                recp->output.type = DYN_LIBRARY;
            } else if (strcmp(binary_type, "static-lib") == 0){
                recp->output.type = STATIC_LIBRARY;
            } else {
                fprintf(stderr, "[axle] error: unknown type of binary output \"%s\", fallback to \"executable\"\n", binary_type);
                recp->output.type = EXECUTABLE;
            }
        }
    }

    yyjson_val *metadata = yyjson_obj_get(root, "metadata");
    if (metadata) {
        const char *name = yyjson_get_str(yyjson_obj_get(metadata, "name"));
        if (name) recp->metadata.name = strdup(name);

        const char *ver = yyjson_get_str(yyjson_obj_get(metadata, "version"));
        if (ver) recp->metadata.version = strdup(ver);

        yyjson_val *prio = yyjson_obj_get(metadata, "priority");
        if (prio) recp->metadata.priority = yyjson_get_int(prio);
    }

    yyjson_val *code = yyjson_obj_get(root, "code");
    if (code){
        const char **sources_arr = json_arr_to_strlist(yyjson_obj_get(code, "sources"));
        if (sources_arr) recp->sources.sources = sources_arr;

        const char **includes_arr = json_arr_to_strlist(yyjson_obj_get(code, "includes"));
        if (includes_arr) {
            recp->sources.incl_dirs = includes_arr;
        }

        const char **libraries_arr = json_arr_to_strlist(yyjson_obj_get(code, "libraries"));
        if (libraries_arr) recp->sources.libs = libraries_arr;

        const char **libdirs_arr = json_arr_to_strlist(yyjson_obj_get(code, "lib-dirs"));
        if (libdirs_arr) recp->sources.lib_dirs = libdirs_arr;

        const char **packages_arr = json_arr_to_strlist(yyjson_obj_get(code, "packages"));
        if (packages_arr) recp->sources.pkgs = packages_arr;

        const char *output = yyjson_get_str(yyjson_obj_get(code, "output"));
        if (output) recp->output.path = strdup(output);
    }

    yyjson_val *deps = yyjson_obj_get(root, "dependencies");
    if (deps){
        size_t idx = 0;
        yyjson_val *key, *val;
        yyjson_obj_iter iter;
        yyjson_obj_iter_init(deps, &iter);

        while ((key = yyjson_obj_iter_next(&iter))) {
            val = yyjson_obj_iter_get_val(key);
            const char *strkey = yyjson_get_str(key);
            const char *version = yyjson_get_str(yyjson_obj_get(val, "version"));
            const char *url = yyjson_get_str(yyjson_obj_get(val, "url"));

            printf("[axle][deps] module: %s\n", strkey);

            if (url) {
                if (0 > axle_dep_download(url, version, dir_path, strkey)){
                    fprintf(stderr, "[axle][deps] failed to download dependency\n");
                    free(dir_path);
                    yyjson_doc_free(doc);

                    if (recp->dependencies){
                        for (size_t j = 0; j < recp->deps_n; j++)
                            free((void*)recp->dependencies[j].name);
                        free(recp->dependencies);
                        recp->dependencies = NULL;
                        recp->deps_n = 0;
                    }

                    return -1;
                }
            }
            axle_dependency dep = {.name = strdup(strkey)};
            if ( 0 > axle_dep_parsever(&dep, version)){
                fprintf(stderr, "[axle][deps] failed to parse version for dependency\n");
                free(dir_path);
                yyjson_doc_free(doc);

                if (recp->dependencies){
                    for (size_t j = 0; j < recp->deps_n; j++)
                        free((void*)recp->dependencies[j].name);
                    free(recp->dependencies);
                    recp->dependencies = NULL;
                    recp->deps_n = 0;
                }

                return -1;
            }

            axle_dependency *deps = realloc(recp->dependencies, sizeof(axle_dependency) * (recp->deps_n + 1));
            if (!deps){
                fprintf(stderr, "[axle][deps] failed to realloc\n");
                free(dir_path);
                yyjson_doc_free(doc);
                return -1;
            }

            deps[recp->deps_n] = dep;
            recp->deps_n++;
            recp->dependencies = deps;
        }
    }

    if (target_name) {
        yyjson_val *targets = yyjson_obj_get(root, "targets");
        yyjson_val *targ = yyjson_obj_get(targets, target_name);
        if (targ) {
            recp->target.optimize = parse_optimization(yyjson_get_str(yyjson_obj_get(targ, "optimization")));

            const char **flags_arr = json_arr_to_strlist(yyjson_obj_get(targ, "flags"));
            if (flags_arr) {
                recp->target.flags = uax_concat(flags_arr, " ", NULL);
                uax_free_strlist_ne((char***)&flags_arr);
            }

            recp->target.defines = parse_defines(yyjson_obj_get(targ, "defines"));
        } else {
            fprintf(stderr, "[warn] target '%s' not found in %s\n", target_name, defaults_path);
        }
    }

    free(dir_path);
    yyjson_doc_free(doc);

    return 0;
}
