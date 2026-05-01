#include "axle/string_utils.h"
#include <stdlib.h>
#include <string.h>

void uax_free_strlist_ne(char* **_strlist){
    if (!_strlist || !*_strlist) return;
    for (size_t i = 0; (*_strlist)[i] != 0; i++) {
        if ((*_strlist)[i])
            free((*_strlist)[i]);
    }
    free(*_strlist);
    *_strlist = NULL;
}

char *uax_concat(const char **input_strs, const char *concat_str, const char *prefix_str){
    if (!input_strs || !input_strs[0]) return NULL;

    size_t comb_len = 0;
    size_t n = 0;

    for (n = 0; input_strs[n] != 0; n++){
        comb_len += strlen(input_strs[n]) +
                    strlen(concat_str) +
                    (prefix_str ? strlen(prefix_str): 0);
    }

    char *output = calloc(comb_len + 1, 1);

    size_t offset = 0;
    for (n = 0; input_strs[n] != 0; n++){
        if (prefix_str) {
            strcpy(output + offset, prefix_str);
            offset += strlen(prefix_str);
        }

        strcpy(output + offset, input_strs[n]); offset += strlen(input_strs[n]);
        strcpy(output + offset, concat_str); offset += strlen(concat_str);
    }

    return output;
}

char *uax_strrepl(const char *replace_in, char what, char with){
    if (!replace_in || !what || !with) return NULL;

    char *output = calloc(strlen(replace_in) + 1, 1);
    for (size_t i = 0; i < strlen(replace_in); i++){
        output[i] = replace_in[i] == what ? with: replace_in[i];
    }

    return output;
}

char *uax_strextend(const char *origin, const char *appendix){
    size_t len = (origin? strlen(origin): 0) + (appendix? strlen(appendix): 0) + 1;
    if (len == 0) return NULL;

    char *output = calloc(len, 1);

    size_t offset = 0;
    if (origin) {strcpy(output + offset, origin); offset += strlen(origin); }
    if (appendix) {strcpy(output + offset, appendix); offset += strlen(appendix); }

    return output;
}

int uax_ip_strextend(char **origin, const char *appendix){
    if (!origin || !appendix) return -1;

    char *extended = uax_strextend(*origin, appendix);
    if (*origin)
        free(*origin);
    *origin = extended;

    return 0;
}

void uax_free_strlist(char* **_strlist, size_t *n){
    if (!_strlist || !*_strlist) return;
    for (size_t i = 0; i < *n; i++) {
        if ((*_strlist)[i])
            free((*_strlist)[i]);
    }
    free(*_strlist);
    *_strlist = NULL;
    *n = 0;
}

int uax_strlist_extend(char ***origin, size_t *n, const char *appendix){
    // if (!appendix) return 0;

    char **tmp = realloc(*origin, sizeof(char*) * (*n + 1));
    if (!tmp) return -1;

    *origin = tmp;
    (*origin)[*n] = appendix? strdup(appendix): NULL;
    if (!(*origin)[*n] && appendix) return -1;

    (*n)++;
    return 0;
}

int uax_strlist_extend_ne(char ***origin, const char *appendix){
    size_t n = 0;
    if (*origin){
        for (size_t i = 0; (*origin)[i] != 0; i++)
            n++;
    }

    char **tmp = realloc(*origin, sizeof(char*) * (n + 2));
    if (!tmp) return -1;

    *origin = tmp;
    (*origin)[n] = appendix? strdup(appendix): NULL;
    (*origin)[n + 1] = NULL;

    if (!(*origin)[n] && appendix) return -1;

    return 0;
}
