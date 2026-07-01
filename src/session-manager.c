#include <errno.h>
#include <pwd.h>
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

// Configuration des chemins et environnement
#define PATH_WALLPAPER "~/.config/openbox/Images/IMG_20240821_104051.jpg"
#define PATH_AUTOSTART                                                         \
  "~/.config/session-manager/autostart" // Emplacement de ton fichier texte
#define ENV_LANG "fr_FR.UTF-8"
#define ENV_LC_TIME "fr_FR.UTF-8"
#define ENV_MONITOR "DP-1"
#define POLYBAR_BAR_NAME "ma_barre"

#define MAX_PATH_SIZE 512
#define MAX_LINE_SIZE 1024
#define MAX_RUNNING_APPS 128

const char *const gpg_kill_args[] = {"gpgconf", "--kill", "gpg-agent", NULL};
pid_t polybar_pid = 0;

// Structure d'instance pour le suivi et la fermeture propre des processus en
// arrière-plan
typedef struct {
  pid_t pid;
  char name[64];
} RuntimeProcess;

RuntimeProcess session_pids[MAX_RUNNING_APPS] = {0};
size_t num_running_apps = 0;

// --- UTILS AVEC PROGRAMMATION DÉFENSIVE ---

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

void resolve_home_path(const char *src, char *dest, size_t dest_len) {
  if (!src || !dest || dest_len == 0)
    return;

  // 1. On tente de récupérer la variable d'environnement $HOME
  const char *home = getenv("HOME");

  // 2. Si $HOME n'existe pas, on demande proprement au système
  if (!home || home[0] == '\0') {
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_dir && pw->pw_dir[0] != '\0') {
      home = pw->pw_dir;
    } else {
      // Solution de secours ultime et neutre si TOUT échoue
      home = "/tmp";
    }
  }

  // 3. Résolution du chemin
  if (src[0] == '~') {
    int written = snprintf(dest, dest_len, "%s%s", home, src + 1);
    if (written < 0 || (size_t)written >= dest_len)
      dest[0] = '\0';
  } else {
    strncpy(dest, src, dest_len - 1);
    dest[dest_len - 1] = '\0';
  }
}

// Fonction générique pour déléguer l'exécution d'une ligne au Shell
pid_t launch_cmd_line(const char *cmd_line) {
  // Programmation défensive : si le pointeur est NULL ou la chaîne vide
  if (!cmd_line || cmd_line[0] == '\0') {
    return -1;
  }

  pid_t pid = fork();
  if (pid < 0)
    return -1;

  if (pid == 0) {
    if (freopen("/dev/null", "w", stdout) == NULL ||
        freopen("/dev/null", "w", stderr) == NULL) {
      _exit(1);
    }
    execl("/bin/sh", "sh", "-c", cmd_line, (char *)NULL);
    _exit(127);
  }
  return pid;
}

void terminate_all_apps(void) {
  printf("[Session] Fermeture ordonnée des applications...\n");
  size_t i = num_running_apps;
  while (i--) {
    if (session_pids[i].pid > 0) {
      kill(session_pids[i].pid, SIGTERM);
    }
  }

  if (is_program_installed(gpg_kill_args[0])) {
    pid_t gpg_pid = fork();
    if (gpg_pid == 0) {
      if (freopen("/dev/null", "w", stdout) == NULL ||
          freopen("/dev/null", "w", stderr) == NULL) {
        _exit(1);
      }
      execvp(gpg_kill_args[0], (char *const *)gpg_kill_args);
      _exit(127);
    }
  }
}

void cleanup_session(void) {
  printf("[Supervisor] Nettoyage et fermeture...\n");
  signal(SIGTERM, SIG_IGN);
  signal(SIGINT, SIG_IGN);

  terminate_all_apps();

  if (is_program_installed("openbox")) {
    pid_t ob_pid = fork();
    if (ob_pid == 0) {
      char *const ob_args[] = {"openbox", "--exit", NULL};
      if (freopen("/dev/null", "w", stdout) == NULL ||
          freopen("/dev/null", "w", stderr) == NULL) {
        _exit(1);
      }
      execvp(ob_args[0], ob_args);
      _exit(127);
    }
  }

  sleep(1);

  if (freopen("/dev/null", "w", stdout) != NULL &&
      freopen("/dev/null", "w", stderr) != NULL) {
    execlp("loginctl", "loginctl", "terminate-session", "self", (char *)NULL);
  }
  exit(0);
}

void handle_signal(int sig) {
  (void)sig;
  cleanup_session();
}

// --- SÉQUENCE PRINCIPALE ---
void launch_session(void) {
  char wallpaper_resolved[MAX_PATH_SIZE];
  resolve_home_path(PATH_WALLPAPER, wallpaper_resolved,
                    sizeof(wallpaper_resolved));

  // 1. VARIABLE D'ENVIRONNEMENT DE BASE
  if (setenv("LANG", ENV_LANG, 1) != 0 ||
      setenv("LC_TIME", ENV_LC_TIME, 1) != 0 ||
      setenv("MONITOR", ENV_MONITOR, 1) != 0 ||
      (wallpaper_resolved[0] != '\0' &&
       setenv("WALLPAPER_PATH", wallpaper_resolved, 1) != 0)) {
    fprintf(stderr, "[Erreur] Variables d'environnement.\n");
  }

  char uid_str[64];
  if (snprintf(uid_str, sizeof(uid_str), "unix:path=/run/user/%d/bus",
               getuid()) > 0) {
    setenv("DBUS_SESSION_BUS_ADDRESS", uid_str, 1);
  }

  char ssh_sock_path[MAX_PATH_SIZE];
  if (snprintf(ssh_sock_path, sizeof(ssh_sock_path),
               "/run/user/%d/gnupg/S.gpg-agent.ssh", getuid()) > 0) {
    setenv("SSH_AUTH_SOCK", ssh_sock_path, 1);
  }

  // 2. BARRIÈRE DE SÉCURITÉ REFORCÉE GPG (3 ESSAIS CHRONOMÉTRÉS)
  int essais = 0;
  int gpg_ok = 0;

  (void)system("gpgconf --launch gpg-agent");

  while (essais < 3) {
    const char *gpg_cmd = "gpg --batch --yes --clearsign <<< \"Verrou de "
                          "Securite Session\" > /dev/null";
    pid_t gpg_pid = fork();

    if (gpg_pid == 0) {
      execl("/bin/sh", "sh", "-c", gpg_cmd, (char *)NULL);
      _exit(127);
    }

    int status;
    while (waitpid(gpg_pid, &status, 0) < 0) {
      if (errno == EINTR)
        continue;
      break;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
      gpg_ok = 1;
      break;
    } else {
      essais++;
      if (essais < 3 && is_program_installed("notify-send")) {
        (void)system("notify-send -u critical \"Sécurité\" \"Phrase secrète "
                     "incorrecte. Réessayez.\"");
      }
    }
  }

  if (!gpg_ok) {
    if (is_program_installed("notify-send")) {
      (void)system("notify-send -u critical \"Sécurité\" \"3 échecs. Fermeture "
                   "immédiate...\"");
      sleep(1);
    }
    cleanup_session();
    return;
  }

  // 3. PARSAGE ET LANCEUR SÉQUENTIEL DE L'AUTOSTART EXTERNE TEXTE
  char config_path[MAX_PATH_SIZE];
  resolve_home_path(PATH_AUTOSTART, config_path, sizeof(config_path));

  FILE *fp = fopen(config_path, "r");
  if (!fp) {
    fprintf(stderr, "[Erreur Critique] Impossible d'ouvrir l'autostart : %s\n",
            config_path);
    cleanup_session();
    return;
  }

  char line[MAX_LINE_SIZE];
  while (fgets(line, sizeof(line), fp)) {
    char *cmd = line;
    // Supprimer les espaces au début
    while (*cmd == ' ' || *cmd == '\t')
      cmd++;

    // Ignorer les commentaires et lignes vides
    if (*cmd == '\0' || *cmd == '\n' || *cmd == '\r' || *cmd == '#')
      continue;

    // Supprimer les retours à la ligne et espaces en fin de ligne
    size_t len = strlen(cmd);
    while (len > 0 && (cmd[len - 1] == '\n' || cmd[len - 1] == '\r' ||
                       cmd[len - 1] == ' ' || cmd[len - 1] == '\t')) {
      cmd[--len] = '\0';
    }

    // Détection du mode arrière-plan via '&'
    int background = 0;
    if (len > 0 && cmd[len - 1] == '&') {
      background = 1;
      cmd[len - 1] = '\0';
      // Nettoyer les espaces résiduels se trouvant juste avant le '&'
      while (len > 1 && (cmd[len - 2] == ' ' || cmd[len - 2] == '\t')) {
        cmd[--len - 1] = '\0';
      }
    }

    // Exécution de la commande lue
    pid_t pid = launch_cmd_line(cmd);
    if (pid > 0) {
      if (!background) {
        // Mode Impératif : On bloque et on attend que le processus se termine
        int status;
        while (waitpid(pid, &status, 0) < 0) {
          if (errno == EINTR)
            continue;
          break;
        }
      } else {
        // Mode Arrière-plan : On l'ajoute dans notre registre pour le clean
        // final
        if (num_running_apps < MAX_RUNNING_APPS) {
          session_pids[num_running_apps].pid = pid;
          sscanf(cmd, "%63s", session_pids[num_running_apps].name);
          num_running_apps++;
        }
      }
    }
  }
  fclose(fp);

  // 4. SURVEILLANCE DE POLYBAR (Main-loop de la session)
  polybar_pid = fork();
  if (polybar_pid < 0)
    cleanup_session();

  if (polybar_pid == 0) {
    char *const polybar_cmd[] = {"polybar", "--reload", POLYBAR_BAR_NAME, NULL};
    execvp(polybar_cmd[0], polybar_cmd);
    _exit(127);
  } else {
    int status;
    while (waitpid(polybar_pid, &status, 0) < 0) {
      if (errno == EINTR)
        continue;
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
  }

  if (setpgid(0, 0) < 0) {
    exit(EXIT_FAILURE);
  }

  struct sigaction sa;
  sa.sa_handler = sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
  sigaction(SIGCHLD, &sa, NULL);

  if (signal(SIGINT, handle_signal) == SIG_ERR ||
      signal(SIGTERM, handle_signal) == SIG_ERR) {
    exit(EXIT_FAILURE);
  }

  launch_session();
  return 0;
}