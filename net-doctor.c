#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <gtk/gtk.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// --- NOM PROGRAMME ---
#define PROGRAMME_NAME "net-doctor"

// Cibles de test dynamiques
#define TARGET_IP "8.8.8.8"
#define TARGET_DOMAINE "google.fr"

// --- 1. FONCTIONS UTILITAIRES ---

/**
 * @brief Vérifie si un programme est présent et exécutable sur le système.
 */
int is_program_installed(const char *name_or_path) {
  if (name_or_path == NULL || strlen(name_or_path) == 0)
    return 0;

  if (name_or_path[0] == '/' || strncmp(name_or_path, "./", 2) == 0 ||
      strncmp(name_or_path, "../", 3) == 0) {
    return (access(name_or_path, F_OK | X_OK) == 0);
  }

  char *discovered_path = g_find_program_in_path(name_or_path);
  if (discovered_path != NULL) {
    g_free(discovered_path);
    return 1;
  }
  return 0;
}

/**
 * @brief Test de route réseau instantané via Socket UDP (0 ms).
 */
int is_network_route_active(const char *ip_target) {
  if (ip_target == NULL || strlen(ip_target) == 0)
    return 0;

  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0)
    return 0;

  struct sockaddr_in serv;
  memset(&serv, 0, sizeof(serv));
  serv.sin_family = AF_INET;
  serv.sin_port = htons(53);

  if (inet_pton(AF_INET, ip_target, &serv.sin_addr) != 1) {
    close(sock);
    return 0;
  }

  int result = connect(sock, (const struct sockaddr *)&serv, sizeof(serv));
  close(sock);
  return (result == 0) ? 1 : 0;
}

/**
 * @brief Exécute une commande de manière synchrone et sécurisée sans passer par
 * un Shell.
 */
int execute_command_safe(char *const argv[], gboolean hide_stderr) {
  if (argv == NULL || argv[0] == NULL)
    return -1;
  if (!is_program_installed(argv[0]))
    return -1;

  pid_t pid = fork();
  if (pid < 0)
    return -1;

  if (pid == 0) {
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull != -1) {
      dup2(devnull, STDOUT_FILENO);
      if (hide_stderr) {
        dup2(devnull, STDERR_FILENO);
      }
      close(devnull);
    }
    execvp(argv[0], argv);
    _exit(127);
  }

  int status;
  waitpid(pid, &status, 0);
  return (WIFEXITED(status) ? WEXITSTATUS(status) : -1);
}

/**
 * @brief Capture la sortie d'une commande.
 */
void get_command_output_safe(char *const argv[], char *buffer, int size) {
  if (buffer == NULL || size <= 0)
    return;
  memset(buffer, 0, size);
  if (argv == NULL || argv[0] == NULL || !is_program_installed(argv[0])) {
    snprintf(buffer, size, "Invalide/Introuvable");
    return;
  }

  int pipefd[2];
  if (pipe(pipefd) == -1)
    return;

  pid_t pid = fork();
  if (pid < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    return;
  }

  if (pid == 0) {
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO);
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull != -1) {
      dup2(devnull, STDERR_FILENO);
      close(devnull);
    }
    close(pipefd[1]);
    execvp(argv[0], argv);
    _exit(127);
  } else {
    close(pipefd[1]);
    ssize_t n = read(pipefd[0], buffer, size - 1);
    if (n > 0) {
      buffer[n] = '\0';
      buffer[strcspn(buffer, "\r\n")] = '\0';
    }
    close(pipefd[0]);
    waitpid(pid, NULL, 0);
  }
}

// --- 2. LOGIQUE DE DIAGNOSTIC DE BAS NIVEAU ---

/**
 * @brief Teste la connectivité via le binaire ping vers une IP publique.
 */
int test_ip_link(const char *ip_target) {
  if (ip_target == NULL || strlen(ip_target) == 0)
    return 0;
  char *const ping_cmd[] = {"ping", "-c", "1", "-W", "2", (char *)ip_target,
                            NULL};
  return (execute_command_safe(ping_cmd, TRUE) == 0) ? 1 : 0;
}

/**
 * @brief Teste la résolution de noms de domaine (DNS) via le binaire ping.
 */
int test_dns_resolution(const char *domain_target) {
  if (domain_target == NULL || strlen(domain_target) == 0)
    return 0;
  char *const ping_cmd[] = {"ping", "-c", "1", "-W", "2", (char *)domain_target,
                            NULL};
  return (execute_command_safe(ping_cmd, TRUE) == 0) ? 1 : 0;
}

/**
 * @brief Vérifie l'état de la connexion Internet par paliers.
 */
void verifier_internet(const char *ip, const char *domain) {
  if (ip == NULL || strlen(ip) == 0 || domain == NULL || strlen(domain) == 0)
    return;

  printf("📊 Analyse approfondie de la connexion Internet...\n");
  printf("--------------------------------------------------\n");

  // Étape 1 : Route locale (Socket)
  if (!is_network_route_active(ip)) {
    printf(
        "❌ Pas de réseau (Aucune interface locale capable de joindre %s).\n",
        ip);
    return;
  }
  printf("⚙️ Carte réseau / Interface locale : ACTIVE.\n");

  // Étape 2 : Liaison IP
  printf("📡 Test de liaison IP extérieure (%s)...\n", ip);
  if (test_ip_link(ip)) {
    printf("  ✅ Liaison IP fonctionnelle.\n");

    // Étape 3 : Résolution DNS
    printf("🔎 Test de résolution DNS (%s)...\n", domain);
    if (test_dns_resolution(domain)) {
      printf("\n🎉 INTERNET OK : Accès total et DNS opérationnel !\n");
    } else {
      printf("\n⚠️ ERREUR DNS : Liaison IP OK, mais impossible de traduire les "
             "noms.\n");
    }
  } else {
    printf("❌ ÉCHEC : L'IP extérieure ne répond pas (Problème de Box/FAI).\n");
  }
}

gboolean is_networking_enabled(void) {
  char status[32];
  char *const cmd[] = {"nmcli", "networking", "connectivity", NULL};
  get_command_output_safe(cmd, status, sizeof(status));

  if (strlen(status) == 0 || strstr(status, "none") != NULL ||
      strstr(status, "Introuvable") != NULL) {
    return FALSE;
  }
  return TRUE;
}

// --- 3. ACTIONS & CALLBACKS GTK ---

// Action : Ouvre la fenêtre Zenity et lance la fonction verifier_internet
void open_diagnostic_gui(GtkWidget *widget G_GNUC_UNUSED,
                         gpointer data G_GNUC_UNUSED) {
  if (!is_program_installed("zenity")) {
    g_printerr("Erreur : 'zenity' est requis pour le diagnostic graphique.\n");
    return;
  }

  // Préparation de la fenêtre Zenity
  FILE *zenity_pipe =
      popen("zenity --text-info --title='Diagnostic Réseau Approfondi' "
            "--width=500 --height=350 --font='Monospace 10'",
            "w");
  if (zenity_pipe == NULL)
    return;

  // Redirection temporaire de STDOUT vers Zenity
  int stdout_backup = dup(STDOUT_FILENO);
  int zenity_fd = fileno(zenity_pipe);

  if (stdout_backup != -1 && zenity_fd != -1) {
    if (dup2(zenity_fd, STDOUT_FILENO) != -1) {

      // Appel de la fonction (toutes ses sorties printf iront dans Zenity)
      verifier_internet(TARGET_IP, TARGET_DOMAINE);

      fflush(stdout);
    }
    // Restauration de la console d'origine
    dup2(stdout_backup, STDOUT_FILENO);
    close(stdout_backup);
  }
  pclose(zenity_pipe);
}

void refresh_info(GtkWidget *label) {
  if (label == NULL || !GTK_IS_LABEL(label))
    return;

  char dns_status[128], ip_addr[128], final_text[1024], net_enabled_text[128];

  // 1. État global NetworkManager
  gboolean enabled = is_networking_enabled();
  snprintf(net_enabled_text, sizeof(net_enabled_text),
           enabled ? "<span color='green'>Activé</span>"
                   : "<span color='red'>Désactivé</span>");

  // 2. Test DNS intelligent (Fast-Pass via socket d'abord, puis ping)
  int dns_res = -1;
  if (enabled && is_network_route_active(TARGET_IP)) {
    char *const ping_cmd[] = {"ping", "-c", "1", "-W", "1", TARGET_IP, NULL};
    dns_res = execute_command_safe(ping_cmd, TRUE);
  }
  snprintf(dns_status, sizeof(dns_status),
           (dns_res == 0) ? "🟢 DNS OK" : "🔴 DNS Échec");

  // 3. IP Locale de la route par défaut (Évite le spam des interfaces lo,
  // docker, vpn...) 'ip route get 1.1.1.1' demande à l'OS quelle interface et
  // quelle IP locale il va utiliser.
  char *const ip_cmd[] = {"ip", "route", "get", "1.1.1.1", NULL};
  char raw_route[256] = {0};
  get_command_output_safe(ip_cmd, raw_route, sizeof(raw_route));

  // On cherche le mot clé "src" dans la réponse (ex: "1.1.1.1 dev wlan0 src
  // 192.168.1.42 uid 1000")
  char *src_ptr = strstr(raw_route, "src");
  if (src_ptr && enabled) {
    // On extrait l'adresse IP qui se trouve juste après "src"
    sscanf(src_ptr, "src %127s", ip_addr);
  } else {
    strncpy(ip_addr, "Non connecté", sizeof(ip_addr));
  }

  // 4. Rapport d'affichage
  snprintf(final_text, sizeof(final_text),
           "<span size='large'><b>Rapport Réseau</b></span>\n\n"
           "<b>Réseau global :</b> %s\n"
           "<b>Statut DNS :</b> %s\n"
           "<b>Adresse IP :</b> %s",
           net_enabled_text, dns_status, ip_addr);

  gtk_label_set_markup(GTK_LABEL(label), final_text);
}

void open_nmtui(GtkWidget *widget G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED) {
  if (!is_program_installed("qterminal") || !is_program_installed("nmtui")) {
    g_printerr("Erreur : 'qterminal' ou 'nmtui' est manquant.\n");
    return;
  }

  pid_t pid = fork();
  if (pid < 0)
    return;

  if (pid == 0) {
    if (fork() == 0) {
      char *const args[] = {"qterminal", "-e", "nmtui", NULL};
      int devnull = open("/dev/null", O_WRONLY);
      dup2(devnull, STDOUT_FILENO);
      dup2(devnull, STDERR_FILENO);
      close(devnull);

      execvp(args[0], args);
      _exit(1);
    }
    _exit(0);
  }
  waitpid(pid, NULL, 0);
}

void on_toggle_network(GtkToggleButton *source, gpointer user_data) {
  if (source == NULL || user_data == NULL || !GTK_IS_LABEL(user_data))
    return;

  GtkWidget *label = (GtkWidget *)user_data;
  const char *state = gtk_toggle_button_get_active(source) ? "on" : "off";
  char *const nmcli_cmd[] = {"nmcli", "networking", (char *)state, NULL};

  execute_command_safe(nmcli_cmd, TRUE);
  g_usleep(500000);
  refresh_info(label);
}

void restart_network(GtkWidget *widget G_GNUC_UNUSED, gpointer data) {
  if (data == NULL || !GTK_IS_LABEL(data))
    return;

  char *const restart_cmd[] = {"pkexec", "systemctl", "restart",
                               "NetworkManager", NULL};
  int status = execute_command_safe(restart_cmd, FALSE);

  if (status == 0) {
    g_usleep(1000000);
  }
  refresh_info(GTK_WIDGET(data));
}

// --- 4. MAIN ---

int main(int argc, char *argv[]) {
  prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
  gtk_init(&argc, &argv);

  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "NetDoctor - Diagnostic");
  gtk_container_set_border_width(GTK_CONTAINER(window), 20);
  gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
  gtk_container_add(GTK_CONTAINER(window), vbox);

  GtkWidget *icon = gtk_image_new_from_icon_name("network-transmit-receive",
                                                 GTK_ICON_SIZE_DIALOG);
  gtk_box_pack_start(GTK_BOX(vbox), icon, FALSE, FALSE, 0);

  GtkWidget *info_label = gtk_label_new(NULL);
  gtk_label_set_justify(GTK_LABEL(info_label), GTK_JUSTIFY_CENTER);
  gtk_box_pack_start(GTK_BOX(vbox), info_label, FALSE, FALSE, 0);

  GtkWidget *check_net =
      gtk_check_button_new_with_label("Activer le réseau global");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_net),
                               is_networking_enabled());
  g_signal_connect(check_net, "toggled", G_CALLBACK(on_toggle_network),
                   info_label);
  gtk_box_pack_start(GTK_BOX(vbox), check_net, FALSE, FALSE, 5);

  // NOUVEAU : Bouton Diagnostic Réseau (Zenity)
  GtkWidget *btn_diag =
      gtk_button_new_with_label("🔍 Lancer un diagnostic complet");
  g_signal_connect(btn_diag, "clicked", G_CALLBACK(open_diagnostic_gui), NULL);
  gtk_box_pack_start(GTK_BOX(vbox), btn_diag, FALSE, FALSE, 0);

  GtkWidget *btn_nmtui =
      gtk_button_new_with_label("🌐 Configurer les connexions (nmtui)");
  g_signal_connect(btn_nmtui, "clicked", G_CALLBACK(open_nmtui), NULL);
  gtk_box_pack_start(GTK_BOX(vbox), btn_nmtui, FALSE, FALSE, 0);

  GtkWidget *btn_restart =
      gtk_button_new_with_label("🔄 Réinitialiser le service");
  g_signal_connect(btn_restart, "clicked", G_CALLBACK(restart_network),
                   info_label);
  gtk_box_pack_start(GTK_BOX(vbox), btn_restart, FALSE, FALSE, 0);

  GtkWidget *btn_close = gtk_button_new_with_label("Quitter");
  g_signal_connect(btn_close, "clicked", G_CALLBACK(gtk_main_quit), NULL);
  gtk_box_pack_start(GTK_BOX(vbox), btn_close, FALSE, FALSE, 0);

  refresh_info(info_label);

  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
  gtk_widget_show_all(window);
  gtk_main();

  return 0;
}