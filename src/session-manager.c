#include <glib.h> // Indispensable pour g_find_program_in_path
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// --- NOM PROGRAMME ---
#define PROGRAMME_NAME "session-manager"

pid_t polybar_pid = 0;

// --- UTILS : Vérification si un programme est installé ---
int is_program_installed(const char *name) {
  if (!name || strlen(name) == 0)
    return 0;
  char *path = g_find_program_in_path(name);
  if (path) {
    g_free(path);
    return 1;
  }
  return 0;
}

// --- UTILS : Lancement sécurisé en tâche de fond ---
void run_bg(char *const argv[]) {
  if (!is_program_installed(argv[0]))
    return; // On ignore si l'application n'est pas installée
  if (fork() == 0) {
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
    execvp(argv[0], argv);
    _exit(127);
  }
}

// --- NETTOYAGE GLOBAL ---
void cleanup_session() {
  printf("[Supervisor] Polybar s'est arrêté ou un signal a été reçu. "
         "Nettoyage...\n");
  signal(SIGTERM, SIG_IGN);
  kill(0, SIGTERM);
  exit(0);
}

void handle_signal(int sig) {
  (void)sig;
  cleanup_session();
}

int main(void) {
  // On définit le nom du processus dans le gestionnaire de tâches
  prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);

  // 1. Devenir leader du groupe de processus pour isoler le kill(0)
  setpgid(0, 0);

  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);

  // Récupération dynamique du répertoire HOME pour les chemins de fichiers
  const char *home = getenv("HOME");
  if (!home)
    home = "/home/alban"; // Fallback de sécurité

  // Construction dynamique des chemins de fichiers
  char wallpaper_path[512];
  char picom_conf_path[512];
  snprintf(wallpaper_path, sizeof(wallpaper_path),
           "%s/.config/openbox/Images/IMG_20240821_104051.jpg", home);
  snprintf(picom_conf_path, sizeof(picom_conf_path),
           "%s/.config/picom/picom.conf", home);

  // 2. EXPORT DE L'ENVIRONNEMENT
  setenv("LANG", "fr_FR.UTF-8", 1);
  setenv("LC_TIME", "fr_FR.UTF-8", 1);
  setenv("MONITOR", "DP-1", 1);
  setenv("WALLPAPER_PATH", wallpaper_path, 1);

  // --- Écrans & D-Bus ---
  char *const xrandr_cmd[] = {
      "xrandr",    "--output", "HDMI1",    "--primary", "--mode",
      "1920x1080", "--pos",    "1600x0",   "--rotate",  "normal",
      "--output",  "HDMI2",    "--mode",   "1600x1200", "--pos",
      "0x0",       "--rotate", "normal",   "--output",  "VGA1",
      "--off",     "--output", "VIRTUAL1", "--off",     NULL};
  run_bg(xrandr_cmd);

  char uid_str[64];
  snprintf(uid_str, sizeof(uid_str), "unix:path=/run/user/%d/bus", getuid());
  setenv("DBUS_SESSION_BUS_ADDRESS", uid_str, 1);

  char *const dbus_cmd[] = {"dbus-update-activation-environment",
                            "--systemd",
                            "DBUS_SESSION_BUS_ADDRESS",
                            "DISPLAY",
                            "XAUTHORITY",
                            NULL};
  run_bg(dbus_cmd);

  // --- Périphériques de saisie ---
  char *const kbd_cmd[] = {"setxkbmap", "fr", NULL};
  run_bg(kbd_cmd);
  char *const num_cmd[] = {"numlockx", "on", NULL};
  run_bg(num_cmd);

  // --- Agent SSH / GPG ---
  char ssh_sock_path[256];
  snprintf(ssh_sock_path, sizeof(ssh_sock_path),
           "/run/user/%d/gnupg/S.gpg-agent.ssh", getuid());
  setenv("SSH_AUTH_SOCK", ssh_sock_path, 1);

  char *const gpg_cmd[] = {"gpgconf", "--launch", "gpg-agent", NULL};
  run_bg(gpg_cmd);

  // --- 3. NETTOYAGE PRÉVENTIF DES SESSIONS SQUATTEUSES ---
  system("pkill -9 picom 2>/dev/null");
  system("pkill -9 nm-applet 2>/dev/null");
  system("pkill -9 dunst 2>/dev/null");

  // --- 4. SERVICES GRAPHIQUES DE BASE SIMULTANÉS ---
  char *const feh_cmd[] = {"feh",           "--no-fehbg",   "--bg-fill",
                           "--no-xinerama", wallpaper_path, NULL};
  run_bg(feh_cmd);
  char *const polkit_cmd[] = {
      "/usr/lib/polkit-gnome/polkit-gnome-authentication-agent-1", NULL};
  run_bg(polkit_cmd);
  char *const picom_cmd[] = {"picom", "--config", picom_conf_path, NULL};
  run_bg(picom_cmd);
  char *const dunst_cmd[] = {"dunst", NULL};
  run_bg(dunst_cmd);

  // --- 5. BLOC CHRONOMÉTRÉ A : GPG & AMC_SYNC ---
  if (fork() == 0) {
    sleep(2);
    system("gpg --clearsign <<< \"Bonjour Alban, déverrouillage pour "
           "amc_sync\" > /dev/null");
    sleep(1);
    system("systemctl --user start amc-sync.service");
    if (is_program_installed("notify-send")) {
      system("notify-send \"AMC-Sync\" \"GPG OK : Le service a démarré.\"");
    }
    _exit(0);
  }

  // --- 6. BLOC CHRONOMÉTRÉ B : APPLETS & CONFIG COMPORTEMENTALE ---
  if (fork() == 0) {
    sleep(3);
    system("openbox --reconfigure");

    if (is_program_installed("xdotool")) {
      system("xdotool mousemove_relative 1 1 && xdotool mousemove_relative -- "
             "-1 -1");
    }

    char *const udiskie_cmd[] = {"udiskie", NULL};
    run_bg(udiskie_cmd);
    char *const redshift_cmd[] = {"redshift", "-l",        "43.6:1.4",
                                  "-t",       "5700:3600", NULL};
    run_bg(redshift_cmd);
    char *const gkhal_cmd[] = {"gkhal", NULL};
    run_bg(gkhal_cmd);

    // Détections dynamiques du matériel
    if (access("/sys/class/bluetooth", F_OK) == 0 &&
        is_program_installed("blueman-applet")) {
      system("blueman-applet &");
    }
    if ((access("/sys/class/power_supply/BAT0", F_OK) == 0 ||
         access("/sys/class/power_supply/BAT1", F_OK) == 0) &&
        is_program_installed("cbatticon")) {
      system("cbatticon -n &");
    }
    _exit(0);
  }

  // --- 7. BLOC CHRONOMÉTRÉ C : APPLICATIONS PRINCIPALES ---
  if (fork() == 0) {
    sleep(5);
    char *const term_cmd[] = {"launcher-qterminal", NULL};
    run_bg(term_cmd);
    char *const ff_cmd[] = {"gfirefox", NULL};
    run_bg(ff_cmd);
    char *const mutt_cmd[] = {"gneomutt", NULL};
    run_bg(mutt_cmd);
    _exit(0);
  }

  // Applications lancées sans attente
  char *const gdesk_cmd[] = {"gdesktop", NULL};
  run_bg(gdesk_cmd);

  // --- 8. SURVEILLANCE CRITIQUE DE POLYBAR ---
  polybar_pid = fork();
  if (polybar_pid == 0) {
    char *const polybar_cmd[] = {"polybar", "--reload", "ma_barre", NULL};
    execvp(polybar_cmd[0], polybar_cmd);
    _exit(127);
  } else if (polybar_pid > 0) {
    int status;
    waitpid(polybar_pid, &status, 0); // Le manager attend ici
    cleanup_session(); // Polybar s'est arrêté $\rightarrow$ On balaie le bureau
  }

  return 0;
}