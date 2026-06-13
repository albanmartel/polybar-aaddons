#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define PROGRAMME_NAME "Full-Capture"
/*-- Choix de scrot --*/
#define SCROT0 "/usr/bin/scrot"
#define SCROT1 "-q"
#define SCROT2 "100"

// Fonction pour envoyer une notification de bureau de manière sécurisée via
// libnotify/notify-send
void envoyer_notification(const char *icon, const char *title,
                          const char *message, int critical) {
  // 1. Protection contre les pointeurs NULL
  // On remplace par des chaînes vides ou des valeurs par défaut sécurisées
  const char *safe_icon = icon ? icon : "dialog-information";
  const char *safe_title = title ? title : "Notification";
  const char *safe_message = message ? message : "";

  // 2. Premier fork
  pid_t pid = fork();

  if (pid < 0) {
    // Échec du fork (manque de mémoire, limite de processus atteinte)
    perror("Erreur lors du premier fork");
    return;
  }

  if (pid == 0) {
    // Premier enfant : on refork immédiatement pour détacher le processus
    pid_t grandchild_pid = fork();

    if (grandchild_pid < 0) {
      perror("Erreur lors du second fork");
      exit(EXIT_FAILURE);
    }

    if (grandchild_pid == 0) {
      // Petit-enfant : c'est lui qui va exécuter notify-send
      // Le cast (char *) est propre ici car execv requiert un tableau de
      // pointeurs modifiables, mais l'exécutable ne modifiera pas ces chaînes.
      char *args[] = {"/usr/bin/notify-send",
                      "-i",
                      (char *)safe_icon,
                      "-u",
                      critical ? "critical" : "normal",
                      (char *)safe_title,
                      (char *)safe_message,
                      NULL};

      execv(args[0], args);

      // Si execv échoue (ex: notify-send n'est pas installé à cet endroit)
      perror("Échec de execv pour notify-send");
      exit(EXIT_FAILURE);
    }

    // Le premier enfant meurt immédiatement.
    // Le petit-enfant est adopté par init/systemd, pas de zombie !
    exit(EXIT_SUCCESS);
  }

  // Processus parent original : on attend la mort du premier enfant
  // (instantanée)
  int status;
  waitpid(pid, &status, 0);
}

int main(void) {
  prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
  // 1. Déterminer le dossier de capture (XDG_PICTURES_DIR ou défaut)
  char screenshot_dir[512];
  const char *home = getenv("HOME");
  if (!home) {
    fprintf(stderr, "Erreur : Variable HOME introuvable.\n");
    return EXIT_FAILURE;
  }

  // Par défaut, on cible $HOME/Pictures/Screenshots
  snprintf(screenshot_dir, sizeof(screenshot_dir), "%s/Pictures/Screenshots",
           home);

  // 2. Créer le dossier s'il n'existe pas (équivalent de mkdir -p)
  // On crée d'abord ~/Pictures au cas où, puis ~/Pictures/Screenshots
  char parent_dir[512];
  snprintf(parent_dir, sizeof(parent_dir), "%s/Pictures", home);
  mkdir(parent_dir, 0755);
  mkdir(screenshot_dir, 0755);

  // 3. Générer le nom du fichier avec l'horodatage actuel
  time_t t = time(NULL);
  struct tm *tm_info = localtime(&t);
  if (!tm_info) {
    fprintf(stderr, "Erreur lors de la récupération de l'heure.\n");
    return EXIT_FAILURE;
  }

  char filename[128];
  char full_path[512];
  strftime(filename, sizeof(filename), "full_screenshot_%Y%m%d_%H%M%S.png",
           tm_info);
  // snprintf(full_path, sizeof(full_path), "%s/%s", screenshot_dir, filename);

  int ret =
      snprintf(full_path, sizeof(full_path), "%s/%s", screenshot_dir, filename);

  if (ret < 0 || (size_t)ret >= sizeof(full_path)) {
    fprintf(
        stderr,
        "Erreur : Le chemin du screenshot est trop long et a été tronqué !\n");
    // Optionnel : quitter la fonction ou le programme pour éviter de lancer
    // scrot dans le vide
    return EXIT_FAILURE;
  }

  // 4. Exécuter Flameshot de manière sécurisée avec fork/execv
  printf("[Screenshot] Capture des écrans vers : %s\n", filename);

  pid_t pid = fork();
  if (pid == -1) {
    fprintf(stderr, "Erreur lors du fork.\n");
    return EXIT_FAILURE;
  }

  if (pid == 0) {
    // Dans le processus enfant : on remplace le processus par flameshot
    // Cela évite de passer par un shell (pas de vulnérabilité aux injections)
    char *args[] = {SCROT0, SCROT1, SCROT2, full_path, NULL};
    execv(args[0], args);
    // Si execv échoue (par exemple si flameshot n'est pas installé) :
    exit(EXIT_FAILURE);
  }

  // Dans le processus parent : on attend que flameshot se termine et on vérifie
  // son statut
  int status;
  waitpid(pid, &status, 0);

  if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
    // Succès
    char msg[256];
    snprintf(msg, sizeof(msg), "Tes 2 écrans ont été enregistrés dans %s",
             filename);
    envoyer_notification("camera-photo", "Capture Totale", msg, 0);
    printf("[Screenshot] Succès !\n");
  } else {
    // Échec
    envoyer_notification("dialog-error", "Erreur", "La capture a échoué.", 1);
    fprintf(stderr, "[Screenshot] Erreur lors de la capture.\n");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}