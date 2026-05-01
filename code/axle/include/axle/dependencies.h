#ifndef AXLE_DEPENDENCIES_H
#define AXLE_DEPENDENCIES_H

#include "axle/settings.h"

int axle_dep_download(
    const char *url,
    const char *version,
    const char *base_path,
    const char *module_name
);

int axle_dep_parsever(axle_dependency *dep, const char *version);
int axle_dep_check(const axle_dependency *dep, const char *version);
int axle_dep_inject(axle_dependency *dep, axle_sources *sources, const char *req_version, const char *module_path, const char *real_path);

#endif
