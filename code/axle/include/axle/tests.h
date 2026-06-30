#ifndef AXLE_TESTS_H
#define AXLE_TESTS_H

#include <stdbool.h>

/**
* Build (and optionally run) project tests.
*
* The tests directory is read from the `testsPath` field of the root
* `<project_root>/module.json`. Inside that directory there must be another
* `module.json` that describes how tests are built (compiler, includes,
* dependencies, defaults, ...). Each file matching the `code.sources` glob
* patterns inside that tests module is treated as one standalone test —
* it is compiled alone and linked into `testsPath/bin/<filename-without-ext>`.
*
* Dependencies declared in the tests module.json are resolved against
* `<project_root>/.modules` (NOT `<testsPath>/.modules`), exactly like a
* regular module build.
*
* @param project_root  Path to the project root (the folder that contains
*                      `module.json` and `.modules`). Usually "." when axle
*                      is invoked from the project root.
* @param test_name     If non-NULL, only the test whose filename (without
*                      `.c`/`.cpp`) equals this string is processed.
*                      If NULL, every file matching the sources patterns
*                      is processed.
* @param only_build    If true, tests are only compiled/linked and the
*                      resulting binaries are KEPT on disk.
*                      If false, every successfully built binary is
*                      executed and then removed after ALL tests have
*                      finished running.
*
* @return 0 if every requested test was built (and, when only_build is
*         false, also exited with status 0); non-zero otherwise.
*/
int axle_tests_run(const char *project_root,
                  const char *test_name,
                  bool        only_build);

#endif /* AXLE_TESTS_H */
