#include "axle/building.h"
#include "axle/path_utils.h"
#include "axle/settings.h"
#include "axle/types.h"
#include "axle/string_utils.h"

#include "stdio.h"
#include <stdlib.h>


#define AXLE_VERSION "v0.0.1"

static int axle_compile_sources(const axle_receipt *receipt);

static int axle_link_executable(const axle_receipt *receipt);
static int axle_link_static_lib(const axle_receipt *receipt);
static int axle_link_dynamic_lib(const axle_receipt *receipt);

int axle_build(const axle_receipt *receipt, bool hide_greeting){
    (void)receipt;

    if (!hide_greeting)
        printf("[axle] version " AXLE_VERSION "\n");

    printf(
        "[axle] building %s project, version: %s\n",
        receipt->metadata.name, receipt->metadata.version
    );

    int ret = 0;

    ret = axle_compile_sources(receipt);
    if (ret < 0){
        fprintf(stderr, "[axle] failed to build project at source compilation stage, code: %d\n", ret);
        return -1;
    }

    switch (receipt->output.type){
        case EXECUTABLE:
            printf("[axle] linking executable\n");
            ret = axle_link_executable(receipt);
            break;
        case DYN_LIBRARY:
            printf("[axle] linking dynamic library\n");
            ret = axle_link_dynamic_lib(receipt);
            break;
        case STATIC_LIBRARY:
            printf("[axle] linking static library\n");
            ret = axle_link_static_lib(receipt);
            break;

        default: break;
    }

    if (ret < 0){
        fprintf(stderr, "[axle] failed to build project at linking stage, code: %d\n", ret);
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

static int axle_compile_sources(const axle_receipt *receipt){
    const axle_sources *sources = &receipt->sources;
    if (!sources->sources){
        fprintf(stderr, "[axle][compilation] no sources given\n");
        return -1;
    }

    char *_cmb_includes = sources->incl_dirs ? uax_concat(
        sources->incl_dirs, " ", "-I"
    ): NULL;

    char *_cmb_pkgs_cflags = sources->pkgs ? uax_concat(
        sources->pkgs, ") ", " $(pkg-config --cflags "
    ): NULL;

    char *_cmb_defines = receipt->target.defines ? uax_concat(
        receipt->target.defines, " ", " -D"
    ): NULL;

    for (int i = 0; 0 != sources->sources[i]; i++){
        const char *_this_source = sources->sources[i];
        // printf("[axle][compilation] path: %s\n", _this_source);

        char *cmd = NULL;

        uax_ip_strextend(&cmd, receipt->compiler);

        if (_cmb_pkgs_cflags){
            uax_ip_strextend(&cmd, _cmb_pkgs_cflags);
        }

        if (_cmb_defines){
            uax_ip_strextend(&cmd, _cmb_defines);
        }

        if (receipt->output.type == DYN_LIBRARY){
            uax_ip_strextend(&cmd, " -fPIC");
        }

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

        char *changed = uax_path_change_ext(_this_source, "o");
        char *cropped = uax_strrepl(changed, '/', '_');
        char *o_path  = uax_path_concat(receipt->output.obj_path, cropped);

        uax_ip_strextend(&cmd, o_path);
        free(cropped);
        free(changed);
        free(o_path);

        printf("[axle][compilation] command: %s\n", cmd);
        int ret = system(cmd);
        free(cmd);

        if (0 != ret){
            fprintf(stderr, "[axle][compilation] failed to execute compilation command. Exit code: %d\n", ret);

            goto fail;
        }
    }

ok:
    if (_cmb_includes) free(_cmb_includes);
    if (_cmb_pkgs_cflags) free(_cmb_pkgs_cflags);
    if (_cmb_defines) free(_cmb_defines);
    return 0;
fail:
    if (_cmb_includes) free(_cmb_includes);
    if (_cmb_pkgs_cflags) free(_cmb_pkgs_cflags);
    if (_cmb_defines) free(_cmb_defines);
    return -1;
}

typedef struct {
    char *_cmb_sources;
    char *_cmb_libdirs;
    char *_cmb_libs;
    char *_cmb_pkgs_libs;
} _axle_link_metadata;

static _axle_link_metadata axle_link_metadata(const axle_receipt *receipt){
    char *_cmb_sources = NULL;

    const axle_sources *sources = &receipt->sources;
    for (int i = 0; 0 != sources->sources[i]; i++){
        const char *_this_source = sources->sources[i];

        char *changed = uax_path_change_ext(_this_source, "o");
        char *cropped = uax_strrepl(changed, '/', '_');
        char *o_path  = uax_path_concat(receipt->output.obj_path, cropped);
        free(changed);
        free(cropped);

        uax_ip_strextend(&_cmb_sources, o_path);
        uax_ip_strextend(&_cmb_sources, " ");

        // printf("[axle][linking] path for .o: %s\n", o_path);
        free(o_path);
    }
    printf("[axle][linking] .o paths: %s\n", _cmb_sources);

    char *_cmb_libdirs = sources->lib_dirs ? uax_concat(
        sources->lib_dirs, " ", "-L"
    ): NULL;

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

static int axle_link_executable(const axle_receipt *receipt){
    _axle_link_metadata md = axle_link_metadata(receipt);

    char *cmd = NULL;
    uax_ip_strextend(&cmd, receipt->compiler);

    if (receipt->target.flags){
        uax_ip_strextend(&cmd, " ");
        uax_ip_strextend(&cmd, receipt->target.flags);
    }

    uax_ip_strextend(&cmd, " -o ");
    uax_ip_strextend(&cmd, receipt->output.path);
    uax_ip_strextend(&cmd, " ");
    uax_ip_strextend(&cmd, md._cmb_sources);

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

    printf("[axle][link] command: %s\n", cmd);
    int ret = system(cmd);
    free(cmd);

    if (ret != 0){
        fprintf(stderr, "[axle][linking] failed to execute linking command. Exit code: %d\n", ret);
        return -1;
    }

    return 0;
}

static int axle_link_static_lib(const axle_receipt *receipt){
    _axle_link_metadata md = axle_link_metadata(receipt);

    char *cmd = NULL;
    uax_ip_strextend(&cmd, "ar rcs ");
    uax_ip_strextend(&cmd, receipt->output.path);
    uax_ip_strextend(&cmd, ".a ");
    uax_ip_strextend(&cmd, md._cmb_sources);

    free(md._cmb_sources);
    if (md._cmb_pkgs_libs) free(md._cmb_pkgs_libs);
    if (md._cmb_libdirs)   free(md._cmb_libdirs);
    if (md._cmb_libs)      free(md._cmb_libs);

    printf("[axle][link] command: %s\n", cmd);
    int ret = system(cmd);
    free(cmd);

    if (ret != 0){
        fprintf(stderr, "[axle][linking] failed to execute linking command. Exit code: %d\n", ret);
        return -1;
    }

    return 0;
}

static int axle_link_dynamic_lib(const axle_receipt *receipt){
    _axle_link_metadata md = axle_link_metadata(receipt);

    char *cmd = NULL;
    uax_ip_strextend(&cmd, receipt->compiler);

    if (receipt->target.flags){
        uax_ip_strextend(&cmd, " ");
        uax_ip_strextend(&cmd, receipt->target.flags);
    }

    uax_ip_strextend(&cmd, " -shared -o ");
    uax_ip_strextend(&cmd, receipt->output.path);
    uax_ip_strextend(&cmd, ".so ");
    uax_ip_strextend(&cmd, md._cmb_sources);

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

    printf("[axle][link] command: %s\n", cmd);
    int ret = system(cmd);
    free(cmd);

    if (ret != 0){
        fprintf(stderr, "[axle][linking] failed to execute linking command. Exit code: %d\n", ret);
        return -1;
    }

    return 0;
}
