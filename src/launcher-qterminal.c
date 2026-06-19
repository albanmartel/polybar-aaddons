#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// --- CONFIGURATION CENTRALISÉE ---
#define PROGRAMME_NAME "launcher-lxterminal"
#define MAX_PATH_SIZE 512

// Commande de lancement et ses arguments sous forme de constante de réglage
const char *const terminal_args[] = {"lxterminal", "--geometry=500x500", NULL};

// Variables globales nécessaires pour le destructeur externe
char lock_path[256];
int lock_fd = -1;
pid_t child_pid = -1;

// --- FONCTIONS UTILITAIRES ---

/**
 * @brief Vérifie la présence d'un exécutable dans le PATH.
 */
int is_program_installed(const char *name) {
  if (!name || name[0] == '\0')
    return 0;
  if (strchr(name, '/'))
    return (access(name, X_OK) == 0);

  const char *path_env = getenv("PATH");
  if (!path_env)
    path_env = "/usr/bin:/bin";

  char *path_env_dup = strdup(path_env);
  if (!path_env_dup)
    return 0;

  int found = 0;
  char full_path[MAX_PATH_SIZE];
  char *token = strtok(path_env_dup, ":");

  while (token != NULL) {
    if (snprintf(full_path, sizeof(full_path), "%s/%s", token, name) <
        (int)sizeof(full_path)) {
      if (access(full_path, X_OK) == 0) {
        found = 1;
        break;
      }
    }
    token = strtok(NULL, ":");
  }

  free(path_env_dup);
  return found;
}

// --- DESTRUCTEUR ---

/**
 * @brief Gère la destruction et le nettoyage de l'instance à sa fermeture.
 */
void destructor_cleanup(void) {
  static int already_cleaned = 0;
  if (already_cleaned)
    return;
  already_cleaned = 1;

  printf("[%s] Destructeur activé : Nettoyage en cours...\n", PROGRAMME_NAME);

  if (lock_fd >= 0) {
    flock(lock_fd, LOCK_UN);
    close(lock_fd);
    unlink(lock_path);
    printf("[%s] Verrou et fichier lock supprimés.\n", PROGRAMME_NAME);
  }
}

// --- COEUR DE LOGIQUE DU LANCEUR SINGLETON ---

/**
 * @brief Gère l'intégralité du cycle de vie de l'instance (Vérification,
 * Singleton, Fork et Destructeur).
 * @return 0 en cas de succès (ou si une instance tourne déjà), 1 en cas
 * d'erreur critique.
 */
int manage_lxterminal_instance(void) {
  // 1. Préparation du chemin du lock
  snprintf(lock_path, sizeof(lock_path), "/run/user/%d/%s.lock", getuid(),
           PROGRAMME_NAME);

  // 2. Ouverture et vérification du Singleton via flock
  lock_fd = open(lock_path, O_RDWR | O_CREAT, 0600);
  if (lock_fd < 0) {
    perror("Erreur ouverture lockfile");
    return 1;
  }

  if (flock(lock_fd, LOCK_EX | LOCK_NB) < 0) {
    if (errno == EWOULDBLOCK) {
      fprintf(stderr,
              "[%s] Une instance est déjà en cours d'exécution. Abandon.\n",
              PROGRAMME_NAME);
      close(lock_fd);
      return 0; // Sortie propre
    }
    perror("Erreur flock");
    close(lock_fd);
    return 1;
  }

  // 3. Vérification de la présence de lxterminal
  if (!is_program_installed(terminal_args[0])) {
    fprintf(stderr, "Erreur Critique [%s] : '%s' est introuvable.\n",
            PROGRAMME_NAME, terminal_args[0]);
    destructor_cleanup();
    return 1;
  }

  // 4. Lancement de l'application enfant
  child_pid = fork();
  if (child_pid < 0) {
    perror("Erreur fork");
    destructor_cleanup();
    return 1;
  }

  if (child_pid == 0) {
    // --- ENFANT (LXTERMINAL) ---
    prctl(PR_SET_PDEATHSIG, SIGTERM);
    if (getppid() == 1)
      _exit(0);

    execvp(terminal_args[0], (char *const *)terminal_args);
    perror("Erreur critique lors de l'exécution du terminal");
    _exit(127);
  } else {
    // --- PARENT (LAUNCHER) ---
    // Écriture du PID dans le lockfile
    if (ftruncate(lock_fd, 0) == 0) {
      lseek(lock_fd, 0, SEEK_SET);
      dprintf(lock_fd, "%d\n", child_pid);
    }

    printf("[%s] Fenêtre lancée (PID enfant : %d). En attente...\n",
           PROGRAMME_NAME, child_pid);

    int status;
    // Attente passive de la fermeture de la fenêtre
    while (waitpid(child_pid, &status, 0) < 0) {
      if (errno != EINTR)
        break;
    }

    printf("[%s] La fenêtre LXTerminal a été fermée.\n", PROGRAMME_NAME);

    // Déclenchement automatique du destructeur
    destructor_cleanup();
  }

  return 0;
}

// --- LE MAIN MINIMAL ---

int main(void) {
  prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);

  // Délégation complète de la logique
  return (manage_lxterminal_instance() == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}