#include <X11/Xlib.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

/* ---- CONFIGURATION ---- */
#define PROGRAMME_NAME "gdesktop"
#define ORIGINAL_WALLPAPER ".config/openbox/Images/IMG_20240821_104051.jpg"
#define SCREENSHOT_PATH ".config/openbox/Images/wallpaper_temp.png"
#define PATH_MAX_LEN 512
#define MONITOR_DELAY_US                                                       \
  400000 // Fréquence de vérification de la perte de focus

char original_wallpaper[PATH_MAX_LEN];
char screenshot_path[PATH_MAX_LEN];

// Listes d'arguments pour execvp
char *feh_init_cmd[4];
char *pcmanfm_on_cmd[] = {"pcmanfm-qt", "--desktop", NULL};
char *pcmanfm_off_cmd[] = {"pcmanfm-qt", "--desktop-off", NULL};
char *scrot_cmd[] = {"scrot", "-o", "-q", "100", screenshot_path, NULL};
char *feh_live_cmd[5];

char *wmctrl_show_desktop[] = {"wmctrl", "-k", "on", NULL};
char *wmctrl_hide_desktop[] = {"wmctrl", "-k", "off", NULL};

// Drapeau atomique levé par le clic Openbox (SIGUSR1)
volatile sig_atomic_t request_desktop_wakeup = 0;

/* ---- GESTIONNAIRE DE SIGNAL ---- */

/**
 * @brief Intercepte le signal SIGUSR1 envoyé par Openbox lors d'un clic sur le
 * bureau.
 */
void handle_openbox_click(int sig) {
  if (sig == SIGUSR1) {
    request_desktop_wakeup = 1; // On lève le drapeau de réveil
  }
}

/* ---- LES FONCTIONS ---- */

void configurer_chemins() {
  char *home = getenv("HOME");
  if (home == NULL) {
    fprintf(stderr, "Erreur critique : $HOME introuvable.\n");
    exit(EXIT_FAILURE);
  }
  snprintf(original_wallpaper, sizeof(original_wallpaper), "%s/%s", home,
           ORIGINAL_WALLPAPER);
  snprintf(screenshot_path, sizeof(screenshot_path), "%s/%s", home,
           SCREENSHOT_PATH);
}

/**
 * @brief Interroge X11 pour savoir si le bureau a nativement le focus.
 */
bool is_desktop_focused(Display *display) {
  if (display == NULL)
    return false;
  Window root = DefaultRootWindow(display);
  Window focused_window;
  int revert_to;

  XGetInputFocus(display, &focused_window, &revert_to);
  return (focused_window == root || focused_window == PointerRoot ||
          focused_window == None);
}

pid_t lance_application(char *argv[]) {
  if (argv == NULL || argv[0] == NULL)
    return -1;
  pid_t pid = fork();
  if (pid < 0)
    return -1;
  if (pid == 0) {
    execvp(argv[0], argv);
    exit(EXIT_FAILURE);
  }
  return pid;
}

/**
 * @brief Phase d'initialisation et capture au démarrage.
 */
void init_desktop(Display *display) {
  if (display == NULL)
    return;

  printf("[Init] Étape 1 : Fond d'écran d'origine avec feh...\n");
  pid_t feh_init = lance_application(feh_init_cmd);
  if (feh_init > 0)
    waitpid(feh_init, NULL, 0);

  printf("[Init] Étape 2 : Démarrage temporaire de pcmanfm-qt...\n");
  pid_t pcmanfm_init = lance_application(pcmanfm_on_cmd);
  sleep(2);

  printf("[Init] Action wmctrl : Masquage temporaire des fenêtres...\n");
  pid_t wmctrl_pid = lance_application(wmctrl_show_desktop);
  if (wmctrl_pid > 0)
    waitpid(wmctrl_pid, NULL, 0);
  usleep(300000);

  printf("[Init] Étape 3 : Prise de la capture d'écran globale "
         "(Dual-Screen)...\n");
  pid_t scrot_pid = lance_application(scrot_cmd);
  if (scrot_pid > 0)
    waitpid(scrot_pid, NULL, 0);

  printf("[Init] Action wmctrl : Restauration des fenêtres...\n");
  wmctrl_pid = lance_application(wmctrl_hide_desktop);
  if (wmctrl_pid > 0)
    waitpid(wmctrl_pid, NULL, 0);

  if (pcmanfm_init > 0)
    waitpid(pcmanfm_init, NULL, WNOHANG);
  printf("[Init] Initialisation terminée avec succès !\n");
}

/**
 * @brief Boucle de surveillance réactive (hybride Signaux / Polling)
 */
void run_desktop_monitor(Display *display) {
  if (display == NULL)
    return;
  int current_state = 1; // Au début, le bureau est actif (1)

  printf("Démarrage de la surveillance réactive (Ctrl+C pour quitter)...\n");

  while (1) {
    bool desktop_focused = is_desktop_focused(display);

    // Nettoyage automatique des processus terminés (feh, etc.)
    while (waitpid(-1, NULL, WNOHANG) > 0)
      ;

    // CAS 1 : Déclenchement par le bouton de souris (Signal Openbox)
    if (request_desktop_wakeup == 1) {
      request_desktop_wakeup = 0; // On baisse le drapeau

      if (current_state != 1) {
        printf("[Signal] Clic Bureau détecté -> Activation instantanée de "
               "pcmanfm-qt\n");
        pid_t pid = lance_application(pcmanfm_on_cmd);
        if (pid > 0)
          waitpid(pid, NULL, WNOHANG);
        current_state = 1;
      }
    }

    // CAS 2 : Détection de la perte de focus (L'utilisateur est sur une
    // application)
    if (!desktop_focused && current_state != 0 && request_desktop_wakeup == 0) {
      printf("[Action] Perte Focus -> Masquage Bureau & Affichage Capture "
             "(feh)\n");

      // On cache le bureau pcmanfm
      pid_t off_pid = lance_application(pcmanfm_off_cmd);
      if (off_pid > 0)
        waitpid(off_pid, NULL, 0);

      // On affiche la photo sur le double-écran étendu via feh
      pid_t feh_pid = lance_application(feh_live_cmd);
      if (feh_pid > 0)
        waitpid(feh_pid, NULL, WNOHANG);

      current_state = 0;
    }

    fflush(stdout);
    usleep(MONITOR_DELAY_US);
  }
}

/* ---- ENTRÉE ---- */
int main() {
  // Optionnel : renommer le processus pour htop
  prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);

  configurer_chemins();

  // Armement du signal POSIX pour SIGUSR1 (déclenché par Openbox)
  struct sigaction sa;
  sa.sa_handler = handle_openbox_click;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART; // Évite de faire crasher les usleep lors de la
                            // réception du signal
  sigaction(SIGUSR1, &sa, NULL);

  // Initialisation dynamique des commandes de fond d'écran
  feh_init_cmd[0] = "feh";
  feh_init_cmd[1] = "--bg-scale";
  feh_init_cmd[2] = original_wallpaper;
  feh_init_cmd[3] = NULL;
  feh_live_cmd[0] = "feh";
  feh_live_cmd[1] = "--no-xinerama";
  feh_live_cmd[2] = "--bg-scale";
  feh_live_cmd[3] = screenshot_path;
  feh_live_cmd[4] = NULL;

  Display *display = XOpenDisplay(NULL);
  if (display == NULL) {
    fprintf(stderr, "Erreur : Impossible d'ouvrir le display X11\n");
    return 1;
  }

  init_desktop(display);
  run_desktop_monitor(display);

  XCloseDisplay(display);
  return 0;
}