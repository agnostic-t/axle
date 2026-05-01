#include "axle/dependencies.h"
#include "axle/path_utils.h"
#include "axle/string_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <yyjson.h>

static int axle_ver_cmp(int maj1, int mid1, int min1, int maj2, int mid2, int min2) {
    if (maj1 != maj2) return maj1 > maj2 ? 1 : -1;
    if (mid1 != mid2) return mid1 > mid2 ? 1 : -1;
    if (min1 != min2) return min1 > min2 ? 1 : -1;
    return 0;
}

/*
 * git clone URL /tmp/axle_download_<TOKEN>/
 * module structure:
 * -> include
 * -> src
 * -> module.json
 * -> defaults.json
 */

int axle_dep_download(
    const char *url,
    const char *version,
    const char *base_path,
    const char *module_name
){
    if (!url || !version || !base_path) return -1;

    struct stat st;

    char *dep_path_base = uax_path_concat(base_path, ".modules");
    char *dep_path = uax_path_concat(dep_path_base, module_name);
    free(dep_path_base);

    if (stat(dep_path, &st) != 0) {
        printf("[axle][depman] downloading remote dep: %s -> %s\n", module_name, url);

        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "git clone -q %s %s", url, dep_path);
        if (system(cmd) != 0) {
            fprintf(stderr, "[axle][depman] failed to clone %s\n", url);
            free(dep_path);
            return -1;
        }
    } else {
        printf("[axle][depman] module %s is already downloaded\n", module_name);
    }

    return 0;
}

int axle_dep_parsever(axle_dependency *dep, const char *version){
    if (!dep || !version) return -1;

    const char *ptr = version;
    while (*ptr && (*ptr < '0' || *ptr > '9')) {
        ptr++;
    }

    int major = 0, middle = 0, minor = 0;
    if (3 != sscanf(ptr, "%d.%d.%d", &major, &middle, &minor)){
        fprintf(stderr, "[axle][depman] failed to parse version number in `X.Y.Z` format: %s\n", version);
        return -1;
    }

    dep->major = major;
    dep->middle = middle;
    dep->minor = minor;

    return 0;
}

int axle_dep_check(
    const axle_dependency *dep,
    const char *version
){
    if (!dep || !version) return -1;

    int major, middle, minor;
    int cmp_result;

    if (strncmp(version, ">=", 2) == 0){
        if (3 != sscanf(version, ">=%d.%d.%d", &major, &middle, &minor)){
            fprintf(stderr, "[axle][depcheck] failed to parse version number (must be in `>=X.Y.Z` format)\n");
            return -1;
        }

        cmp_result = axle_ver_cmp(dep->major, dep->middle, dep->minor, major, middle, minor);
        if (cmp_result >= 0) return 0;

        fprintf(stderr, "[axle][depcheck] dependency `%s` is not in `%s` range: %d.%d.%d\n",
                dep->name, version, dep->major, dep->middle, dep->minor);
        return -1;

    } else if (strncmp(version, "<=", 2) == 0){
        if (3 != sscanf(version, "<=%d.%d.%d", &major, &middle, &minor)){
            fprintf(stderr, "[axle][depcheck] failed to parse version number (must be in `<=X.Y.Z` format)\n");
            return -1;
        }

        cmp_result = axle_ver_cmp(dep->major, dep->middle, dep->minor, major, middle, minor);
        if (cmp_result <= 0) return 0;

        fprintf(stderr, "[axle][depcheck] dependency `%s` is not in `%s` range: %d.%d.%d\n",
                dep->name, version, dep->major, dep->middle, dep->minor);
        return -1;

    } else if (strncmp(version, "=", 1) == 0){
        if (3 != sscanf(version, "=%d.%d.%d", &major, &middle, &minor)){
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

    return 0;
}

int axle_dep_inject(
    axle_dependency *dep,
    axle_sources *sources,
    const char *req_version,
    const char *module_name,
    const char *real_path
){
    if (!dep || !sources) return -1;
    printf("[axle][depman] injecting... %s:%s\n", module_name, req_version);

    char *mod_path = uax_path_concat(real_path, "module.json");
    yyjson_read_err err;
    yyjson_doc *doc = yyjson_read_file(mod_path, 0, NULL, &err);

    if (!doc) {
        fprintf(stderr, "[axle] read error (%u): %s at position: %ld\n", err.code, err.msg, err.pos);
        goto fail;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *md = yyjson_obj_get(root, "metadata");
    if (!md){
        fprintf(stderr, "[axle][depman] failed to get metadata from module.json of dependency by %s\n", mod_path);
        goto fail;
    }

    const char *version = yyjson_get_str(yyjson_obj_get(md, "version"));
    if (!version){
        fprintf(stderr, "[axle][depman] failed to get version from metadata of dependency by %s\n", mod_path);
        goto fail;
    }

    if (0 > axle_dep_parsever(dep, version)){
        fprintf(stderr, "[axle][depman] failed to parse version from metadata of dependency by %s\n", mod_path);
        goto fail;
    }

    if (0 > axle_dep_check(dep, req_version)){
        fprintf(stderr, "[axle][depman] version check failed for dependency by %s\n", mod_path);
        goto fail;
    }

    char *incl_dir = uax_path_concat(module_name, "include");
    uax_strlist_extend_ne((char***)&sources->incl_dirs, incl_dir);
    free(incl_dir);

    char *lib_dir = uax_path_concat(module_name, "bin");
    struct stat st;
    if (stat(lib_dir, &st) == 0){
        uax_strlist_extend_ne((char***)&sources->lib_dirs, lib_dir);
    }
    free(lib_dir);

    yyjson_val *code = yyjson_obj_get(root, "code");
    const char *out_name = NULL;
    const char *filename = out_name;
    if (code) {
        out_name = yyjson_get_str(yyjson_obj_get(code, "output"));
        if (filename && *filename) {
            const char *last_slash = strrchr(filename, '/');
            if (last_slash) {
                filename = last_slash + 1;
                if (*filename == '\0') {
                    filename = NULL;
                }
            }
        }
    }

    if (out_name) {
        char *lib_dir = uax_path_concat(module_name, "bin");
        uax_strlist_extend_ne((char***)&sources->lib_dirs, lib_dir);
        free(lib_dir);

        if (filename)
            uax_strlist_extend_ne((char***)&sources->libs, filename);
    }

    yyjson_doc_free(doc);
    free(mod_path);
    return 0;

fail:
    yyjson_doc_free(doc);
    free(mod_path);
    return -1;
}
