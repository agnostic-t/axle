#include "axle/building.h"
#include "axle/cleanup.h"
#include "axle/path_utils.h"
#include "axle/project.h"
#include "axle/settings.h"
#include "axle/colors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int build(const char *path, bool rebuild){
    printf("%s[AXLE] build system v0.0.1%s\n", xfore.magenta, xfore.normal);

    char *target_file = NULL;
    struct stat s;
    if (stat(path, &s) == 0 && S_ISDIR(s.st_mode)) {
        target_file = uax_path_concat(path, "module.json");
    } else {
        target_file = strdup(path);
    }

    char *directory = uax_path_get_dir(target_file);

    axle_receipt recp;
    if (0 > axle_project_prepare(target_file, NULL, &recp)){
        fprintf(stderr, "%s[axle]%s failed to prepare reciept\n", xfore.red, xfore.normal);
        free(target_file);
        free(directory);
        return -1;
    }

    free(target_file);

    axle_receipt *mod_receipts = NULL;
    size_t        mods_n = 0;

    struct stat st;

    char *main_defaults = uax_path_concat(directory, "defaults.json");
    if (stat(main_defaults, &st) != 0) {
        free(main_defaults);
        main_defaults = NULL;
    }

    if (0 > axle_modules_prepare(main_defaults, &recp, directory, &mod_receipts, &mods_n)){
        fprintf(stderr, "%s[axle]%s failed to prepare modules\n", xfore.red, xfore.normal);
        clean_axle_receipt(&recp);
        free(directory);
        return -1;
    }

    if (main_defaults) free(main_defaults);

    printf("%s[axle] prepared%s everything\n", xfore.green, xfore.normal);

    int success = 1;
    char *mods_base = uax_path_concat(directory, ".modules");
    for (size_t i = 0; i < mods_n; i++){
        char *mod_base_path = uax_path_concat(mods_base, mod_receipts[i].metadata.name);

        printf("%s[axle]%s[%s%zu%s/%zu] building deps: %s%s%s\n", xfore.cyan, xfore.normal, xfore.green, i + 1, xfore.normal, mods_n, xfore.magenta, mod_receipts[i].metadata.name, xfore.normal);
        if (0 > axle_build(&mod_receipts[i], mod_base_path, true, rebuild)){
            fprintf(stderr, "%s[axle][%zu/%zu]%s build failed\n", xfore.red, i + 1, mods_n, xfore.normal);

            success = 0;
            free(mod_base_path);
            break;
        }

        free(mod_base_path);
    }
    free(mods_base);

    if (!success)
        goto fail;

    printf("%s[axle] deps all built%s\n", xfore.green, xfore.normal);

    if (recp.only_deps){
        printf("%s[axle] build done%s only dependencies were built\n", xfore.green, xfore.normal);
        free(directory);
        clean_axle_receipt(&recp);

        for (size_t i = 0; i < mods_n; i++) clean_axle_receipt(&mod_receipts[i]);
        free(mod_receipts);

        return 0;
    }

    int ret = 0;
    if (0 > axle_build(&recp, directory, true, rebuild)){
        fprintf(stderr, "%s[axle] failed%s to build main module\n", xfore.red, xfore.normal);
        ret = -1;
    } else {
        printf("%s[axle] build done%s without errors\n", xfore.green, xfore.normal);
    }

    free(directory);
    clean_axle_receipt(&recp);

    for (size_t i = 0; i < mods_n; i++) clean_axle_receipt(&mod_receipts[i]);
    free(mod_receipts);

    return ret;

fail:
    clean_axle_receipt(&recp);
    free(directory);

    for (size_t i = 0; i < mods_n; i++) clean_axle_receipt(&mod_receipts[i]);
    free(mod_receipts);

    return -1;
}


const char *template =
"{\n"
"  \"metadata\": {\n"
"    \"name\": \"%s\",\n"
"    \"version\": \"0.0.0\"\n"
"  },\n\n"
"  \"code\": {\n"
"    \"sources\": [\"**/*.c\"],\n"
"    \"output\": \"./bin/%s\",\n"
"    \"includes\": [\"include\"]\n"
"  },\n\n"
"  \"target\": \"debug\"\n"
"}";

int main(int argc, const char *argv[]) {
    const char *path = "./test/code/module.json";

    if (argc == 1 || strcmp(argv[1], "help") == 0){
        fprintf(
            stderr,
            "axle - build system\n"
            "version: 0.0.1\n"
            "\tusage: %s build|module|help PATH|NAME [--clean]\n\n"
            "--clean - works with build, rebuilds all files\n\n"
            "build:PATH - path to directory with project (module.json file)\n"
            "module:NAME - name of the module. Will be place in ./.modules/<name>\n"
            "help - print this help message and exit\n",
            argv[0]
        );

        return -1;
    }

    if (strcmp("build", argv[1]) == 0) {

        if (argc < 3) {
            fprintf(stderr, "[axle] invalid usage, no path provided\n");
            return -1;
        }

        if (argc >= 4 && strcmp(argv[3], "--clean") == 0){
            build(argv[2], true);
        } else {
            build(argv[2], false);
        }

    } else if (strcmp("module", argv[1]) == 0) {
        if (argc != 3){
            fprintf(stderr, "[axle] invalid usage, no name provided\n");
            return -1;
        }

        mkdir("./.modules", 0755);

        char *mod_dir = uax_path_concat("./.modules", argv[2]);
        mkdir(mod_dir, 0755);

        char *incl_dir = uax_path_concat(mod_dir, "include");
        char *bin_dir  = uax_path_concat(mod_dir, "bin");
        char *src_dir  = uax_path_concat(mod_dir, "src");

        mkdir(incl_dir, 0755); free(incl_dir);
        mkdir(bin_dir, 0755); free(bin_dir);
        mkdir(src_dir, 0755); free(src_dir);

        char *mod_json_file = uax_path_concat(mod_dir, "module.json");

        printf("[axle] made all directories\n");
        FILE *module_json = fopen(mod_json_file, "w");
        fprintf(module_json, template, argv[2], argv[2]);
        fclose(module_json);

        free(mod_json_file);
        free(mod_dir);
        printf("[axle] filled template\n");
    } else {
        fprintf(stderr, "[axle] invalid usage, consider running %s help\n", argv[0]);
        return -1;
    }
}
