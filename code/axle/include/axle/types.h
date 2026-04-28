#ifndef AXLE_TYPES_H
#define AXLE_TYPES_H

typedef enum {
    O0,
    O1,
    O2,
    O3,
    OFAST
} axb_optilevel;

typedef enum {
    EXECUTABLE,
    DYN_LIBRARY,
    STATIC_LIBRARY
} axb_outtype;

#endif
