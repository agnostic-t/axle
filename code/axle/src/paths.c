#include "axle/path_utils.h"
#include <linux/limits.h>
#include <glob.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>
#include <sys/stat.h>
#include <fnmatch.h>
#include <libgen.h>

#define AXLE_MURMURHASH_SEED 823958123

char *uax_path_get_ext(const char* path){
    if (!path) return NULL;

    char *output = calloc(strlen(path), 1);

    for (ssize_t i = strlen(path) - 1; i >= 0; i--){
        if (path[i] == '.')
            break;
        output[strlen(path) - 1 - i] = path[i];
    }

    return output;
}

char *uax_path_change_ext(const char *path, const char *new_ext) {
    if (!path || !new_ext) return NULL;

    const char *last_dot = strrchr(path, '.');
    if (last_dot == path) last_dot = NULL;

    size_t base_len = last_dot ? (size_t)(last_dot - path) : strlen(path);
    size_t ext_len  = strlen(new_ext);
    size_t dot_len  = (ext_len > 0 && new_ext[0] != '.') ? 1 : 0;

    char *output = malloc(base_len + dot_len + ext_len + 1);
    if (!output) return NULL;

    memcpy(output, path, base_len);
    if (dot_len) output[base_len] = '.';
    memcpy(output + base_len + dot_len, new_ext, ext_len);
    output[base_len + dot_len + ext_len] = '\0';

    return output;
}

char *uax_path_concat(const char* path_1, const char *path_2){

    size_t len = (path_1? strlen(path_1): 0) + (path_2? strlen(path_2): 0) + 4;
    if (len == 0) return NULL;

    char *output = calloc(len, 1);

    size_t offset = 0;
    if (path_1) {
        strcpy(output, path_1);
        if (output[strlen(output) - 1] != '/'){
            output[strlen(output)] = '/';
            offset += 1;
        }

        offset += strlen(path_1);
    }

    if (path_2) {
        strcpy(output + offset, path_2);
        offset += strlen(path_2);
    }

    return output;
}

char *uax_path_get_dir(const char *path){
    if (!path) return NULL;

    char *path_copy = strdup(path);
    if (!path_copy) return NULL;

    char *dir = dirname(path_copy);
    char *result = strdup(dir);

    free(path_copy);
    return result;
}

void uax_get_hash(const char* key, uint32_t len, char output[8]) {
    uint32_t c1 = 0xcc9e2d51;
    uint32_t c2 = 0x1b873593;
    uint32_t r1 = 15;
    uint32_t r2 = 13;
    uint32_t m = 5;
    uint32_t n = 0xe6546b64;
    uint32_t h = AXLE_MURMURHASH_SEED;

    const uint32_t* blocks = (const uint32_t*)(key);
    int nblocks = len / 4;

    for (int i = 0; i < nblocks; i++) {
        uint32_t k = blocks[i];
        k *= c1;
        k = (k << r1) | (k >> (32 - r1));
        k *= c2;

        h ^= k;
        h = (h << r2) | (h >> (32 - r2));
        h = h * m + n;
    }

    const uint8_t* tail = (const uint8_t*)(key + nblocks * 4);
    uint32_t k1 = 0;
    switch (len & 3) {
        case 3: k1 ^= tail[2] << 16;
        case 2: k1 ^= tail[1] << 8;
        case 1: k1 ^= tail[0];
                k1 *= c1;
                k1 = (k1 << r1) | (k1 >> (32 - r1));
                k1 *= c2;
                h ^= k1;
    }

    h ^= len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    sprintf(
        output, "%x%x%x%x",
        (h >> (8*0)) & 0xff,
        (h >> (8*1)) & 0xff,
        (h >> (8*2)) & 0xff,
        (h >> (8*3)) & 0xff
    );
}

static int has_glob_chars(const char *str) {
    return strpbrk(str, "*?[{") != NULL;
}

static int has_globstar(const char *pattern) {
    return strstr(pattern, "**") != NULL;
}

static int _cfg_append_str(char ***arr, size_t *n, const char *str) {
    if (!str) return 0;

    char **tmp = realloc(*arr, sizeof(char*) * (*n + 1));
    if (!tmp) return -1;

    *arr = tmp;
    (*arr)[*n] = strdup(str);
    if (!(*arr)[*n]) return -1;

    (*n)++;
    return 0;
}

static void _expand_globstar(const char *base_dir, const char *file_pattern, char ***out_paths, size_t *out_n) {
    DIR *dir = opendir(base_dir);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, entry->d_name);

        struct stat st;
        if (lstat(full_path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            _expand_globstar(full_path, file_pattern, out_paths, out_n);
        }

        if (S_ISREG(st.st_mode) && fnmatch(file_pattern, entry->d_name, 0) == 0) {
            _cfg_append_str(out_paths, out_n, full_path);
        }
    }
    closedir(dir);
}


int uax_expand_files(const char *base_dir, const char **path_list, char ***out_paths, size_t *out_n) {
    if (!path_list) return -1;

    for (int i = 0; path_list[i] != NULL; i++) {
        const char *token = path_list[i];

        if (has_glob_chars(token)) {
            char pattern[PATH_MAX];
            if (base_dir && strcmp(base_dir, ".") != 0) {
                snprintf(pattern, sizeof(pattern), "%s/%s", base_dir, token);
            } else {
                snprintf(pattern, sizeof(pattern), "%s", token);
            }

            if (has_globstar(pattern)) {
                const char *star_pos = strstr(pattern, "**");
                if (star_pos) {
                    char base_path[PATH_MAX];
                    size_t prefix_len = star_pos - pattern;
                    strncpy(base_path, pattern, prefix_len);
                    base_path[prefix_len] = '\0';

                    size_t len = strlen(base_path);
                    if (len > 0 && base_path[len - 1] == '/') base_path[len - 1] = '\0';
                    if (len == 0) strcpy(base_path, ".");

                    const char *final_pattern = star_pos + 2;
                    if (*final_pattern == '/') final_pattern++;

                    _expand_globstar(base_path, final_pattern, out_paths, out_n);
                }
            } else {
                glob_t glob_result;
                int ret = glob(pattern, GLOB_TILDE | GLOB_BRACE | GLOB_ERR, NULL, &glob_result);

                if (ret != 0) {
                    fprintf(stderr, "[warn] no files matched pattern: %s\n", token);
                    continue;
                }

                for (size_t j = 0; j < glob_result.gl_pathc; j++) {
                    char *full_path = glob_result.gl_pathv[j];
                    char *relative_path = NULL;

                    if (base_dir && strncmp(full_path, base_dir, strlen(base_dir)) == 0) {
                        char *start = full_path + strlen(base_dir);
                        if (*start == '/') start++;
                        relative_path = strdup(start);
                    } else {
                        relative_path = strdup(full_path);
                    }

                    if (relative_path) {
                        // printf("[uax][exp_files] adding %s\n", relative_path);
                        _cfg_append_str(out_paths, out_n, relative_path);
                        free(relative_path);
                    }
                }
                globfree(&glob_result);
            }
        } else {
            char *path = uax_path_concat(base_dir, token);
            _cfg_append_str(out_paths, out_n, path);
            free(path);
        }
    }

    return 0;
}
