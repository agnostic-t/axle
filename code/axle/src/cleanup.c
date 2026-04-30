#include "axle/string_utils.h"
#include <axle/cleanup.h>
#include <stdlib.h>

void clean_axle_metadata(axle_metadata *md){
    if (!md) return;
    if (md->name) free((char*)md->name);
    if (md->version) free((char*)md->version);
}

void clean_axle_target(axle_target *targ){
    if (!targ) return;
    if (targ->flags) free((char*)targ->flags);
    if (targ->defines) uax_free_strlist_ne((char***)&targ->defines);
}

void clean_axle_sources(axle_sources *src){
    if (!src) return;
    if (src->sources) uax_free_strlist_ne((char***)&src->sources);
    if (src->lib_dirs) uax_free_strlist_ne((char***)&src->lib_dirs);
    if (src->incl_dirs) uax_free_strlist_ne((char***)&src->incl_dirs);
    if (src->libs) uax_free_strlist_ne((char***)&src->libs);
    if (src->pkgs) uax_free_strlist_ne((char***)&src->pkgs);
}

void clean_axle_output(axle_output *out){
    if (!out) return;
    if (out->obj_path) free((char*)out->obj_path);
    if (out->path) free((char*)out->path);
}

void clean_axle_receipt(axle_receipt *recp){
    if (!recp) return;
    if (recp->compiler) free((char*)recp->compiler);
    clean_axle_target(&recp->target);
    clean_axle_sources(&recp->sources);
    clean_axle_output(&recp->output);
    clean_axle_metadata(&recp->metadata);
}
