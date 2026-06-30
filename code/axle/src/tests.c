#include "axle/tests.h"

#include "axle/building.h"
#include "axle/cleanup.h"
#include "axle/colors.h"
#include "axle/path_utils.h"
#include "axle/project.h"
#include "axle/settings.h"
#include "axle/string_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <yyjson.h>

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */

/* Read the `testsPath` field from <project_root>/module.json.
 * Returns a malloc'd string (caller frees) or NULL on error / missing. */
static char *axle_tests_read_path(const char *project_root) {
  char *mod_json = uax_path_concat(project_root, "module.json");
  if (!mod_json)
    return NULL;

  yyjson_read_err err;
  yyjson_doc *doc = yyjson_read_file(mod_json, 0, NULL, &err);
  free(mod_json);
  if (!doc) {
    fprintf(stderr, "%s[axle][tests] failed to read module.json: %s%s\n",
            xfore.red, err.msg, xfore.normal);
    return NULL;
  }

  yyjson_val *root = yyjson_doc_get_root(doc);
  const char *tests_path = yyjson_get_str(yyjson_obj_get(root, "testsPath"));

  char *result = tests_path ? strdup(tests_path) : NULL;
  yyjson_doc_free(doc);
  return result;
}

/* Extract the test name (filename without .c / .cpp) from a path.
 * "tests/foo.c"           -> "foo"
 * "tests/sub/bar.cpp"     -> "bar"
 * Returns a malloc'd string. */
static char *axle_tests_name_from_file(const char *file_path) {
  const char *filename = uax_path_filename(file_path);
  if (!filename)
    return NULL;

  size_t len = strlen(filename);
  if (len > 4 && strcmp(filename + len - 4, ".cpp") == 0) {
    char *out = malloc(len - 4 + 1);
    if (!out)
      return NULL;
    memcpy(out, filename, len - 4);
    out[len - 4] = '\0';
    return out;
  }
  if (len > 2 && strcmp(filename + len - 2, ".c") == 0) {
    char *out = malloc(len - 2 + 1);
    if (!out)
      return NULL;
    memcpy(out, filename, len - 2);
    out[len - 2] = '\0';
    return out;
  }
  return strdup(filename);
}

static bool axle_tests_name_matches(const char *file_path,
                                    const char *test_name) {
  char *name = axle_tests_name_from_file(file_path);
  if (!name)
    return false;
  bool ok = (strcmp(name, test_name) == 0);
  free(name);
  return ok;
}

/* In-place: prepend `prefix/` to every entry of a NULL-terminated string list.
 */
static void axle_tests_prepend_to_strlist(const char ***list,
                                          const char *prefix) {
  if (!list || !*list || !prefix)
    return;
  for (size_t i = 0; (*list)[i] != NULL; i++) {
    char *new_path = uax_path_concat(prefix, (*list)[i]);
    free((void *)(*list)[i]);
    (*list)[i] = new_path;
  }
}

/* Replace receipt->sources.sources with a fresh single-element list [file]. */
static int axle_tests_set_single_source(axle_receipt *recp, const char *file) {
  if (recp->sources.sources) {
    uax_free_strlist_ne((char ***)&recp->sources.sources);
    recp->sources.sources = NULL;
  }
  const char **new_sources = calloc(2, sizeof(char *));
  if (!new_sources)
    return -1;
  new_sources[0] = strdup(file);
  new_sources[1] = NULL;
  if (!new_sources[0]) {
    free(new_sources);
    return -1;
  }
  recp->sources.sources = new_sources;
  return 0;
}

/* Set receipt->output.path to "<tests_path_rel>/bin/<test_name>".
 * Any previous output.path is freed. */
static int axle_tests_set_output(axle_receipt *recp, const char *tests_path_rel,
                                 const char *test_name) {
  if (recp->output.path) {
    free((char *)recp->output.path);
    recp->output.path = NULL;
  }
  char *bin_dir = uax_path_concat(tests_path_rel, "bin");
  if (!bin_dir)
    return -1;
  char *output_path = uax_path_concat(bin_dir, test_name);
  free(bin_dir);
  if (!output_path)
    return -1;
  recp->output.path = output_path;
  return 0;
}

/* ------------------------------------------------------------------ */
/* Main entry point                                                    */
/* ------------------------------------------------------------------ */

int axle_tests_run(const char *project_root, const char *test_name,
                   bool only_build) {
  bool silent = true;

  if (!project_root) {
    fprintf(stderr, "%s[axle][tests] project_root is required%s\n", xfore.red,
            xfore.normal);
    return -1;
  }

  int result = 0;

  /* ---------- 1. locate testsPath ---------- */
  char *tests_path_rel = axle_tests_read_path(project_root);
  if (!tests_path_rel) {
    fprintf(stderr,
            "%s[axle][tests] no 'testsPath' field in %s/module.json%s\n",
            xfore.red, project_root, xfore.normal);
    return -1;
  }

  char *tests_full_path = uax_path_concat(project_root, tests_path_rel);
  printf("[axle][tests] tests directory: %s\n", tests_full_path);

  struct stat st;
  if (stat(tests_full_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
    fprintf(stderr, "%s[axle][tests] tests directory does not exist: %s%s\n",
            xfore.red, tests_full_path, xfore.normal);
    free(tests_path_rel);
    free(tests_full_path);
    return -1;
  }

  /* ---------- 2. load <testsPath>/module.json ---------- */
  char *tests_mod_json = uax_path_concat(tests_full_path, "module.json");
  axle_receipt test_recp;
  memset(&test_recp, 0, sizeof(test_recp));

  if (0 > axle_project_prepare(tests_mod_json, NULL, &test_recp)) {
    fprintf(stderr, "%s[axle][tests] failed to prepare test module receipt%s\n",
            xfore.red, xfore.normal);
    free(tests_mod_json);
    free(tests_path_rel);
    free(tests_full_path);
    return -1;
  }
  free(tests_mod_json);

  /* ---------- 3. rebase test's own paths onto project_root ----------
   *
   * We're going to call axle_build() with base_path = project_root, so
   * that dependency paths produced by axle_modules_prepare() — which are
   * of the form ".modules/<dep>/include" — resolve correctly against
   * project_root. As a consequence, every path declared inside the
   * tests module.json (sources / includes / lib-dirs / objects_path)
   * must be rewritten to be relative to project_root, i.e. prefixed
   * with testsPath/.
   */
  axle_tests_prepend_to_strlist(&test_recp.sources.sources, tests_path_rel);
  axle_tests_prepend_to_strlist(&test_recp.sources.incl_dirs, tests_path_rel);
  axle_tests_prepend_to_strlist(&test_recp.sources.lib_dirs, tests_path_rel);

  if (test_recp.output.obj_path) {
    char *new_obj = uax_path_concat(tests_path_rel, test_recp.output.obj_path);
    free((char *)test_recp.output.obj_path);
    test_recp.output.obj_path = new_obj;
  } else {
    test_recp.output.obj_path = uax_path_concat(tests_path_rel, "objs");
  }

  /* ---------- 4. resolve dependencies against <project_root>/.modules
   * ---------- */
  axle_receipt *dep_receipts = NULL;
  size_t dep_n = 0;

  printf("[axle][tests] resolving dependencies...\n");
  if (0 > axle_modules_prepare(NULL, &test_recp, project_root, &dep_receipts,
                               &dep_n)) {
    fprintf(stderr, "%s[axle][tests] failed to resolve dependencies%s\n",
            xfore.red, xfore.normal);
    clean_axle_receipt(&test_recp);
    free(tests_path_rel);
    free(tests_full_path);
    return -1;
  }

  /* ---------- 5. make sure every dependency is actually built ----------
   *
   * axle_build() with rebuild=false is essentially a no-op when the
   * artefacts are already up-to-date, so this is cheap when the user
   * has already run `axle build`. If they haven't, we make sure the
   * .o / .so / .a files the tests will link against actually exist.
   */
  // for (size_t i = 0; i < dep_n; i++) {
  //   const char *dep_name = dep_receipts[i].metadata.name;
  //   if (!dep_name)
  //     continue;

  //   char *dep_base = uax_path_concat(project_root, ".modules");
  //   char *dep_path = uax_path_concat(dep_base, dep_name);
  //   free(dep_base);

  //   printf("[axle][tests] ensuring dependency is built: %s\n", dep_name);
  //   if (0 > axle_build(&dep_receipts[i], dep_path, true, false, silent)) {
  //     fprintf(stderr,
  //             "%s[axle][tests] warning: failed to build dependency %s "
  //             "(will try to continue)%s\n",
  //             xfore.yellow, dep_name, xfore.normal);
  //   }
  //   free(dep_path);
  // }

  /* ---------- 6. discover test files ---------- */
  char **test_files = NULL;
  size_t test_files_n = 0;
  if (test_recp.sources.sources) {
    uax_expand_files(project_root, test_recp.sources.sources, &test_files,
                     &test_files_n);
  }

  if (test_files_n == 0) {
    fprintf(stderr, "%s[axle][tests] no test files found in %s%s\n", xfore.red,
            tests_full_path, xfore.normal);
    result = -1;
    goto cleanup;
  }

  printf("[axle][tests] found %zu test file(s)\n", test_files_n);

  /* ---------- 7. optionally filter to a single test ---------- */
  if (test_name) {
    const char *matched = NULL;
    for (size_t i = 0; i < test_files_n; i++) {
      if (axle_tests_name_matches(test_files[i], test_name)) {
        matched = test_files[i];
        break;
      }
    }

    if (!matched) {
      fprintf(stderr, "%s[axle][tests] test '%s' not found.%s\n", xfore.red,
              test_name, xfore.normal);
      fprintf(stderr, "%s[axle][tests] available tests:%s\n", xfore.yellow,
              xfore.normal);
      for (size_t i = 0; i < test_files_n; i++) {
        char *n = axle_tests_name_from_file(test_files[i]);
        fprintf(stderr, "  - %s  (%s)\n", n, test_files[i]);
        free(n);
      }
      result = -1;
      goto cleanup;
    }

    /* keep only the matched entry */
    char *kept = strdup(matched);
    uax_free_strlist(&test_files, &test_files_n);
    uax_strlist_extend(&test_files, &test_files_n, kept);
    free(kept);

    printf("[axle][tests] selected test: %s\n", test_name);
  }

  /* ---------- 8. build (+ run) each test ---------- */
  int build_failures = 0;
  int run_failures = 0;
  char **built_bins = NULL;
  size_t built_n = 0;

  for (size_t i = 0; i < test_files_n; i++) {
    const char *test_file = test_files[i];
    char *this_name = axle_tests_name_from_file(test_file);
    if (!this_name) {
      fprintf(stderr, "%s[axle][tests] failed to extract test name from %s%s\n",
              xfore.red, test_file, xfore.normal);
      build_failures++;
      continue;
    }

    printf("\n[axle][tests] %s========== [%zu/%zu] %s ==========%s\n",
           xfore.cyan, i + 1, test_files_n, this_name, xfore.normal);

    /* swap sources for just this one file */
    if (0 > axle_tests_set_single_source(&test_recp, test_file)) {
      fprintf(stderr, "%s[axle][tests] failed to set source for %s%s\n",
              xfore.red, this_name, xfore.normal);
      free(this_name);
      build_failures++;
      continue;
    }

    /* swap output.path to testsPath/bin/<name> */
    if (0 > axle_tests_set_output(&test_recp, tests_path_rel, this_name)) {
      fprintf(stderr, "%s[axle][tests] failed to set output for %s%s\n",
              xfore.red, this_name, xfore.normal);
      free(this_name);
      build_failures++;
      continue;
    }

    /* build (rebuild=true so changing the single source always wins
     * over any stale .o from a previous test of the same project) */
    if (0 > axle_build(&test_recp, project_root, true, true, silent)) {
      fprintf(stderr, "%s[axle][tests] build FAILED for %s%s\n", xfore.red,
              this_name, xfore.normal);
      free(this_name);
      build_failures++;
      continue;
    }

    printf("[axle][tests] %sbuilt%s %s\n", xfore.green, xfore.normal,
           this_name);

    if (only_build) {
      free(this_name);
      continue;
    }

    /* run */
    char *bin_path = uax_path_concat(project_root, test_recp.output.path);
    printf("[axle][tests] %srunning%s %s\n", xfore.cyan, xfore.normal,
           bin_path);

    int ret = system(bin_path);
    if (ret != 0) {
      fprintf(stderr, "%s[axle][tests] test %s FAILED (exit code %d)%s\n",
              xfore.red, this_name, ret, xfore.normal);
      run_failures++;
    } else {
      printf("[axle][tests] %sPASSED%s %s\n", xfore.green, xfore.normal,
             this_name);
    }

    /* remember the binary so we can wipe it after every test has run */
    uax_strlist_extend(&built_bins, &built_n, bin_path);
    free(bin_path);
    free(this_name);
  }

  /* ---------- 9. delete binaries (only when we actually ran them) ----------
   */
  if (!only_build && built_n > 0) {
    printf("\n[axle][tests] cleaning up %zu binary(ies)...\n", built_n);
    for (size_t i = 0; i < built_n; i++) {
      if (unlink(built_bins[i]) != 0) {
        fprintf(stderr, "%s[axle][tests] warning: failed to delete %s%s\n",
                xfore.yellow, built_bins[i], xfore.normal);
      }
    }
  }

  /* ---------- 10. summary ---------- */
  printf("\n[axle][tests] %s========== SUMMARY ==========%s\n", xfore.cyan,
         xfore.normal);
  printf("[axle][tests] total: %zu, built ok: %zu, build failures: %d",
         test_files_n, test_files_n - build_failures, build_failures);
  if (!only_build) {
    printf(", run failures: %d", run_failures);
  }
  printf("\n");

  if (build_failures > 0 || run_failures > 0) {
    result = -1;
  }

cleanup:
  if (built_bins)
    uax_free_strlist(&built_bins, &built_n);
  if (test_files)
    uax_free_strlist(&test_files, &test_files_n);

  if (dep_receipts) {
    for (size_t i = 0; i < dep_n; i++) {
      clean_axle_receipt(&dep_receipts[i]);
    }
    free(dep_receipts);
  }

  clean_axle_receipt(&test_recp);

  free(tests_path_rel);
  free(tests_full_path);

  return result;
}
