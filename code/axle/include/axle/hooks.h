#ifndef AXLE_HOOKS_H
#define AXLE_HOOKS_H

#include "axle/settings.h"
#include <stdbool.h>

/* Run a NULL-terminated list of shell commands in base_path.
 * Commands are executed sequentially through /bin/sh -c.
 * The first non-zero command aborts the hook stage and returns -1. */
int axle_hooks_run(const char **commands,
                   const char *stage,
                   const axle_receipt *receipt,
                   const char *base_path,
                   bool rebuild,
                   bool silent);

#endif
