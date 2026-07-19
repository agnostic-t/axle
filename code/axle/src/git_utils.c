#include "axle/git_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int axle_git_available(void) {
    int ret = system("git --version > /dev/null 2>&1");
    return ret == 0 ? 1 : 0;
}

int axle_git_clone(const char *url, const char *dest_path) {
    if (!url || !dest_path) return -1;

    /* 8KB should be enough for any sane URL + path pair. */
    size_t needed = strlen(url) + strlen(dest_path) + 64;
    char *cmd = malloc(needed);
    if (!cmd) return -1;
    snprintf(cmd, needed, "git clone -q %s %s", url, dest_path);

    int ret = system(cmd);
    free(cmd);
    return ret;
}

int axle_git_pull(const char *repo_path) {
    if (!repo_path) return -1;

    size_t needed = strlen(repo_path) + 64;
    char *cmd = malloc(needed);
    if (!cmd) return -1;
    /* --ff-only: refuse to create surprise merge commits. If a non-ff update
     * is required, the user will have to do it manually. */
    snprintf(cmd, needed, "git reset --hard && git -C %s pull -q --ff-only", repo_path);

    int ret = system(cmd);
    free(cmd);
    return ret;
}

int axle_git_head_sha(const char *repo_path, char *out_sha) {
    if (!repo_path || !out_sha) return -1;

    size_t needed = strlen(repo_path) + 64;
    char *cmd = malloc(needed);
    if (!cmd) return -1;
    snprintf(cmd, needed, "git -C %s rev-parse HEAD", repo_path);

    FILE *fp = popen(cmd, "r");
    free(cmd);
    if (!fp) return -1;

    /* Read first line; trim trailing newline. */
    if (!fgets(out_sha, 41, fp)) {
        pclose(fp);
        return -1;
    }
    size_t len = strlen(out_sha);
    while (len > 0 && (out_sha[len - 1] == '\n' || out_sha[len - 1] == '\r')) {
        out_sha[--len] = '\0';
    }

    /* Sanity: a SHA-1 hex string is 40 chars. If we got something shorter,
     * the repo is probably empty / has no commits yet. */
    if (len != 40) {
        pclose(fp);
        return -1;
    }

    pclose(fp);
    return 0;
}
