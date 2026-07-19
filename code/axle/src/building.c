#include "axle/building.h"
#include "axle/path_utils.h"
#include "axle/settings.h"
#include "axle/types.h"
#include "axle/string_utils.h"
#include "axle/colors.h"

#include "stdio.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define AXLE_VERSION "v0.0.1"

static int axle_compile_sources(const axle_receipt *receipt, const char *base_path, bool rebuild, bool silent);

static int axle_link_executable(const axle_receipt *receipt, const char *base_path, bool silent);
static int axle_link_static_lib(const axle_receipt *receipt, const char *base_path, bool silent);
static int axle_link_dynamic_lib(const axle_receipt *receipt, const char *base_path, bool silent);

static int _needs_rebuild(const char *source_file, const char *obj_file) {
    struct stat src_stat, obj_stat;

    if (stat(source_file, &src_stat) != 0) return -1;
    if (stat(obj_file, &obj_stat) != 0)    return 1;
    if (src_stat.st_mtime > obj_stat.st_mtime) return 1;

    return 0;
}

int axle_build(const axle_receipt *receipt, const char *base_path, bool hide_greeting, bool rebuild, bool silent){

    if (!hide_greeting && !silent)
        printf("[axle] version " AXLE_VERSION "\n");

    if (!silent){
        printf(
            "[axle] building %s project, version: %s\n",
            receipt->metadata.name, receipt->metadata.version
        );
    }

    int ret = 0;

    char *obj_path = uax_path_concat(base_path, receipt->output.obj_path);
    struct stat st;
    if (stat(obj_path, &st) != 0){
        int status = mkdir(obj_path, 0755);
        if (status != 0) {
            fprintf(stderr, "%s[axle] failed to build project%s, cannot create object files (objs) directory: %s\n", xfore.red, xfore.normal, obj_path);
            return -1;
        }
    }
    free(obj_path);

    char *out_path_file = uax_path_concat(base_path, receipt->output.path);
    char *out_path = uax_path_get_dir(out_path_file);
    free(out_path_file);

    if (stat(out_path, &st) != 0){
        int status = mkdir(out_path, 0755);
        if (status != 0) {
            fprintf(stderr, "%s[axle] failed to build project%s, cannot create output files (bin) directory: %s\n", xfore.red, xfore.normal, out_path);
            return -1;
        }
    }
    free(out_path);

    ret = axle_compile_sources(receipt, base_path, rebuild, silent);
    if (ret < 0){
        fprintf(stderr, "%s[axle] failed to build project%s at source compilation stage, code: %d\n", xfore.red, xfore.normal, ret);
        return -1;
    }

    if (receipt->only_deps) {
        return 0;
    }

    switch (receipt->output.type){
        case EXECUTABLE:
            if (!silent) printf("[axle] linking %sexecutable%s\n", xfore.yellow, xfore.normal);
            ret = axle_link_executable(receipt, base_path, silent);
            break;
        case DYN_LIBRARY:
            if (!silent) printf("[axle] linking %sdynamic library%s\n", xfore.yellow, xfore.normal);
            ret = axle_link_dynamic_lib(receipt, base_path, silent);
            break;
        case STATIC_LIBRARY:
            if (!silent) printf("[axle] linking %sstatic library%s\n", xfore.yellow, xfore.normal);
            ret = axle_link_static_lib(receipt, base_path, silent);
            break;

        default: break;
    }

    if (ret < 0){
        fprintf(stderr, "%s[axle] failed to build project%s at linking stage, code: %d\n", xfore.red, xfore.normal, ret);
        return -1;
    }

    return 0;
}

static const char *axle_decr_optimization(axb_optilevel level){
    switch (level) {
        case O0: return "O0";
        case O1: return "O1";
        case O2: return "O2";
        case O3: return "O3";
        case OFAST: return "Ofast";
    }

    return "O0";
}

void uax_trim_leading_dot_uscore(char *str) {
    if (!str || !*str) return;

    char *src = str + strspn(str, "._");

    if (src != str) {
        memmove(str, src, strlen(src) + 1);
    }
}

static int axle_compile_sources(const axle_receipt *receipt, const char *base_path, bool rebuild, bool silent){
    const axle_sources *sources = &receipt->sources;
    if (!sources->sources && !receipt->only_deps){
        fprintf(stderr, "%s[axle][compilation] no sources given%s\n", xfore.red, xfore.normal);
        return -1;
    } else if (receipt->only_deps){
        printf("[axle][compilation] only deps specified, %sskipping compilation%s\n", xfore.magenta, xfore.normal);
        return 0;
    }

    char **out_sources = NULL;
    size_t src_sz = 0;
    uax_expand_files(base_path, sources->sources, &out_sources, &src_sz);

    char **out_incls = NULL;
    size_t incls_sz = 0;
    if (sources->incl_dirs) {
        uax_expand_files(base_path, sources->incl_dirs, &out_incls, &incls_sz);
        uax_strlist_extend(&out_incls, &incls_sz, 0);
    }

    char *_cmb_includes = sources->incl_dirs ? uax_concat(
        (const char **)out_incls, " ", "-I"
    ): NULL;

    if (incls_sz != 0)
        uax_free_strlist(&out_incls, &incls_sz);

    char *_cmb_pkgs_cflags = sources->pkgs ? uax_concat(
        sources->pkgs, ") ", " $(pkg-config --cflags "
    ): NULL;

    char *_cmb_defines = receipt->target.defines ? uax_concat(
        receipt->target.defines, " ", " -D"
    ): NULL;

    char *obj_path = uax_path_concat(base_path, receipt->output.obj_path);

    for (size_t i = 0; i < src_sz; i++){
        const char *_this_source = out_sources[i];

        char *cmd = NULL;

        char *changed = uax_path_change_ext(_this_source, "o");
        char *cropped = uax_strrepl(changed, '/', '_');

        uax_trim_leading_dot_uscore(cropped);
        char *o_path  = uax_path_concat(obj_path, cropped);

        int nr_ret = _needs_rebuild(_this_source, o_path);
        if (nr_ret == -1) {
            fprintf(stderr, "%s[axle][compilation] failed to get information%s about files\n", xfore.red, xfore.normal);
            free(cropped);
            free(changed);
            free(o_path);
            goto fail;
        } else if (nr_ret == 0 && !rebuild){
            if (!silent) printf("[axle][compilation] %sskipping %s%s, already up to date\n", xfore.magenta, _this_source, xfore.normal);

            free(cropped);
            free(changed);
            free(o_path);
            continue;
        }

        uax_ip_strextend(&cmd, receipt->compiler);

        if (_cmb_pkgs_cflags) uax_ip_strextend(&cmd, _cmb_pkgs_cflags);
        if (_cmb_defines)     uax_ip_strextend(&cmd, _cmb_defines);

        if (receipt->output.type == DYN_LIBRARY)
            uax_ip_strextend(&cmd, " -fPIC");

        if (receipt->target.flags){
            uax_ip_strextend(&cmd, " ");
            uax_ip_strextend(&cmd, receipt->target.flags);
        }

        uax_ip_strextend(&cmd, " -");
        uax_ip_strextend(&cmd, axle_decr_optimization(receipt->target.optimize));

        if (_cmb_includes) {
            uax_ip_strextend(&cmd, " ");
            uax_ip_strextend(&cmd, _cmb_includes);
        }

        uax_ip_strextend(&cmd, " -c ");
        uax_ip_strextend(&cmd, _this_source);
        uax_ip_strextend(&cmd, " -o ");
        uax_ip_strextend(&cmd, o_path);

        char *dir_output = uax_path_get_dir(o_path);
        struct stat st;
        if (stat(dir_output, &st) != 0){
            mkdir(dir_output, 0755);
        }
        free(dir_output);

        free(cropped);
        free(changed);
        free(o_path);

        if (!silent) printf("[axle][compilation] command: %s%s%s\n", xfore.gray, cmd, xfore.normal);
        int ret = system(cmd);
        free(cmd);

        if (0 != ret){
            fprintf(stderr, "%s[axle][compilation] failed to execute compilation command%s. Exit code: %d\n", xfore.red, xfore.normal, ret);

            goto fail;
        }
    }

ok:
    if (_cmb_includes) free(_cmb_includes);
    if (_cmb_pkgs_cflags) free(_cmb_pkgs_cflags);
    if (_cmb_defines) free(_cmb_defines);
    free(obj_path);
    uax_free_strlist(&out_sources, &src_sz);
    return 0;
fail:
    if (_cmb_includes) free(_cmb_includes);
    if (_cmb_pkgs_cflags) free(_cmb_pkgs_cflags);
    if (_cmb_defines) free(_cmb_defines);
    free(obj_path);
    uax_free_strlist(&out_sources, &src_sz);
    return -1;
}

typedef struct {
    char *_cmb_sources;
    char *_cmb_libdirs;
    char *_cmb_libs;
    char *_cmb_pkgs_libs;
} _axle_link_metadata;

static _axle_link_metadata axle_link_metadata(const axle_receipt *receipt, const char *base_path){
    char *_cmb_sources = NULL;

    const axle_sources *sources = &receipt->sources;

    char **out_sources = NULL;
    size_t src_sz = 0;
    uax_expand_files(base_path, sources->sources, &out_sources, &src_sz);

    char *obj_path = uax_path_concat(base_path, receipt->output.obj_path);

    for (size_t i = 0; i < src_sz; i++){
        const char *_this_source = out_sources[i];

        char *changed = uax_path_change_ext(_this_source, "o");
        char *cropped = uax_strrepl(changed, '/', '_');

        uax_trim_leading_dot_uscore(cropped);
        char *o_path  = uax_path_concat(obj_path, cropped);

        free(changed);
        free(cropped);

        uax_ip_strextend(&_cmb_sources, o_path);
        uax_ip_strextend(&_cmb_sources, " ");

        // printf("[axle][linking] path for .o: %s\n", o_path);
        free(o_path);
    }
    uax_free_strlist(&out_sources, &src_sz);
    free(obj_path);

    // printf("[axle][linking] .o paths: %s\n", _cmb_sources);

    char **out_libs = NULL;
    size_t libs_sz = 0;
    if (sources->lib_dirs) {

        uax_expand_files(base_path, sources->lib_dirs, &out_libs, &libs_sz);
        uax_strlist_extend(&out_libs, &libs_sz, 0);
    }

    char *_cmb_libdirs = sources->lib_dirs ? uax_concat(
        (const char **)out_libs, " ", "-L"
    ): NULL;

    if (out_libs)
        uax_free_strlist(&out_libs, &libs_sz);

    char *_cmb_libs = sources->libs ? uax_concat(
        sources->libs, " ", "-l"
    ): NULL;

    char *_cmb_pkgs_libs = sources->pkgs ? uax_concat(
        sources->pkgs, ") ", " $(pkg-config --libs "
    ): NULL;

    return (_axle_link_metadata){
        ._cmb_libdirs = _cmb_libdirs,
        ._cmb_libs = _cmb_libs,
        ._cmb_sources = _cmb_sources,
        ._cmb_pkgs_libs = _cmb_pkgs_libs
    };
}

static int axle_link_executable(const axle_receipt *receipt, const char *base_path, bool silent){
    _axle_link_metadata md = axle_link_metadata(receipt, base_path);

    char *cmd = NULL;
    uax_ip_strextend(&cmd, receipt->compiler);

    if (receipt->target.flags){
        uax_ip_strextend(&cmd, " ");
        uax_ip_strextend(&cmd, receipt->target.flags);
    }

    char *output_path = uax_path_concat(base_path, receipt->output.path);

    uax_ip_strextend(&cmd, " -o ");
    uax_ip_strextend(&cmd, output_path);
    uax_ip_strextend(&cmd, " ");
    uax_ip_strextend(&cmd, md._cmb_sources);
    free(output_path);

    if (md._cmb_libdirs) {
        uax_ip_strextend(&cmd, md._cmb_libdirs);
        uax_ip_strextend(&cmd, " ");
    }

    if (md._cmb_libs) {
        uax_ip_strextend(&cmd, md._cmb_libs);
        uax_ip_strextend(&cmd, " ");
    }

    free(md._cmb_sources);
    if (md._cmb_pkgs_libs) free(md._cmb_pkgs_libs);
    if (md._cmb_libdirs)   free(md._cmb_libdirs);
    if (md._cmb_libs)      free(md._cmb_libs);

    if (!silent) printf("[axle][link] command: %s%s%s\n", xfore.gray, cmd, xfore.normal);
    int ret = system(cmd);
    free(cmd);

    if (ret != 0){
        fprintf(stderr, "%s[axle][linking] failed to execute linking%s command. Exit code: %d\n", xfore.red, xfore.normal, ret);
        return -1;
    }

    return 0;
}

static int axle_link_static_lib(const axle_receipt *receipt, const char *base_path, bool silent){
    _axle_link_metadata md = axle_link_metadata(receipt, base_path);

    char *_dirname = uax_path_get_dir(receipt->output.path);
    const char *filename = uax_path_filename(receipt->output.path);

    char *libname = NULL;
    if (strncmp(filename, "lib", 3) != 0){
        libname = malloc(1 + 3 + strlen(filename));
        strcpy(libname, "lib");
        strcpy(libname + 3, filename);
    } else {
        libname = strdup(filename);
    }

    char *lib_path = uax_path_concat(_dirname, libname);
    char *output_path = uax_path_concat(base_path, lib_path);
    free(lib_path);
    free(libname);
    free(_dirname);

    char *cmd = NULL;
    uax_ip_strextend(&cmd, receipt->linker);
    uax_ip_strextend(&cmd, " rcs ");
    uax_ip_strextend(&cmd, output_path);
    uax_ip_strextend(&cmd, ".a ");
    uax_ip_strextend(&cmd, md._cmb_sources);
    free(output_path);

    free(md._cmb_sources);
    if (md._cmb_pkgs_libs) free(md._cmb_pkgs_libs);
    if (md._cmb_libdirs)   free(md._cmb_libdirs);
    if (md._cmb_libs)      free(md._cmb_libs);

    if (!silent) printf("[axle][link] command: %s%s%s\n", xfore.gray, cmd, xfore.normal);
    int ret = system(cmd);
    free(cmd);

    if (ret != 0){
        fprintf(stderr, "%s[axle][linking] failed to execute linking%s command. Exit code: %d\n", xfore.red, xfore.normal, ret);
        return -1;
    }

    return 0;
}

static int axle_link_dynamic_lib(const axle_receipt *receipt, const char *base_path, bool silent){
    _axle_link_metadata md = axle_link_metadata(receipt, base_path);

    char *cmd = NULL;
    uax_ip_strextend(&cmd, receipt->compiler);

    if (receipt->target.flags){
        uax_ip_strextend(&cmd, " ");
        uax_ip_strextend(&cmd, receipt->target.flags);
    }

    char *_dirname = uax_path_get_dir(receipt->output.path);
    const char *filename = uax_path_filename(receipt->output.path);

    char *libname = NULL;
    if (strncmp(filename, "lib", 3) != 0){
        libname = malloc(1 + 3 + strlen(filename));
        strcpy(libname, "lib");
        strcpy(libname + 3, filename);
    } else {
        libname = strdup(filename);
    }

    char *lib_path = uax_path_concat(_dirname, libname);
    char *output_path = uax_path_concat(base_path, lib_path);
    free(lib_path);
    free(libname);
    free(_dirname);

    uax_ip_strextend(&cmd, " -shared -o ");
    uax_ip_strextend(&cmd, output_path);
    uax_ip_strextend(&cmd, ".so ");
    uax_ip_strextend(&cmd, md._cmb_sources);

    free(output_path);

    if (md._cmb_libdirs) {
        uax_ip_strextend(&cmd, md._cmb_libdirs);
        uax_ip_strextend(&cmd, " ");
    }

    if (md._cmb_libs) {
        uax_ip_strextend(&cmd, md._cmb_libs);
        uax_ip_strextend(&cmd, " ");
    }

    free(md._cmb_sources);
    if (md._cmb_pkgs_libs) free(md._cmb_pkgs_libs);
    if (md._cmb_libdirs)   free(md._cmb_libdirs);
    if (md._cmb_libs)      free(md._cmb_libs);

    if (!silent) printf("[axle][link] command: %s%s%s\n", xfore.gray, cmd, xfore.normal);
    int ret = system(cmd);
    free(cmd);

    if (ret != 0){
        fprintf(stderr, "%s[axle][linking] failed to execute linking%s command. Exit code: %d\n", xfore.red, xfore.normal, ret);
        return -1;
    }

    return 0;
}
