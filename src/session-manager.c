#include <glib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define PROGRAMME_NAME "session-manager"

#define PATH_WALLPAPER "~/.config/openbox/Images/IMG_20240821_104051.jpg"
#define PATH_PICOM_CFG "~/.config/picom/picom.conf"

// Limites structurelles pour la sécurité
#define MAX_ARGV_SIZE 30
#define MAX_PATH_SIZE 512

pid_t polybar_pid = 0;

typedef enum { CMD_EXECVP, CMD_SYSTEM, CMD_BLUETOOTH, CMD_BATTERY } CmdType;

typedef struct {
  CmdType type;
  int delay;
  char *const argv[MAX_ARGV_SIZE];
  const char *shell_cmd;
} ProcessToLaunch;

// --- DÉCLARATION DU TABLEAU DE PROCESSUS ---
const ProcessToLaunch apps[] = {
    {CMD_EXECVP,
     0,
     {"xrandr",    "--output", "HDMI1",    "--primary", "--mode",
      "1920x1080", "--pos",    "1600x0",   "--rotate",  "normal",
      "--output",  "HDMI2",    "--mode",   "1600x1200", "--pos",
      "0x0",       "--rotate", "normal",   "--output",  "VGA1",
      "--off",     "--output", "VIRTUAL1", "--off",     NULL},
     NULL},
    {CMD_EXECVP,
     0,
     {"dbus-update-activation-environment", "--systemd",
      "DBUS_SESSION_BUS_ADDRESS", "DISPLAY", "XAUTHORITY", NULL},
     NULL},
    {CMD_EXECVP, 0, {"setxkbmap", "fr", NULL}, NULL},
    {CMD_EXECVP, 0, {"numlockx", "on", NULL}, NULL},
    {CMD_EXECVP, 0, {"gpgconf", "--launch", "gpg-agent", NULL}, NULL},
    {CMD_EXECVP,
     0,
     {"feh", "--no-fehbg", "--bg-fill", "--no-xinerama", PATH_WALLPAPER, NULL},
     NULL},
    {CMD_EXECVP,
     0,
     {"/usr/lib/polkit-gnome/polkit-gnome-authentication-agent-1", NULL},
     NULL},
    {CMD_EXECVP, 0, {"picom", "--config", PATH_PICOM_CFG, NULL}, NULL},
    {CMD_EXECVP, 0, {"dunst", NULL}, NULL},
    {CMD_EXECVP, 0, {"gdesktop", NULL}, NULL},

    // Bloc Chronométré A
    {CMD_SYSTEM,
     2,
     {NULL},
     "gpg --clearsign <<< \"Bonjour Alban, déverrouillage pour amc_sync\" > "
     "/dev/null"},
    {CMD_SYSTEM, 3, {NULL}, "systemctl --user start amc-sync.service"},
    {CMD_SYSTEM,
     3,
     {NULL},
     "notify-send \"AMC-Sync\" \"GPG OK : Le service a démarré.\""},

    // Bloc Chronométré B
    {CMD_SYSTEM, 3, {NULL}, "openbox --reconfigure"},
    {CMD_SYSTEM,
     3,
     {NULL},
     "xdotool mousemove_relative 1 1 && xdotool mousemove_relative -- -1 -1"},
    {CMD_EXECVP, 3, {"udiskie", NULL}, NULL},
    {CMD_EXECVP,
     3,
     {"redshift", "-l", "43.6:1.4", "-t", "5700:3600", NULL},
     NULL},
    {CMD_EXECVP, 3, {"gkhal", NULL}, NULL},
    {CMD_BLUETOOTH, 3, {NULL}, NULL},
    {CMD_BATTERY, 3, {NULL}, NULL},

    // Bloc Chronométré C
    {CMD_EXECVP, 5, {"launcher-qterminal", NULL}, NULL},
    {CMD_EXECVP, 5, {"gfirefox", NULL}, NULL},
    {CMD_EXECVP, 5, {"gneomutt", NULL}, NULL}};

const size_t num_apps = sizeof(apps) / sizeof(apps[0]);

// --- UTILS AVEC PROGRAMMATION DÉFENSIVE ---

int is_program_installed(const char *name) {
  // Défensif : On rejette les pointeurs nuls ou chaînes vides
  if (!name || name[0] == '\0') {
    fprintf(stderr, "[Erreur] is_program_installed: argument invalide.\n");
    return 0;
  }

  char *path = g_find_program_in_path(name);
  if (path) {
    g_free(path);
    return 1;
  }
  return 0;
}

void resolve_home_path(const char *src, char *dest, size_t dest_len) {
  // Défensif : Validation stricte des pointeurs et tailles de buffers
  if (!src || !dest || dest_len == 0) {
    fprintf(stderr,
            "[Erreur] resolve_home_path: Arguments de fonction NULL.\n");
    return;
  }

  const char *home = getenv("HOME");
  if (!home || home[0] == '\0') {
    home = "/home/alban"; // Repli sécurisé
  }

  if (src[0] == '~') {
    // Sécurisation du formattage de chaîne pour éviter les troncatures
    // sournoises
    int written = snprintf(dest, dest_len, "%s%s", home, src + 1);
    if (written < 0 || (size_t)written >= dest_len) {
      fprintf(stderr, "[Alerte] Chemin trop long et tronqué : %s\n", src);
      dest[0] = '\0'; // On invalide le buffer corrompu
    }
  } else {
    strncpy(dest, src, dest_len - 1);
    dest[dest_len - 1] = '\0';
  }
}

void run_bg_argv(char *const argv[]) {
  // Défensif : Rejet immédiat si pas d'arguments ou pas de binaire
  if (!argv || !argv[0]) {
    fprintf(stderr, "[Erreur] run_bg_argv: Tableau d'arguments vide.\n");
    return;
  }

  char *resolved_argv[MAX_ARGV_SIZE];
  char path_buffers[MAX_ARGV_SIZE][MAX_PATH_SIZE];
  int i = 0;

  // Protection contre les tableaux mal terminés (sans NULL final) ou trop
  // grands
  while (argv[i] != NULL && i < (MAX_ARGV_SIZE - 1)) {
    if (argv[i][0] == '~') {
      resolve_home_path(argv[i], path_buffers[i], sizeof(path_buffers[i]));
      resolved_argv[i] = path_buffers[i];
    } else {
      resolved_argv[i] = argv[i];
    }
    i++;
  }
  resolved_argv[i] = NULL;

  // Est-ce que le binaire nettoyé existe sur le système ?
  if (!is_program_installed(resolved_argv[0])) {
    fprintf(stderr, "[Session] Ignoré (non installé) : %s\n", resolved_argv[0]);
    return;
  }

  pid_t pid = fork();
  if (pid < 0) {
    perror("[Erreur critique] Fork échoué pour run_bg_argv");
    return;
  }

  if (pid == 0) {
    // Processus Enfant
    if (freopen("/dev/null", "w", stdout) == NULL ||
        freopen("/dev/null", "w", stderr) == NULL) {
      _exit(1); // Échec de redirection, on quitte discrètement
    }
    execvp(resolved_argv[0], resolved_argv);
    perror("execvp a échoué"); // Ne s'exécute que si execvp échoue
    _exit(127);
  }
}

void run_bg_system(const char *cmd) {
  // Défensif : Validation paramètre
  if (!cmd || cmd[0] == '\0') {
    fprintf(stderr, "[Erreur] run_bg_system: Commande vide.\n");
    return;
  }

  pid_t pid = fork();
  if (pid < 0) {
    perror("[Erreur critique] Fork échoué pour run_bg_system");
    return;
  }

  if (pid == 0) {
    if (freopen("/dev/null", "w", stdout) == NULL ||
        freopen("/dev/null", "w", stderr) == NULL) {
      _exit(1);
    }
    int ret = system(cmd);
    // Si system() échoue (-1) ou renvoie une erreur shell
    _exit(ret == -1 ? 1 : 0);
  }
}

// --- NETTOYAGE ---
void cleanup_session() {
  printf("[Supervisor] Polybar s'est arrêté ou un signal a été reçu. "
         "Nettoyage...\n");

  // Évite les boucles infinies de signaux pendant le massacre général
  signal(SIGTERM, SIG_IGN);
  signal(SIGINT, SIG_IGN);

  // Tue proprement TOUS les enfants du groupe
  kill(0, SIGTERM);
  exit(0);
}

void handle_signal(int sig) {
  (void)sig;
  cleanup_session();
}

// --- MOTEUR DE SÉLECTION ---
void spawn_process(ProcessToLaunch p) {
  if (p.delay > 0) {
    pid_t pid = fork();
    if (pid < 0) {
      perror("[Erreur] Impossible de fork pour le délai");
      return;
    }
    if (pid == 0) {
      sleep(p.delay);
      p.delay = 0;
      spawn_process(p); // Rappel récursif sécurisé dans l'enfant
      _exit(0);
    }
    return;
  }

  switch (p.type) {
  case CMD_EXECVP:
    run_bg_argv((char *const *)p.argv);
    break;

  case CMD_SYSTEM:
    run_bg_system(p.shell_cmd);
    break;

  case CMD_BLUETOOTH:
    if (access("/sys/class/bluetooth", F_OK) == 0) {
      if (is_program_installed("blueman-applet")) {
        run_bg_system("blueman-applet &");
      }
    }
    break;

  case CMD_BATTERY:
    if (access("/sys/class/power_supply/BAT0", F_OK) == 0 ||
        access("/sys/class/power_supply/BAT1", F_OK) == 0) {
      if (is_program_installed("cbatticon")) {
        run_bg_system("cbatticon -n &");
      }
    }
    break;

  default:
    fprintf(stderr,
            "[Alerte] Type de commande inconnu détecté dans le tableau.\n");
    break;
  }
}

// --- SÉQUENCE PRINCIPALE ---
void launch_session(void) {
  char wallpaper_resolved[MAX_PATH_SIZE];
  resolve_home_path(PATH_WALLPAPER, wallpaper_resolved,
                    sizeof(wallpaper_resolved));

  // Défensif : Vérification systématique des setenv()
  if (setenv("LANG", "fr_FR.UTF-8", 1) != 0 ||
      setenv("LC_TIME", "fr_FR.UTF-8", 1) != 0 ||
      setenv("MONITOR", "DP-1", 1) != 0 ||
      (wallpaper_resolved[0] != '\0' &&
       setenv("WALLPAPER_PATH", wallpaper_resolved, 1) != 0)) {
    fprintf(stderr, "[Erreur] Échec de l'initialisation des variables "
                    "d'environnement de base.\n");
  }

  char uid_str[64];
  if (snprintf(uid_str, sizeof(uid_str), "unix:path=/run/user/%d/bus",
               getuid()) > 0) {
    if (setenv("DBUS_SESSION_BUS_ADDRESS", uid_str, 1) != 0) {
      perror("setenv DBUS");
    }
  }

  char ssh_sock_path[MAX_PATH_SIZE];
  if (snprintf(ssh_sock_path, sizeof(ssh_sock_path),
               "/run/user/%d/gnupg/S.gpg-agent.ssh", getuid()) > 0) {
    if (setenv("SSH_AUTH_SOCK", ssh_sock_path, 1) != 0) {
      perror("setenv SSH");
    }
  }

  // Nettoyages initiaux
  int discard = system("pkill -9 picom 2>/dev/null") +
                system("pkill -9 nm-applet 2>/dev/null") +
                system("pkill -9 dunst 2>/dev/null");
  (void)discard;

  // Déroulement séquentiel sécurisé du tableau global
  for (size_t i = 0; i < num_apps; i++) {
    spawn_process(apps[i]);
  }

  // --- SURVEILLANCE CRITIQUE DE POLYBAR ---
  polybar_pid = fork();
  if (polybar_pid < 0) {
    perror("[Erreur fatale] Impossible de lancer Polybar. Session avortée.");
    cleanup_session();
  }

  if (polybar_pid == 0) {
    char *const polybar_cmd[] = {"polybar", "--reload", "ma_barre", NULL};
    execvp(polybar_cmd[0], polybar_cmd);
    perror("[Erreur] L'exécution de polybar a échoué");
    _exit(127);
  } else {
    int status;
    // On attend l'arrêt du processus Polybar pivot
    if (waitpid(polybar_pid, &status, 0) < 0) {
      perror("waitpid interrompu anormalement");
    }
    cleanup_session();
  }
}

int main(void) {
  // Changement de nom et isolation de groupe de processus
  if (prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0) < 0) {
    perror("Avertissement: Impossible de renommer le processus principal");
  }

  if (setpgid(0, 0) < 0) {
    perror("Erreur critique: Impossible d'isoler le groupe de processus "
           "(setpgid)");
    exit(EXIT_FAILURE);
  }

  // Liaison défensive des signaux fondamentaux
  if (signal(SIGINT, handle_signal) == SIG_ERR ||
      signal(SIGTERM, handle_signal) == SIG_ERR) {
    perror("Erreur critique lors de la mise en place des gestionnaires de "
           "signaux");
    exit(EXIT_FAILURE);
  }

  launch_session();

  return 0;
}