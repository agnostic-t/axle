#include "axle/as_module.h"
#include "axle/colors.h"
#include "axle/path_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yyjson.h>

static void axle_export_entry_init(axle_export_entry *e) {
  if (!e)
    return;
  e->name = NULL;
  e->version = NULL;
  e->priority = 1000;
  e->internal_module = NULL;
}

static int parse_modules_object(yyjson_val *modules, bool is_legacy,
                                axle_as_module *out) {
  if (!yyjson_is_obj(modules)) {
    fprintf(stderr, "%s[axle][as_module] %s `modules` is not an object%s\n",
            xfore.red, is_legacy ? "insert" : "export", xfore.normal);
    return -1;
  }

  /* First pass: count entries. */
  size_t count = 0;
  {
    yyjson_obj_iter iter;
    yyjson_obj_iter_init(modules, &iter);
    while (yyjson_obj_iter_next(&iter))
      count++;
  }

  if (count == 0) {
    out->valid = false;
    return 0;
  }

  out->entries = calloc(count, sizeof(axle_export_entry));
  if (!out->entries)
    return -1;

  size_t idx = 0;
  yyjson_obj_iter iter;
  yyjson_obj_iter_init(modules, &iter);
  yyjson_val *key;

  while ((key = yyjson_obj_iter_next(&iter))) {
    yyjson_val *val = yyjson_obj_iter_get_val(key);
    const char *key_str = yyjson_get_str(key);

    if (!key_str)
      continue;

    axle_export_entry *e = &out->entries[idx];
    axle_export_entry_init(e);

    if (is_legacy) {
      /* Legacy shape: only `_` is meaningful, and it carries name+version
       * explicitly. */
      if (strcmp(key_str, "_") != 0) {
        continue;
      }
      const char *name = yyjson_get_str(yyjson_obj_get(val, "name"));
      const char *ver = yyjson_get_str(yyjson_obj_get(val, "version"));
      if (!name) {
        continue;
      }
      e->name = strdup(name);
      if (ver)
        e->version = strdup(ver);
    } else {
      /* New shape: key is the internal module, name is in val OR key IS the
       * import name. */
      const char *name_override = yyjson_get_str(yyjson_obj_get(val, "name"));
      if (name_override) {
        e->name = strdup(name_override);
        e->internal_module = strdup(key_str);
      } else {
        e->name = strdup(key_str);
        e->internal_module = NULL;
      }

      const char *ver = yyjson_get_str(yyjson_obj_get(val, "version"));
      if (ver)
        e->version = strdup(ver);
      yyjson_val *prio = yyjson_obj_get(val, "priority");
      if (yyjson_is_int(prio)) {
        e->priority = (int)yyjson_get_int(prio);
      }
    }

    idx++;
  }

  out->n = idx;
  out->valid = (idx > 0);

  /* Compact if some legacy entries were skipped (only `_` is meaningful). */
  if (is_legacy && idx < count) {
    /* Already compacted by only incrementing idx for real entries. */
  }

  return 0;
}

int axle_as_module_parse(const char *module_dir, axle_as_module *out) {
  if (!module_dir || !out)
    return -1;

  memset(out, 0, sizeof(*out));
  out->include_tests = true; /* default: include tests */

  char *file_path = uax_path_concat(module_dir, ".as.module.json");
  if (!file_path)
    return -1;

  yyjson_read_err err;
  yyjson_doc *doc = yyjson_read_file(file_path, 0, NULL, &err);
  free(file_path);

  if (!doc) {
    /* File doesn't exist — not an error, just not valid. */
    return 0;
  }

  yyjson_val *root = yyjson_doc_get_root(doc);
  if (!yyjson_is_obj(root)) {
    fprintf(stderr, "%s[axle][as_module] root is not an object%s\n", xfore.red,
            xfore.normal);
    yyjson_doc_free(doc);
    return -1;
  }

  /* Prefer new `export` shape; fall back to legacy `insert` with warning. */
  yyjson_val *container = yyjson_obj_get(root, "export");
  bool is_legacy = false;

  if (!container) {
    container = yyjson_obj_get(root, "insert");
    if (container) {
      fprintf(stderr,
              "%s[axle][as_module] warning: `.as.module.json` uses deprecated "
              "`insert` shape. Please rename `insert` to `export` and use the "
              "new `export.modules` schema.%s\n",
              xfore.yellow, xfore.normal);
      is_legacy = true;
    }
  }

  if (container && yyjson_is_obj(container)) {
    yyjson_val *tests = yyjson_obj_get(container, "tests");
    if (yyjson_is_bool(tests)) {
      out->include_tests = yyjson_get_bool(tests);
    }

    yyjson_val *modules = yyjson_obj_get(container, "modules");
    if (modules) {
      if (0 != parse_modules_object(modules, is_legacy, out)) {
        yyjson_doc_free(doc);
        axle_as_module_free(out);
        return -1;
      }
    }
  }

  yyjson_doc_free(doc);
  return 0;
}

void axle_as_module_free(axle_as_module *as) {
  if (!as)
    return;
  if (as->entries) {
    for (size_t i = 0; i < as->n; i++) {
      if (as->entries[i].name)
        free(as->entries[i].name);
      if (as->entries[i].version)
        free(as->entries[i].version);
      if (as->entries[i].internal_module)
        free(as->entries[i].internal_module); // <-- ДОБАВЛЕНО
    }
    free(as->entries);
  }
  memset(as, 0, sizeof(*as));
}
