#include "axle/hooks.h"
#include "axle/colors.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static int axle_hook_wait(pid_t pid, const char *stage, const char *command) {
  int status = 0;

  while (waitpid(pid, &status, 0) < 0) {
    if (errno == EINTR)
      continue;

    perror("[axle][hook] waitpid");
    return -1;
  }

  if (WIFEXITED(status)) {
    const int exit_code = WEXITSTATUS(status);
    if (exit_code == 0)
      return 0;

    fprintf(stderr,
            "%s[axle][hook][%s] command failed with exit code %d:%s %s\n",
            xfore.red, stage, exit_code, xfore.normal, command);
    return -1;
  }

  if (WIFSIGNALED(status)) {
    fprintf(stderr,
            "%s[axle][hook][%s] command terminated by signal %d:%s %s\n",
            xfore.red, stage, WTERMSIG(status), xfore.normal, command);
    return -1;
  }

  fprintf(stderr, "%s[axle][hook][%s] command ended unexpectedly:%s %s\n",
          xfore.red, stage, xfore.normal, command);
  return -1;
}

static int axle_hook_run_one(const char *command,
                             const char *stage,
                             const axle_receipt *receipt,
                             const char *base_path,
                             bool rebuild) {
  fflush(NULL);

  pid_t pid = fork();
  if (pid < 0) {
    perror("[axle][hook] fork");
    return -1;
  }

  if (pid == 0) {
    if (chdir(base_path) != 0) {
      perror("[axle][hook] chdir");
      _exit(126);
    }

    char *cwd = getcwd(NULL, 0);
    if (cwd) {
      setenv("AXLE_MODULE_DIR", cwd, 1);
      free(cwd);
    }

    setenv("AXLE_MODULE_NAME",
           receipt && receipt->metadata.name ? receipt->metadata.name : "",
           1);
    setenv("AXLE_HOOK_STAGE", stage ? stage : "", 1);
    setenv("AXLE_REBUILD", rebuild ? "1" : "0", 1);

    execl("/bin/sh", "sh", "-c", command, (char *)NULL);
    perror("[axle][hook] exec /bin/sh");
    _exit(127);
  }

  return axle_hook_wait(pid, stage, command);
}

int axle_hooks_run(const char **commands,
                   const char *stage,
                   const axle_receipt *receipt,
                   const char *base_path,
                   bool rebuild,
                   bool silent) {
  if (!commands)
    return 0;

  size_t count = 0;
  while (commands[count])
    count++;

  for (size_t i = 0; i < count; i++) {
    if (!silent) {
      printf("%s[axle][hook][%s]%s [%zu/%zu] %s\n",
             xfore.cyan,
             stage,
             xfore.normal,
             i + 1,
             count,
             commands[i]);
    }

    if (axle_hook_run_one(commands[i], stage, receipt, base_path, rebuild) < 0)
      return -1;
  }

  return 0;
}
