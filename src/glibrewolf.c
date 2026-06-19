#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

// --- CONFIGURATION DES CHEMINS ---
#define PROGRAMME_NAME "librewolf_plume"
#define SOURCE_SUBPATH "/.config/librewolf/librewolf/Modele"
#define TEMP_BASE_PATH "/tmp"
#define LIBREWOLF_BIN "librewolf"
#define GTK_THEME_VAL "Adwaita"
#define MARGIN_MB 50

// --- MESSAGES UTILISATEUR (FRANÇAIS) ---
#define MSG_ESPACE_REQ "Espace requis : %ld Mo | Disponible : %ld Mo\n"
#define MSG_ERREUR_ESPACE                                                      \
  "Erreur : Espace insuffisant dans %s (marge de %d Mo requise) !\n"
#define MSG_PREPARATION "Préparation du profil temporaire...\n"
#define MSG_INJECTION                                                          \
  "Injection des préférences (Browserpass & DuckDuckGo)...\n"
#define MSG_LANCEMENT "Lancement de %s (Profil : %s)...\n"
#define MSG_FERMETURE "\nFermeture de Librewolf détectée. Nettoyage...\n"
#define MSG_NETTOYAGE_OK "Profil temporaire supprimé avec succès.\n"
#define MSG_ERR_HOME "Erreur : Impossible de trouver la variable HOME.\n"
#define MSG_ERR_MODEL "Erreur lors de la copie du profil modèle.\n"

// --- FONCTIONS ---

/**
 * @brief Calcule l'espace disque disponible en Mo pour un point de montage
 * donné.
 * @param path Le chemin du répertoire ou point de montage (ex: "/home").
 * @return L'espace libre en Mo, ou -1 en cas d'erreur ou de chemin invalide.
 */
long get_free_space_mb(const char *path) {
  // 1. Sécurité : On vérifie si le pointeur est NULL
  if (path == NULL) {
    fprintf(stderr,
            "Erreur [get_free_space_mb] : Le pointeur de chemin est NULL.\n");
    return -1;
  }

  // 2. Sécurité : On vérifie si la chaîne est vide
  if (strlen(path) == 0) {
    fprintf(stderr,
            "Erreur [get_free_space_mb] : Le chemin fourni est vide.\n");
    return -1;
  }

  struct statvfs stat;

  // 3. Appel système sécurisé
  if (statvfs(path, &stat) != 0) {
    // Renvoie -1 si le chemin n'existe pas (ex: clé USB débranchée entre-temps)
    return -1;
  }

  // Calcul de l'espace libre en Mo
  return (stat.f_bsize * stat.f_bavail) / (1024 * 1024);
}

/**
 * @brief Calcule la taille d'un répertoire en Mo via la commande 'du'.
 * @param dir Le chemin du répertoire à analyser.
 * @return La taille en Mo, ou 0 en cas d'erreur ou de chemin invalide.
 */
long get_dir_size_mb(const char *dir) {
  if (dir == NULL || strlen(dir) == 0) {
    fprintf(stderr, "Erreur [get_dir_size_mb] : Argument invalide.\n");
    return 0;
  }

  int pfd[2];
  if (pipe(pfd) < 0) {
    perror("Erreur pipe");
    return 0;
  }

  pid_t pid = fork();
  if (pid < 0) {
    perror("Erreur fork du");
    close(pfd[0]);
    close(pfd[1]);
    return 0;
  }

  if (pid == 0) {
    // Enfant : exécute 'du -sm dir' de manière sécurisée sans passer par un
    // shell
    close(pfd[0]);
    if (dup2(pfd[1], STDOUT_FILENO) < 0) {
      _exit(1);
    }
    close(pfd[1]);

    // Rediriger stderr vers /dev/null pour éviter de polluer la console
    if (freopen("/dev/null", "w", stderr) == NULL) {
      _exit(1);
    }

    execlp("du", "du", "-sm", dir, (char *)NULL);
    _exit(127);
  }

  // Parent
  close(pfd[1]);
  char result[32] = {0};
  ssize_t n = read(pfd[0], result, sizeof(result) - 1);
  close(pfd[0]);

  waitpid(pid, NULL, 0);

  if (n <= 0)
    return 0;

  // Extraire le nombre (du renvoie "TAILLE\tCHEMIN\n")
  char *end;
  long size = strtol(result, &end, 10);
  return (end == result) ? 0 : size;
}

/**
 * @brief Prépare un profil Firefox en RAM, le lance, lie sa durée de vie au
 * parent, et nettoie la RAM à sa fermeture.
 * @return 0 en cas de succès, 1 en cas d'erreur.
 */
int launch_firefox_profile(void) {
  char *home = getenv("HOME");
  if (!home) {
    fprintf(stderr, MSG_ERR_HOME);
    return 1;
  }

  char source_model[1024];
  char temp_base_path[256];
  char temp_profile[1024];
  char cmd[2500];

  snprintf(temp_base_path, sizeof(temp_base_path), "/run/user/%d", getuid());
  snprintf(source_model, sizeof(source_model), "%s%s", home, SOURCE_SUBPATH);
  snprintf(temp_profile, sizeof(temp_profile), "%s/firefox_plume_%ld",
           temp_base_path, (long)time(NULL));

  // 1. Espace disque
  long required = get_dir_size_mb(source_model);
  long available = get_free_space_mb(temp_base_path);
  printf(MSG_ESPACE_REQ, required, available);

  if (available < (required + MARGIN_MB)) {
    fprintf(stderr, MSG_ERREUR_ESPACE, temp_base_path, MARGIN_MB);
    return 1;
  }

  // 2. Déploiement du modèle
  printf(MSG_PREPARATION);
  snprintf(cmd, sizeof(cmd), "cp -a \"%s/.\" \"%s/\"", source_model,
           temp_profile);
  if (system(cmd) != 0) {
    fprintf(stderr, MSG_ERR_MODEL);
    return 1;
  }

  // 3. Verrous
  snprintf(cmd, sizeof(cmd), "rm -f \"%s/.parentlock\" \"%s/lock\"",
           temp_profile, temp_profile);
  if (system(cmd) < 0) {
  }

  // 4. Exécution
  pid_t pid = fork();
  if (pid < 0) {
    perror("Erreur fork");
    return 1;
  }

  if (pid == 0) {
    // Enfant : Lie sa survie au parent (gdesktop / session-manager)
    prctl(PR_SET_PDEATHSIG, SIGTERM);
    if (getppid() == 1)
      _exit(0);

    setenv("GDK_BACKEND", "x11", 1);
    setenv("GTK_THEME", GTK_THEME_VAL, 1);

    printf(MSG_LANCEMENT, LIBREWOLF_BIN, temp_profile);
    execlp(LIBREWOLF_BIN, LIBREWOLF_BIN, "--profile", temp_profile,
           "--no-remote", (char *)NULL);
    perror("Erreur execlp");
    _exit(127);
  } else {
    // Parent : Attend Firefox
    int status;
    while (waitpid(pid, &status, 0) < 0) {
      if (errno != EINTR)
        break;
    }

    // Nettoyage impératif
    printf(MSG_FERMETURE);
    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", temp_profile);
    if (system(cmd) == 0) {
      printf(MSG_NETTOYAGE_OK);
    }
  }

  return 0;
}

int main(void) {
  // Changement du nom du processus pour le système
  prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);

  // Appel de notre fonction
  return launch_firefox_profile();
}