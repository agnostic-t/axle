#include <axle/building.h>
#include <stdlib.h>
#include "defaults.c"

void build(){
    md = (axle_metadata){
        .name     = "My First AXLE project",
        .version  = "v1.0.0-dev",
        .priority = 1000
    };

    source = (axle_sources){
        .sources = (strlist){
            "main.c",
            "code/axle/src/building.c",
            "code/axle/src/paths.c",
            "code/axle/src/strings.c", 0
        },
        .incl_dirs = (strlist){
            "code/axle/include", 0
        },
        .lib_dirs = 0,
        .libs = 0,
        .pkgs = 0
    };

    output = (axle_output){
        .type = EXECUTABLE,
        .path = "./bin/main",
        .obj_path = "./objs"
    };
}

int perform(){
    axle_receipt receipt = {
        .compiler = "gcc",
        .target   = release,
        .metadata = md,
        .sources  = source,
        .output   = output
    };

    system("mkdir -p objs");
    return axle_build(&receipt, false);
}
