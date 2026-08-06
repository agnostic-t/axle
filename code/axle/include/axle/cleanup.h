#ifndef AXLE_CLEANUP_H
#define AXLE_CLEANUP_H

#include "settings.h"

void clean_axle_metadata(axle_metadata *md);
void clean_axle_target(axle_target *targ);
void clean_axle_sources(axle_sources *src);
void clean_axle_hooks(axle_hooks *hooks);
void clean_axle_output(axle_output *out);
void clean_axle_receipt(axle_receipt *recp);

#endif
