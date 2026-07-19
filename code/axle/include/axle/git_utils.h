#ifndef AXLE_GIT_UTILS_H
#define AXLE_GIT_UTILS_H

/* Thin wrapper around the system `git` binary. All functions return 0 on
 * success and non-zero on failure. stderr from git is inherited so the user
 * sees the actual error message. */

/* Run `git clone <url> <dest_path>` (quiet: -q). */
int axle_git_clone(const char *url, const char *dest_path);

/* Run `git -C <repo_path> pull` (quiet: -q, ff-only to avoid surprise
 * merges). */
int axle_git_pull(const char *repo_path);

/* Write the current HEAD commit SHA (40 hex chars + NUL) of <repo_path> into
 * `out_sha`, which must be at least 41 bytes. */
int axle_git_head_sha(const char *repo_path, char *out_sha);

/* Returns 1 if `git` is available on PATH (i.e. `git --version` succeeds),
 * 0 otherwise. */
int axle_git_available(void);

#endif /* AXLE_GIT_UTILS_H */
