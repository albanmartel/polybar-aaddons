#include <errno.h>
#include <glib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define PROGRAMME_NAME "session-manager"

#define PATH_WALLPAPER "~/.config/openbox/Images/IMG_20240821_104051.jpg"
#define PATH_PICOM_CFG "~/.config/picom/picom.conf"
#define ENV_LANG "fr_FR.UTF-8"
#define ENV_LC_TIME "fr_FR.UTF-8"
#define ENV_MONITOR "DP-1"
#define POLYBAR_BAR_NAME "ma_barre"

// Limites structurelles pour la sécurité
#define MAX_ARGV_SIZE 30
#define MAX_PATH_SIZE 512

pid_t polybar_pid = 0;

typedef enum { CMD_EXECVP, CMD_SYSTEM, CMD_BLUETOOTH, CMD_BATTERY } CmdType;

// 1. Structure de CONFIGURATION pure (100% Statique/Const)
typedef struct {
  CmdType type;
  int delay;
  char *const argv[MAX_ARGV_SIZE];
  const char *shell_cmd;
} ProcessToLaunch;

// 2. Structure d'INSTANCE pour le suivi de session (Modifiable)
typedef struct {
  pid_t pid;
  const char *name;
} RuntimeProcess;

// --- DÉCLARATION DU TABLEAU DE CONFIGURATION (STRICTEMENT CONST) ---
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
    {CMD_EXECVP, 0, {"/usr/local/bin/clipboard_tool", NULL}, NULL},
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

// --- LE REGISTRE DE SUIVI DE SESSION (Mémoire vive globale) ---
RuntimeProcess session_pids[sizeof(apps) / sizeof(apps[0])] = {0};

// --- UTILS AVEC PROGRAMMATION DÉFENSIVE ---

int is_program_installed(const char *name) {
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
  if (!src || !dest || dest_len == 0) {
    fprintf(stderr,
            "[Erreur] resolve_home_path: Arguments de fonction NULL.\n");
    return;
  }

  const char *home = getenv("HOME");
  if (!home || home[0] == '\0') {
    home = "/home/alban";
  }

  if (src[0] == '~') {
    int written = snprintf(dest, dest_len, "%s%s", home, src + 1);
    if (written < 0 || (size_t)written >= dest_len) {
      fprintf(stderr, "[Alerte] Chemin trop long et tronqué : %s\n", src);
      dest[0] = '\0';
    }
  } else {
    strncpy(dest, src, dest_len - 1);
    dest[dest_len - 1] = '\0';
  }
}

pid_t run_bg_argv(char *const argv[]) {
  if (!argv || !argv[0]) {
    fprintf(stderr, "[Erreur] run_bg_argv: Tableau d'arguments vide.\n");
    return -1;
  }

  char *resolved_argv[MAX_ARGV_SIZE];
  char path_buffers[MAX_ARGV_SIZE][MAX_PATH_SIZE];
  int i = 0;

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

  if (!is_program_installed(resolved_argv[0])) {
    fprintf(stderr, "[Session] Ignoré (non installé) : %s\n", resolved_argv[0]);
    return -1;
  }

  pid_t pid = fork();
  if (pid < 0) {
    perror("[Erreur critique] Fork échoué pour run_bg_argv");
    return -1;
  }

  if (pid == 0) {
    if (freopen("/dev/null", "w", stdout) == NULL ||
        freopen("/dev/null", "w", stderr) == NULL) {
      _exit(1);
    }
    execvp(resolved_argv[0], resolved_argv);
    perror("execvp a échoué");
    _exit(127);
  }

  return pid;
}

pid_t run_bg_system(const char *cmd) {
  if (!cmd || cmd[0] == '\0') {
    fprintf(stderr, "[Erreur] run_bg_system: Commande vide.\n");
    return -1;
  }

  pid_t pid = fork();
  if (pid < 0) {
    perror("[Erreur critique] Fork échoué pour run_bg_system");
    return -1;
  }

  if (pid == 0) {
    if (freopen("/dev/null", "w", stdout) == NULL ||
        freopen("/dev/null", "w", stderr) == NULL) {
      _exit(1);
    }
    execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
    perror("[Erreur] execl a échoué");
    _exit(127);
  }

  return pid;
}

// --- TUEUR DISTINCT (Parcours à l'envers sur la structure d'instance) ---
void terminate_all_apps(void) {
  printf("[Session] Fermeture ordonnée des applications...\n");

  size_t i = num_apps;
  while (i--) {
    // Si l'application dispose d'un PID traqué dans notre structure de suivi,
    // on la termine
    if (session_pids[i].pid > 0) {
      printf("[Nettoyage] Envoi SIGTERM à : %s (PID: %d)\n",
             session_pids[i].name ? session_pids[i].name : "Commande Shell",
             session_pids[i].pid);

      kill(session_pids[i].pid, SIGTERM);
    }
  }
}

// --- NETTOYAGE ---
void cleanup_session() {
  printf("[Supervisor] Signal de fermeture reçu ou Polybar arrêté. "
         "Nettoyage...\n");

  signal(SIGTERM, SIG_IGN);
  signal(SIGINT, SIG_IGN);

  terminate_all_apps();

  sleep(1);
  exit(0);
}

void handle_signal(int sig) {
  (void)sig;
  cleanup_session();
}

// --- CONSTRUCTEUR DISTINCT (Lit du const, retourne un pid_t) ---
pid_t spawn_process(const ProcessToLaunch *app) {
  if (app == NULL) {
    fprintf(stderr, "[Erreur Critique] spawn_process: Pointeur de processus "
                    "NULL renvoyé.\n");
    return -1;
  }

  // Attente séquentielle synchrone (pas de fork temporaire)
  if (app->delay > 0) {
    printf(
        "[Session] Attente de %d secondes avant de lancer l'application...\n",
        app->delay);
    sleep(app->delay);
  }

  pid_t launched_pid = -1;

  switch (app->type) {
  case CMD_EXECVP:
    launched_pid = run_bg_argv((char *const *)app->argv);
    break;

  case CMD_SYSTEM:
    launched_pid = run_bg_system(app->shell_cmd);
    break;

  case CMD_BLUETOOTH:
    if (access("/sys/class/bluetooth", F_OK) == 0) {
      if (is_program_installed("blueman-applet")) {
        launched_pid = run_bg_system("blueman-applet");
      }
    }
    break;

  case CMD_BATTERY:
    if (access("/sys/class/power_supply/BAT0", F_OK) == 0 ||
        access("/sys/class/power_supply/BAT1", F_OK) == 0) {
      if (is_program_installed("cbatticon")) {
        launched_pid = run_bg_system("cbatticon -n");
      }
    }
    break;

  default:
    fprintf(stderr, "[Alerte] Type de commande inconnu détecté.\n");
    break;
  }

  if (launched_pid > 0) {
    const char *name = app->argv[0] ? app->argv[0] : app->shell_cmd;
    printf("[Supervisor] Application lancée avec succès : %s (PID: %d)\n", name,
           launched_pid);
  }

  return launched_pid;
}

// --- SÉQUENCE PRINCIPALE ---
void launch_session(void) {
  char wallpaper_resolved[MAX_PATH_SIZE];
  resolve_home_path(PATH_WALLPAPER, wallpaper_resolved,
                    sizeof(wallpaper_resolved));

  // --- CONFIGURATION CENTRALISÉE DES VARIABLES D'ENVIRONNEMENT ---
  if (setenv("LANG", ENV_LANG, 1) != 0 ||
      setenv("LC_TIME", ENV_LC_TIME, 1) != 0 ||
      setenv("MONITOR", ENV_MONITOR, 1) != 0 ||
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

  // Logique de chargement séquentiel et archivage des PIDs
  for (size_t i = 0; i < num_apps; i++) {
    pid_t pid = spawn_process(&apps[i]);
    if (pid > 0) {
      session_pids[i].pid = pid;
      session_pids[i].name =
          apps[i].argv[0] ? apps[i].argv[0] : apps[i].shell_cmd;
    }
  }

  // --- SURVEILLANCE CRITIQUE DE POLYBAR ---
  polybar_pid = fork();
  if (polybar_pid < 0) {
    perror("[Erreur fatale] Impossible de lancer Polybar. Session avortée.");
    cleanup_session();
  }

  if (polybar_pid == 0) {
    // Utilisation de la macro pour le nom de la barre ici aussi !
    char *const polybar_cmd[] = {"polybar", "--reload", POLYBAR_BAR_NAME, NULL};
    execvp(polybar_cmd[0], polybar_cmd);
    perror("[Erreur] L'exécution de polybar a échoué");
    _exit(127);
  } else {
    int status;
    while (waitpid(polybar_pid, &status, 0) < 0) {
      if (errno == EINTR) {
        continue;
      }
      perror("waitpid de polybar a rencontré une erreur fatale");
      break;
    }
    cleanup_session();
  }
}

void sigchld_handler(int sig) {
  (void)sig;
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
}

int main(void) {
  if (prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0) < 0) {
    perror("Avertissement: Impossible de renommer le processus principal");
  }

  if (setpgid(0, 0) < 0) {
    perror("Erreur critique: Impossible d'isoler le groupe de processus "
           "(setpgid)");
    exit(EXIT_FAILURE);
  }

  struct sigaction sa;
  sa.sa_handler = sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
  sigaction(SIGCHLD, &sa, NULL);

  if (signal(SIGINT, handle_signal) == SIG_ERR ||
      signal(SIGTERM, handle_signal) == SIG_ERR) {
    perror("Erreur critique lors de la mise en place des gestionnaires de "
           "signaux");
    exit(EXIT_FAILURE);
  }

  launch_session();

  return 0;
}