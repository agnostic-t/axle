#ifndef AXLE_EXPORTED_H
#define AXLE_EXPORTED_H

#include <stddef.h>

/* In-memory representation of `.vendor/exported.json` — a flat map
 * { "<export_name>": { "repo": "<repo_name>" } }. */

typedef struct {
  char *export_name;  /* key — name under which a module is visible in .modules */
  char *repo;         /* value — directory name under .vendor */
} axle_exported_entry;

typedef struct {
  axle_exported_entry *entries;
  size_t n;
} axle_exported_map;

/* Load `<project_root>/.vendor/exported.json`. Returns 0 on success (even if
 * the file does not exist — the map will be empty). Returns -1 on error
 * (malformed JSON). */
int axle_exported_load(const char *project_root, axle_exported_map *out);

/* Save `<project_root>/.vendor/exported.json` atomically (write to .tmp then
 * rename). Creates the `.vendor/` directory if it doesn't exist. */
int axle_exported_save(const char *project_root, const axle_exported_map *map);

/* Look up the repo for a given export_name. Returns 0 if found (and allocates
 * a copy of the repo name into *out_repo, caller frees). Returns -1 if not
 * found. */
int axle_exported_find_repo(const char *project_root, const char *export_name,
                            char **out_repo);

/* Add (or replace) an entry in the in-memory map. Takes ownership of no
 * memory; strings are strdup'd. */
int axle_exported_map_set(axle_exported_map *map, const char *export_name,
                          const char *repo);

/* Remove an entry from the in-memory map. Returns 0 if removed, -1 if not
 * present. */
int axle_exported_map_remove(axle_exported_map *map, const char *export_name);

void axle_exported_map_free(axle_exported_map *map);

#endif /* AXLE_EXPORTED_H */
