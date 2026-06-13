#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/**
 * @brief Supprime le lockfile résiduel et lance jgmenu dans un processus
 * complètement désolidarisé (double fork) pour éviter les processus zombies.
 */
void launch_jgmenu_detached(void) {
  // 1. SÉCURITÉ EXÉCUTABLE : On vérifie TOUT DE SUITE si jgmenu existe et est
  // exécutable
  const char *jgmenu_path = "/usr/bin/jgmenu";
  if (access(jgmenu_path, F_OK | X_OK) != 0) {
    fprintf(stderr,
            "Erreur Critique [launch_jgmenu_detached] : L'exécutable '%s' est "
            "introuvable ou non exécutable.\n",
            jgmenu_path);
    return; // On s'arrête immédiatement avant de faire des forks inutiles
  }

  // 2. SÉCURITÉ LOCKFILE : Vérification de la variable HOME
  const char *home = getenv("HOME");
  if (home && strlen(home) > 0) {
    char lockpath[1024];
    snprintf(lockpath, sizeof(lockpath), "%s/.jgmenu-lockfile", home);

    // On ne tente de supprimer le lockfile QUE s'il existe vraiment
    if (access(lockpath, F_OK) == 0) {
      if (remove(lockpath) != 0) {
        perror("Avertissement [launch_jgmenu_detached] : Impossible de "
               "supprimer le lockfile");
      }
    }
  } else {
    fprintf(stderr, "Avertissement [launch_jgmenu_detached] : Variable HOME "
                    "indisponible, suppression du lockfile ignorée.\n");
  }

  // 3. Premier Fork (Désormais sûr car on sait que jgmenu fonctionnera)
  pid_t pid = fork();

  if (pid < 0) {
    perror("Erreur [launch_jgmenu_detached] : Échec du premier fork");
    return;
  }

  if (pid == 0) {
    // --- Processus ENFANT ---
    if (fork() == 0) {
      // --- Processus PETIT-ENFANT ---
      char *args[] = {"jgmenu", "--vcenter", "--at-pointer", NULL};

      // Redirection des sorties pour ne pas polluer l'application parente
      if (freopen("/dev/null", "w", stdout) == NULL ||
          freopen("/dev/null", "w", stderr) == NULL) {
        // Échec optionnel de freopen, on continue quand même
      }

      // Exécution garantie
      execv(jgmenu_path, (char *const *)args);
      _exit(EXIT_FAILURE);
    } else {
      _exit(EXIT_SUCCESS);
    }
  } else {
    // --- Processus PARENT ---
    waitpid(pid, NULL, 0);
  }
}

int main(void) {
  launch_jgmenu_detached();
  return 0;
}