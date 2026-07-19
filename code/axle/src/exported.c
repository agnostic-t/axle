#include "axle/exported.h"
#include "axle/path_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <yyjson.h>

static char *exported_path(const char *project_root) {
    char *vendor = uax_path_concat(project_root, ".vendor");
    char *path = uax_path_concat(vendor, "exported.json");
    free(vendor);
    return path;
}

int axle_exported_load(const char *project_root, axle_exported_map *out) {
    if (!project_root || !out) return -1;

    memset(out, 0, sizeof(*out));

    char *path = exported_path(project_root);
    if (!path) return -1;

    yyjson_read_err err;
    yyjson_doc *doc = yyjson_read_file(path, 0, NULL, &err);
    free(path);

    if (!doc) {
        /* Missing file is OK — empty map. */
        return 0;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return -1;
    }

    size_t count = 0;
    {
        yyjson_obj_iter iter;
        yyjson_obj_iter_init(root, &iter);
        while (yyjson_obj_iter_next(&iter)) count++;
    }

    if (count == 0) {
        yyjson_doc_free(doc);
        return 0;
    }

    out->entries = calloc(count, sizeof(axle_exported_entry));
    if (!out->entries) {
        yyjson_doc_free(doc);
        return -1;
    }

    size_t idx = 0;
    yyjson_obj_iter iter;
    yyjson_obj_iter_init(root, &iter);
    yyjson_val *key;

    while ((key = yyjson_obj_iter_next(&iter))) {
        yyjson_val *val = yyjson_obj_iter_get_val(key);
        const char *name = yyjson_get_str(key);
        const char *repo = yyjson_get_str(yyjson_obj_get(val, "repo"));

        if (!name || !repo) continue;

        out->entries[idx].export_name = strdup(name);
        out->entries[idx].repo = strdup(repo);
        idx++;
    }

    out->n = idx;
    yyjson_doc_free(doc);
    return 0;
}

int axle_exported_save(const char *project_root, const axle_exported_map *map) {
    if (!project_root) return -1;

    /* Make sure .vendor/ exists. */
    char *vendor = uax_path_concat(project_root, ".vendor");
    struct stat st;
    if (stat(vendor, &st) != 0) {
        if (mkdir(vendor, 0755) != 0) {
            free(vendor);
            return -1;
        }
    }

    /* Build the JSON document. */
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    if (map) {
        for (size_t i = 0; i < map->n; i++) {
            yyjson_mut_val *k = yyjson_mut_str(doc, map->entries[i].export_name);
            yyjson_mut_val *v = yyjson_mut_obj(doc);
            yyjson_mut_obj_add_val(doc, v, "repo",
                                   yyjson_mut_str(doc, map->entries[i].repo));
            yyjson_mut_obj_add(root, k, v);
        }
    }

    /* Write to <vendor>/exported.json.tmp, then rename. */
    char *tmp_path = uax_path_concat(vendor, "exported.json.tmp");
    char *final_path = uax_path_concat(vendor, "exported.json");
    free(vendor);

    size_t written = 0;
    char *json_str = yyjson_mut_write(doc, 0, &written);
    yyjson_mut_doc_free(doc);

    if (!json_str) {
        free(tmp_path);
        free(final_path);
        return -1;
    }

    FILE *fp = fopen(tmp_path, "w");
    if (!fp) {
        free(json_str);
        free(tmp_path);
        free(final_path);
        return -1;
    }
    fputs(json_str, fp);
    fputc('\n', fp);
    fclose(fp);
    free(json_str);

    if (rename(tmp_path, final_path) != 0) {
        remove(tmp_path);
        free(tmp_path);
        free(final_path);
        return -1;
    }

    free(tmp_path);
    free(final_path);
    return 0;
}

int axle_exported_find_repo(const char *project_root, const char *export_name,
                            char **out_repo) {
    if (!project_root || !export_name || !out_repo) return -1;
    *out_repo = NULL;

    axle_exported_map map;
    if (0 != axle_exported_load(project_root, &map)) return -1;

    int found = -1;
    for (size_t i = 0; i < map.n; i++) {
        if (strcmp(map.entries[i].export_name, export_name) == 0) {
            *out_repo = strdup(map.entries[i].repo);
            found = 0;
            break;
        }
    }

    axle_exported_map_free(&map);
    return found;
}

int axle_exported_map_set(axle_exported_map *map, const char *export_name,
                          const char *repo) {
    if (!map || !export_name || !repo) return -1;

    /* Replace if already present. */
    for (size_t i = 0; i < map->n; i++) {
        if (strcmp(map->entries[i].export_name, export_name) == 0) {
            free(map->entries[i].repo);
            map->entries[i].repo = strdup(repo);
            return 0;
        }
    }

    axle_exported_entry *arr = realloc(map->entries,
                                       sizeof(axle_exported_entry) * (map->n + 1));
    if (!arr) return -1;
    map->entries = arr;
    map->entries[map->n].export_name = strdup(export_name);
    map->entries[map->n].repo = strdup(repo);
    map->n++;
    return 0;
}

int axle_exported_map_remove(axle_exported_map *map, const char *export_name) {
    if (!map || !export_name) return -1;

    for (size_t i = 0; i < map->n; i++) {
        if (strcmp(map->entries[i].export_name, export_name) == 0) {
            free(map->entries[i].export_name);
            free(map->entries[i].repo);
            /* Shift down. */
            for (size_t j = i + 1; j < map->n; j++) {
                map->entries[j - 1] = map->entries[j];
            }
            map->n--;
            return 0;
        }
    }
    return -1;
}

void axle_exported_map_free(axle_exported_map *map) {
    if (!map) return;
    if (map->entries) {
        for (size_t i = 0; i < map->n; i++) {
            free(map->entries[i].export_name);
            free(map->entries[i].repo);
        }
        free(map->entries);
    }
    memset(map, 0, sizeof(*map));
}
