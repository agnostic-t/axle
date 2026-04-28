#include "axle/string_utils.h"
#include <stdlib.h>
#include <string.h>

char *uax_concat(const char **input_strs, const char *concat_str, const char *prefix_str){
    size_t comb_len = 0;
    size_t n = 0;

    for (n = 0; input_strs[n] != 0; n++){
        comb_len += strlen(input_strs[n]) +
                    strlen(concat_str) +
                    (prefix_str ? strlen(prefix_str): 0);
    }

    char *output = malloc(comb_len + 1);

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
