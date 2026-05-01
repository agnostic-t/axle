#ifndef AXLE_STRING_UTILS_H
#define AXLE_STRING_UTILS_H

// * todo *

#include <stddef.h>
char *uax_concat(const char **input_strs, const char *concat_str, const char *prefix_str);
char *uax_strrepl(const char *replace_in, char what, char with);
char *uax_strextend(const char *origin, const char *appendix);
int uax_ip_strextend(char **origin, const char *appendix);

void uax_free_strlist(char* **_strlist, size_t *n);
void uax_free_strlist_ne(char* **_strlist);

int uax_strlist_extend(char ***origin, size_t *n, const char *appendix);
int uax_strlist_extend_ne(char ***origin, const char *appendix);

#endif
