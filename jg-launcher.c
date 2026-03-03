#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main() {
  // 1. Suppression du lockfile
  char lockpath[1024];
  char *home = getenv("HOME");
  if (home) {
    snprintf(lockpath, sizeof(lockpath), "%s/.jgmenu-lockfile", home);
    remove(lockpath);
  }

  // 2. Premier Fork
  pid_t pid = fork();

  if (pid < 0)
    exit(EXIT_FAILURE);

  if (pid == 0) {
    // --- Processus ENFANT ---
    // Deuxième Fork pour désolidariser jgmenu du terminal/parent
    if (fork() == 0) {
      char *path = "/usr/bin/jgmenu";
      char *args[] = {"jgmenu", "--vcenter", "--at-pointer", NULL};

      // On redirige les sorties vers /dev/null pour éviter de polluer le
      // terminal
      freopen("/dev/null", "w", stdout);
      freopen("/dev/null", "w", stderr);

      execv(path, args);
      _exit(1);
    } else {
      // L'enfant meurt tout de suite
      _exit(0);
    }
  } else {
    // --- Processus PARENT ---
    // Il attend juste que l'enfant meure (ce qui est instantané)
    waitpid(pid, NULL, 0);
  }

  // Ici, le programme s'arrête TOUJOURS immédiatement.
  return 0;
}