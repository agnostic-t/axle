#include <axle/building.h>

axle_target release = (axle_target){
    .optimize = O3,
    .flags    = "-fsanitize=address",
    .defines  = (strlist){
        "MODE=RELEASE", 0
    }
};

axle_sources  source;
axle_output   output;
axle_metadata md;
