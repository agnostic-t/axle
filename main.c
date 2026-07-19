#include "axle/building.h"
#include "axle/cleanup.h"
#include "axle/colors.h"
#include "axle/dependencies.h"
#include "axle/path_utils.h"
#include "axle/project.h"
#include "axle/settings.h"
#include "axle/tests.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int build(const char *path, bool rebuild, bool silent, bool force_update) {
  printf("%s[AXLE] build system v0.0.1%s\n", xfore.magenta, xfore.normal);

  char *target_file = NULL;
  struct stat s;
  if (stat(path, &s) == 0 && S_ISDIR(s.st_mode)) {
    target_file = uax_path_concat(path, "module.json");
  } else {
    target_file = strdup(path);
  }

  char *directory = uax_path_get_dir(target_file);

  /* Resolve the top-level project's defaults.json (if any) BEFORE preparing
   * the main receipt, so we can pass it as `adt_defaults` to
   * `axle_project_prepare`. It is then also reused:
   *   - as `inherited_defaults` for every top-level remote_dep (so sub-deps
   *     inherit our compiler / linker if they don't have their own);
   *   - as `main_defaults` for `axle_modules_prepare` (so local sub-modules
   *     in .modules/ inherit our defaults too). */
  struct stat st;
  char *main_defaults = uax_path_concat(directory, "defaults.json");
  if (stat(main_defaults, &st) != 0) {
    free(main_defaults);
    main_defaults = NULL;
  }

  axle_receipt recp;
  if (0 > axle_project_prepare(target_file, main_defaults, directory, &recp)) {
    fprintf(stderr, "%s[axle]%s failed to prepare reciept\n", xfore.red,
            xfore.normal);
    free(target_file);
    if (main_defaults) free(main_defaults);
    free(directory);
    return -1;
  }

  free(target_file);

  /* ----- install + build all remote_deps BEFORE preparing modules -----
   * This is required because the local `dependencies` section of module.json
   * may reference modules that only become visible after a remote_dep is
   * installed (via the symlinks created in .modules/<export_name>). */
  for (size_t i = 0; i < recp.remote_deps_n; i++) {
    const axle_remote_dep *rd = &recp.remote_deps[i];

    printf("%s[axle][remote_deps]%s [%zu/%zu] %s (%s)\n",
           xfore.cyan, xfore.normal, i + 1, recp.remote_deps_n,
           rd->repo_name, rd->url);

    if (0 > axle_dep_download(rd->url, rd->version_req,
                              directory, rd->repo_name, force_update)) {
      fprintf(stderr, "%s[axle] failed to download remote_dep '%s'%s\n",
              xfore.red, rd->repo_name, xfore.normal);
      if (main_defaults) free(main_defaults);
      clean_axle_receipt(&recp);
      free(directory);
      return -1;
    }

    if (0 > axle_dep_install(directory, rd->repo_name, rd->version_req)) {
      fprintf(stderr, "%s[axle] failed to install remote_dep '%s'%s\n",
              xfore.red, rd->repo_name, xfore.normal);
      if (main_defaults) free(main_defaults);
      clean_axle_receipt(&recp);
      free(directory);
      return -1;
    }

    if (0 > axle_dep_build(directory, rd->repo_name, main_defaults,
                            false, silent, 0)) {
        fprintf(stderr, "%s[axle] failed to build remote_dep '%s'%s\n",
                xfore.red, rd->repo_name, xfore.normal);
        if (main_defaults) free(main_defaults);
        clean_axle_receipt(&recp);
        free(directory);
        return -1;
    }
  }

  if (recp.remote_deps_n > 0) {
    printf("%s[axle] remote_deps all built%s\n", xfore.green, xfore.normal);
  }

  axle_receipt *mod_receipts = NULL;
  size_t mods_n = 0;

  if (0 > axle_modules_prepare(main_defaults, &recp, directory, &mod_receipts,
                               &mods_n)) {
    fprintf(stderr, "%s[axle]%s failed to prepare modules\n", xfore.red,
            xfore.normal);
    if (main_defaults) free(main_defaults);
    clean_axle_receipt(&recp);
    free(directory);
    return -1;
  }

  if (main_defaults)
    free(main_defaults);

  printf("%s[axle] prepared%s everything\n", xfore.green, xfore.normal);

  int success = 1;
  char *mods_base = uax_path_concat(directory, ".modules");
  for (size_t i = 0; i < mods_n; i++) {
    char *mod_base_path =
        uax_path_concat(mods_base, mod_receipts[i].metadata.name);

    /* If this module entry is a symlink, it points into `.vendor/` — which
     * means it's a remote_dep we already built via `axle_dep_build` above.
     * Skip the redundant rebuild; the symlinks/artefacts are already in
     * place and `axle_modules_prepare` has already merged its incl_dirs /
     * lib_dirs / libs into the main receipt via `axle_receipt_merge`. */
    struct stat mod_stat;
    int is_symlink = (lstat(mod_base_path, &mod_stat) == 0 &&
                      S_ISLNK(mod_stat.st_mode));

    printf("%s[axle]%s[%s%zu%s/%zu] building deps: %s%s%s\n", xfore.cyan,
           xfore.normal, xfore.green, i + 1, xfore.normal, mods_n,
           xfore.magenta, mod_receipts[i].metadata.name, xfore.normal);
    if (is_symlink) {
      printf("%s[axle]%s %sskipping%s (already built as remote_dep)\n",
             xfore.cyan, xfore.normal, xfore.magenta, xfore.normal);
      free(mod_base_path);
      continue;
    }

    if (0 >
        axle_build(&mod_receipts[i], mod_base_path, true, rebuild, silent)) {
      fprintf(stderr, "%s[axle][%zu/%zu]%s build failed\n", xfore.red, i + 1,
              mods_n, xfore.normal);

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

  if (recp.only_deps) {
    printf("%s[axle] build done%s only dependencies were built\n", xfore.green,
           xfore.normal);
    free(directory);
    clean_axle_receipt(&recp);

    for (size_t i = 0; i < mods_n; i++)
      clean_axle_receipt(&mod_receipts[i]);
    free(mod_receipts);

    return 0;
  }

  int ret = 0;
  if (0 > axle_build(&recp, directory, true, rebuild, silent)) {
    fprintf(stderr, "%s[axle] failed%s to build main module\n", xfore.red,
            xfore.normal);
    ret = -1;
  } else {
    printf("%s[axle] build done%s without errors\n", xfore.green, xfore.normal);
  }

  free(directory);
  clean_axle_receipt(&recp);

  for (size_t i = 0; i < mods_n; i++)
    clean_axle_receipt(&mod_receipts[i]);
  free(mod_receipts);

  return ret;

fail:
  clean_axle_receipt(&recp);
  free(directory);

  for (size_t i = 0; i < mods_n; i++)
    clean_axle_receipt(&mod_receipts[i]);
  free(mod_receipts);

  return -1;
}

const char *template = "{\n"
                       "  \"metadata\": {\n"
                       "    \"name\": \"%s\",\n"
                       "    \"version\": \"0.0.0\",\n"
                       "    \"priority\": 100\n"
                       "  },\n\n"
                       "  \"compiler\": {\n"
                       "    \"binary\": \"static-lib\"\n"
                       "  },\n\n"
                       "  \"code\": {\n"
                       "    \"sources\": [\"**/*.c\"],\n"
                       "    \"output\": \"./bin/%s\",\n"
                       "    \"includes\": [\"include\"]\n"
                       "  },\n\n"
                       "  \"target\": \"debug\"\n"
                       "}";

static void print_help(const char *argv0) {
  fprintf(
      stderr,
      "axle - build system\n"
      "version: 0.0.1\n"
      "\tusage: %s build|module|test|help PATH|NAME [flags...]\n\n"
      "build flags:\n"
      "  --clean   rebuilds all .o files (forces full recompilation)\n"
      "  --update  forces `git pull` for every remote_dep before building\n"
      "\n"
      "build:PATH - path to directory with project (module.json file)\n"
      "module:NAME - name of the module. Will be place in ./.modules/<name>\n"
      "help - print this help message and exit\n\n"
      "Tests:\naxle test [name of the test] [--only build]\n",
      argv0);
}

int main(int argc, const char *argv[]) {
  const char *path = "./test/code/module.json";

  if (argc == 1 || strcmp(argv[1], "help") == 0) {
    print_help(argv[0]);
    return -1;
  }

  if (strcmp("build", argv[1]) == 0) {

    if (argc < 3) {
      fprintf(stderr, "[axle] invalid usage, no path provided\n");
      return -1;
    }

    bool rebuild = false;
    bool force_update = false;
    for (int i = 3; i < argc; i++) {
      if (strcmp(argv[i], "--clean") == 0) {
        rebuild = true;
      } else if (strcmp(argv[i], "--update") == 0) {
        force_update = true;
      } else {
        fprintf(stderr, "%s[axle] unknown flag: %s%s\n",
                xfore.red, argv[i], xfore.normal);
        return -1;
      }
    }
    return build(argv[2], rebuild, false, force_update) == 0 ? 0 : 1;

  } else if (strcmp("module", argv[1]) == 0) {
    if (argc != 3) {
      fprintf(stderr, "[axle] invalid usage, no name provided\n");
      return -1;
    }

    mkdir("./.modules", 0755);

    char *mod_dir = uax_path_concat("./.modules", argv[2]);
    mkdir(mod_dir, 0755);

    char *incl_dir = uax_path_concat(mod_dir, "include");
    char *bin_dir = uax_path_concat(mod_dir, "bin");
    char *src_dir = uax_path_concat(mod_dir, "src");

    mkdir(incl_dir, 0755);
    free(incl_dir);
    mkdir(bin_dir, 0755);
    free(bin_dir);
    mkdir(src_dir, 0755);
    free(src_dir);

    char *mod_json_file = uax_path_concat(mod_dir, "module.json");

    printf("[axle] made all directories\n");
    FILE *module_json = fopen(mod_json_file, "w");
    fprintf(module_json, template, argv[2], argv[2]);
    fclose(module_json);

    free(mod_json_file);
    free(mod_dir);
    printf("[axle] filled template\n");
  } else if (strcmp(argv[1], "test") == 0) {
    const char *test_name = NULL;
    bool only_build = false;

    for (int i = 2; i < argc; i++) {
      if (strcmp(argv[i], "--only-build") == 0) {
        only_build = true;
      } else if (argv[i][0] == '-') {
        fprintf(stderr, "%s[axle] unknown flag: %s%s\n", xfore.red, argv[i],
                xfore.normal);
        return 1;
      } else {
        if (test_name) {
          fprintf(stderr, "%s[axle] only one test name may be specified%s\n",
                  xfore.red, xfore.normal);
          return 1;
        }
        test_name = argv[i];
      }
    }

    /* `axle test` is always run from the project root, so "." is
     * the project root. If you ever need to support running it from
     * elsewhere, replace "." with an absolute path. */
    return axle_tests_run(".", test_name, only_build) == 0 ? 0 : 1;
  } else {
    fprintf(stderr, "[axle] invalid usage, consider running %s help\n",
            argv[0]);
    return -1;
  }

  return 0;
}
