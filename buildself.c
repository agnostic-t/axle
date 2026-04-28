#include <axle/building.h>
#include <stdlib.h>

int main(){
    axle_metadata md = (axle_metadata){
        .name     = "AXLE build system",
        .version  = "v1.0.0-dev",
        .priority = 1000
    };

    axle_target release = (axle_target){
        .optimize = O3,
        .flags    = "-fsanitize=address",
        .defines  = 0
    };

    axle_sources source = (axle_sources){
        .sources = (strlist){
            "buildself.c",
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

    axle_output output = (axle_output){
        .type = EXECUTABLE,
        .path = "./bin/axle",
        .obj_path = "./objs"
    };

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
