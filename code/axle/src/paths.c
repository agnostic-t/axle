#include "axle/path_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

char *uax_path_change_ext(const char* path, const char *new_ext){
    if (!path) return NULL;

    char *output = calloc(strlen(path) + strlen(new_ext) + 2, 1);
    strcpy(output, path);

    size_t i = 0;
    for (; i < strlen(path); i++)
        if (path[i] == '.') {i++; break;}

    strcpy(output + i, new_ext);
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
