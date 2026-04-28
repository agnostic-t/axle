#ifndef AXLE_PATH_UTILS_H
#define AXLE_PATH_UTILS_H

#include <stdint.h>

// * todo *
// make glob expansion
// make path to hash function


void uax_get_hash(const char* key, uint32_t len, char output[8]);
char *uax_path_get_ext(const char* path);
char *uax_path_change_ext(const char* path, const char *new_ext);
char *uax_path_concat(const char* path_1, const char *path_2);

#endif
