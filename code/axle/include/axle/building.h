#ifndef AXLE_BUILDING_H
#define AXLE_BUILDING_H

#include "settings.h"
#include "stdbool.h"

#define strlist const char*[]

int axle_build(const axle_receipt *receipt, const char *base_path, bool hide_greeting, bool rebuild, bool silent);

#endif
