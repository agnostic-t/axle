#include "axle/string_utils.h"
#include "axle/types.h"
#include "axle/settings.h"
#include "axle/building.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>


const char *boilerplate =
"#include \"build.c\"\n"
"int main(){\n"
"    build();\n"
"    int r = perform();\n"
"    if (r < 0) {\n"
"        fprintf(stderr, \"[axle][end] build is failed\\n\");\n"
"    } else {\n"
"        fprintf(stderr, \"[axle][end] build is successfull\\n\");\n"
"    }\n"
"    return 0;\n"
"}"
;

void usage(const char *prog){
    fprintf(stderr, "axle - C/C++ build system\n");
    fprintf(stderr, "usage: "
                    "%s build PATH\n\n", prog);
    const char *usage =
        "PATH - path to directory with build.c file"
    ;

    fprintf(stderr, "%s\n", usage);
}

int main(int argc, char *argv[]){
    if (argc == 1 ||
        strcmp(argv[1], "-h") == 0 ||
        strcmp(argv[1], "--help") == 0 ||
        strcmp(argv[1], "help") == 0
    ) {
        usage(argv[0]);
        return 1;
    }

    srand(time(NULL));
    uint32_t token = rand() % 14353;
    uint32_t token2 = rand() % 14353;

    if (strcmp(argv[1], "build") == 0){
        if (argc != 3) {
            fprintf(stderr, "[axle] missing path with build script, specify derictory\n");
            return 1;
        }

        printf("[axle] entering build system mode\n");

        char *dir = NULL;
        uax_ip_strextend(&dir, argv[2]);
        if (argv[2][strlen(argv[2]) - 1] != '/')
            uax_ip_strextend(&dir, "/");

        char *path = NULL;
        uax_ip_strextend(&path, argv[2]);
        if (argv[2][strlen(argv[2]) - 1] != '/')
            uax_ip_strextend(&path, "/");
        uax_ip_strextend(&path, "build.c");

        fprintf(stderr, "[axle] path for build.c: %s\n", path);

        struct stat st;
        if (0 > stat(path, &st)){
            fprintf(stderr, "[axle] build.c was not found\n");
            free(path);
            free(dir);

            return 1;
        }
        free(path);

        char bplate_file[100];
        strcpy(bplate_file, dir);
        strcpy(bplate_file + strlen(bplate_file), ".axle_gen");
        sprintf(bplate_file + strlen(bplate_file), "%d.c", token);
        printf("[axle] boilerplate file: %s\n", bplate_file);
        free(dir);

        FILE *bpl = fopen(bplate_file, "w");
        if (!bpl) {
            fprintf(stderr, "[axle] failed to write data to boilerplate file\n");
            return -1;
        }

        fprintf(bpl, "%s\n", boilerplate);
        fclose(bpl);

        char *cmd = NULL;
        uax_ip_strextend(&cmd, "cd ");
        uax_ip_strextend(&cmd, argv[2]);
        uax_ip_strextend(&cmd, " && gcc -o ");

        char exec_file[100];
        strcpy(exec_file, "/tmp/axle_execfile");
        sprintf(exec_file + strlen(exec_file), "%d", token);
        uax_ip_strextend(&cmd, exec_file);
        uax_ip_strextend(&cmd, " ");
        uax_ip_strextend(&cmd, bplate_file);

        printf("[axle] command: %s\n", cmd);
        free(cmd);

        return 0;
    } else {
        fprintf(stderr, "[axle] unknown command \"%s\", use %s help to get usage\n", argv[1], argv[0]);
        return 1;
    }

    printf("[axle] exit\n");
    return 0;
}
