#ifndef AXLE_SETTINGS_H
#define AXLE_SETTINGS_H

#include "types.h"

typedef struct {
    const char *name;
    const char *version;
    int priority;
} axle_metadata;

typedef struct {
    axb_optilevel optimize;
    const char    *flags;
    const char   **defines;
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
    axb_outtype  type;
    const char  *obj_path;
    const char  *path;
} axle_output;

typedef struct {
    const char  *compiler;
    axle_target  target;
    axle_sources sources;
    axle_output  output;

    axle_metadata metadata;
} axle_receipt;

#endif
