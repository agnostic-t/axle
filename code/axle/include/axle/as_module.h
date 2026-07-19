#ifndef AXLE_AS_MODULE_H
#define AXLE_AS_MODULE_H

#include <stdbool.h>
#include <stddef.h>

/* A single exported module entry inside `.as.module.json`'s `export.modules`
 * object. `name` is the key under which the module becomes visible to
 * consumers (i.e. its `import_name`). `version` is the version the exporting
 * repo declares for this module. `priority` participates in the build order
 * sort (smaller = earlier); default 1000 if not specified. */
typedef struct {
  char *name;
  char *version;
  int priority;
  char *internal_module;
} axle_export_entry;

typedef struct {
  axle_export_entry *entries;
  size_t n;
  bool include_tests; /* if false, tests directory should be excluded */
  bool valid;         /* true if .as.module.json was found and parsed */
} axle_as_module;

/* Parse `<module_dir>/.as.module.json`.
 *
 * Recognises two top-level shapes:
 *   { "export": { "modules": { "<name>": { "version": "...", "priority": N },
 *                              ... },
 *                 "tests": true|false } }
 * and (legacy, deprecated):
 *   { "insert": { "modules": { "_": { "name": "...", "version": "..." } },
 *                 "tests": true|false } }
 *
 * For the legacy shape, a single entry is synthesised with `name` taken from
 * `insert.modules._.name`. A warning is printed in that case.
 *
 * Returns 0 on success (including when the file doesn't exist — `valid`
 * will be false). Returns -1 on error (malformed JSON / not an object).
 * Caller must call axle_as_module_free to release resources. */
int axle_as_module_parse(const char *module_dir, axle_as_module *out);

void axle_as_module_free(axle_as_module *as);

#endif /* AXLE_AS_MODULE_H */
