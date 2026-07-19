#ifndef AXLE_JSONLOAD_H
#define AXLE_JSONLOAD_H

#include "axle/settings.h"

int axle_load_settings(axle_receipt *recp, const char *defaults_path,
                       const char *target_name, const char *project_root);

#endif
