#ifndef AXLE_PROJECT_H
#define AXLE_PROJECT_H

#include "axle/settings.h"

int axle_project_prepare(const char *module_json_path, const char *adt_defaults,
                         const char *project_root, axle_receipt *outrecp);
int axle_modules_prepare(const char *defaults, axle_receipt *main_receipt,
                         const char *directory, axle_receipt **out_receipts,
                         size_t *recp_n);

#endif
