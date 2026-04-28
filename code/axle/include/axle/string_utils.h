#ifndef AXLE_STRING_UTILS_H
#define AXLE_STRING_UTILS_H

// * todo *
// make concatinating strings from `char **` to `char *`

char *uax_concat(const char **input_strs, const char *concat_str, const char *prefix_str);
char *uax_strrepl(const char *replace_in, char what, char with);
char *uax_strextend(const char *origin, const char *appendix);
int uax_ip_strextend(char **origin, const char *appendix);

#endif
