#include <arpa/inet.h>
#include <fcntl.h>
#include <gtk/gtk.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vte/vte.h>

#define PROGRAMME_NAME "net-doctor"
#define TARGET_IP "8.8.8.8"

// Structure épurée (plus besoin d'expander ni de terminal principal)
typedef struct {
  GtkWidget *info_label;
} AppWidgets;

// --- FONCTIONS UTILITAIRES ---

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

int is_network_route_active(const char *ip_target) {
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
  int res = connect(sock, (const struct sockaddr *)&serv, sizeof(serv));
  close(sock);
  return (res == 0) ? 1 : 0;
}

void get_cmd_output(char *const argv[], char *buf, int size) {
  memset(buf, 0, size);
  int pfd[2];
  if (pipe(pfd) == -1)
    return;
  if (fork() == 0) {
    close(pfd[0]);
    dup2(pfd[1], STDOUT_FILENO);
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull != -1) {
      dup2(devnull, STDERR_FILENO);
      close(devnull);
    }
    execvp(argv[0], argv);
    _exit(127);
  }
  close(pfd[1]);
  ssize_t n = read(pfd[0], buf, size - 1);
  if (n > 0)
    buf[strcspn(buf, "\r\n")] = '\0';
  close(pfd[0]);
  wait(NULL);
}

gboolean is_networking_enabled(void) {
  char status[32];
  char *const cmd[] = {"nmcli", "networking", "connectivity", NULL};
  get_cmd_output(cmd, status, sizeof(status));
  return (strlen(status) > 0 && strstr(status, "none") == NULL);
}

void refresh_info(GtkWidget *label) {
  char ip_addr[128] = "Non connecté", final_text[1024], dns_status[128];
  gboolean enabled = is_networking_enabled();

  // 1. Calcul et affichage du statut DNS
  int dns_res = -1;
  if (enabled && is_network_route_active(TARGET_IP)) {
    char *const ping_cmd[] = {"ping", "-c", "1", "-W", "1", TARGET_IP, NULL};
    char dummy[32];
    get_cmd_output(ping_cmd, dummy, sizeof(dummy));
    dns_res = is_network_route_active("1.1.1.1") ? 0 : -1;
  }
  snprintf(dns_status, sizeof(dns_status),
           (dns_res == 0) ? "🟢 DNS OK" : "🔴 DNS Échec");

  // 2. Récupération de l'adresse IP active
  char *const ip_cmd[] = {"ip", "route", "get", "1.1.1.1", NULL};
  char raw_route[256] = {0};
  get_cmd_output(ip_cmd, raw_route, sizeof(raw_route));
  char *src_ptr = strstr(raw_route, "src");
  if (src_ptr && enabled)
    sscanf(src_ptr, "src %127s", ip_addr);

  // 3. Génération du texte complet (Réseau, DNS, IP)
  snprintf(final_text, sizeof(final_text),
           "<span size='large'><b>Rapport Réseau</b></span>\n\n"
           "<b>Réseau global :</b> %s\n"
           "<b>Statut DNS :</b> %s\n"
           "<b>Adresse IP :</b> %s",
           enabled ? "<span color='green'>Activé</span>"
                   : "<span color='red'>Désactivé</span>",
           dns_status, ip_addr);

  gtk_label_set_markup(GTK_LABEL(label), final_text);
}

// --- GESTION DE LA FENÊTRE TERMINAL INDÉPENDANTE ---

static void on_terminal_child_exited(VteTerminal *terminal G_GNUC_UNUSED,
                                     gint status G_GNUC_UNUSED,
                                     gpointer user_data) {
  GtkWidget *term_window = GTK_WIDGET(user_data);
  gtk_widget_destroy(term_window);
}

void open_detached_terminal(const char *title, char **argv,
                            gpointer main_widgets) {
  AppWidgets *widgets = (AppWidgets *)main_widgets;

  GtkWidget *term_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(term_window), title);
  gtk_window_set_default_size(GTK_WINDOW(term_window), 800, 450);
  gtk_container_set_border_width(GTK_CONTAINER(term_window), 2);

  GtkWidget *terminal = vte_terminal_new();

  GdkRGBA color_green, color_bg;
  gdk_rgba_parse(&color_green, "#00FF00");
  gdk_rgba_parse(&color_bg, "#1A1A1A");
  vte_terminal_set_colors(VTE_TERMINAL(terminal), &color_green, &color_bg, NULL,
                          0);
  vte_terminal_set_scrollback_lines(VTE_TERMINAL(terminal), 500);

  gtk_container_add(GTK_CONTAINER(term_window), terminal);

  g_signal_connect(terminal, "child-exited",
                   G_CALLBACK(on_terminal_child_exited), term_window);
  g_signal_connect_swapped(term_window, "destroy", G_CALLBACK(refresh_info),
                           widgets->info_label);

  vte_terminal_spawn_async(VTE_TERMINAL(terminal), VTE_PTY_DEFAULT, NULL, argv,
                           NULL, G_SPAWN_DEFAULT, NULL, NULL, NULL, -1, NULL,
                           NULL, NULL);

  gtk_widget_show_all(term_window);
}

// --- CALLBACKS BOUTONS ---

void on_diagnostic_clicked(GtkWidget *btn G_GNUC_UNUSED, gpointer data) {
  char *const cmd[] = {
      "/bin/bash", "-c",
      "echo -e '\\e[1;34mAnalyse de la connexion...\\e[0m'; "
      "if ping -c 2 -W 2 8.8.8.8 > /dev/null 2>&1; then "
      "  echo -e '  \\e[1;32m✅ Liaison IP : OK\\e[0m'; "
      "  if ping -c 1 -W 2 google.fr > /dev/null 2>&1; then "
      "    echo -e '  \\e[1;32m✅ Résolution DNS : OK\\e[0m'; "
      "    echo -e '\\e[1;35m🎉 Connexion Internet Opérationnelle !\\e[0m'; "
      "  else "
      "    echo -e '  \\e[1;31m❌ Échec DNS (Impossible de résoudre "
      "google.fr)\\e[0m'; "
      "  fi; "
      "else "
      "  echo -e '  \\e[1;31m❌ Échec Liaison IP (Vérifiez votre "
      "Box/Câble)\\e[0m'; "
      "fi; "
      "echo -e '\\n\\e[2m[Appuyez sur Entrée pour fermer]\\e[0m'; read",
      NULL};
  open_detached_terminal("Diagnostic Connexion", (char **)cmd, data);
}

void on_nmtui_clicked(GtkWidget *btn G_GNUC_UNUSED, gpointer data) {
  if (!is_program_installed("nmtui"))
    return;
  char *const cmd[] = {"nmtui", NULL};
  open_detached_terminal("Configuration Réseau Interface", (char **)cmd, data);
}

void on_toggle_network(GtkToggleButton *src, gpointer data) {
  AppWidgets *widgets = (AppWidgets *)data;
  const char *state = gtk_toggle_button_get_active(src) ? "on" : "off";
  char *const cmd[] = {"nmcli", "networking", (char *)state, NULL};

  if (fork() == 0) {
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull != -1) {
      dup2(devnull, STDOUT_FILENO);
      dup2(devnull, STDERR_FILENO);
      close(devnull);
    }
    execvp(cmd[0], cmd);
    _exit(1);
  }
  wait(NULL);
  g_usleep(500000);
  refresh_info(widgets->info_label);
}

void on_restart_clicked(GtkWidget *btn G_GNUC_UNUSED, gpointer data) {
  AppWidgets *widgets = (AppWidgets *)data;
  char *const cmd[] = {"pkexec", "systemctl", "restart", "NetworkManager",
                       NULL};
  if (fork() == 0) {
    execvp(cmd[0], cmd);
    _exit(1);
  }
  wait(NULL);
  g_usleep(1000000);
  refresh_info(widgets->info_label);
}

void create_main_window(AppWidgets *widgets) {
  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "NetDoctor Pro");
  gtk_container_set_border_width(GTK_CONTAINER(window), 15);
  gtk_window_set_default_size(GTK_WINDOW(window), 400, 350);

  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
  gtk_container_add(GTK_CONTAINER(window), vbox);

  // Icône réseau
  GtkWidget *icon = gtk_image_new_from_icon_name("network-transmit-receive",
                                                 GTK_ICON_SIZE_DIALOG);
  gtk_box_pack_start(GTK_BOX(vbox), icon, FALSE, FALSE, 0);

  widgets->info_label = gtk_label_new(NULL);
  gtk_label_set_justify(GTK_LABEL(widgets->info_label), GTK_JUSTIFY_CENTER);
  gtk_box_pack_start(GTK_BOX(vbox), widgets->info_label, FALSE, FALSE, 5);

  GtkWidget *check_net =
      gtk_check_button_new_with_label("Activer le réseau global");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_net),
                               is_networking_enabled());
  g_signal_connect(check_net, "toggled", G_CALLBACK(on_toggle_network),
                   widgets);
  gtk_box_pack_start(GTK_BOX(vbox), check_net, FALSE, FALSE, 0);

  // --- BOUTONS ---
  GtkWidget *btn_diag =
      gtk_button_new_with_label("🔍 Lancer un diagnostic complet");
  g_signal_connect(btn_diag, "clicked", G_CALLBACK(on_diagnostic_clicked),
                   widgets);
  gtk_box_pack_start(GTK_BOX(vbox), btn_diag, FALSE, FALSE, 0);

  GtkWidget *btn_nmtui =
      gtk_button_new_with_label("🌐 Ouvrir la configuration réseau (nmtui)");
  g_signal_connect(btn_nmtui, "clicked", G_CALLBACK(on_nmtui_clicked), widgets);
  gtk_box_pack_start(GTK_BOX(vbox), btn_nmtui, FALSE, FALSE, 0);

  GtkWidget *btn_restart =
      gtk_button_new_with_label("🔄 Réinitialiser le service");
  g_signal_connect(btn_restart, "clicked", G_CALLBACK(on_restart_clicked),
                   widgets);
  gtk_box_pack_start(GTK_BOX(vbox), btn_restart, FALSE, FALSE, 0);

  GtkWidget *btn_close = gtk_button_new_with_label("Quitter");
  g_signal_connect(btn_close, "clicked", G_CALLBACK(gtk_main_quit), NULL);
  gtk_box_pack_start(GTK_BOX(vbox), btn_close, FALSE, FALSE, 5);

  refresh_info(widgets->info_label);
  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

  gtk_widget_show_all(window);
}

// --- MAIN (Épuré et minimaliste) ---

int main(int argc, char *argv[]) {
  prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
  gtk_init(&argc, &argv);

  AppWidgets *widgets = g_new0(AppWidgets, 1);

  // Appel de la fonction d'interface graphique
  create_main_window(widgets);

  gtk_main();
  g_free(widgets);
  return 0;
}