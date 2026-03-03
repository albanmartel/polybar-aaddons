#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <gtk/gtk.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// --- NOM PROGRAMME ---
#define PROGRAMME_NAME "net-doctor"

// --- Fonctions utilitaires ---

void get_command_output_safe(char *const argv[], char *buffer, int size) {
  // 1. Vérification des entrées (Sanity Check)
  if (buffer == NULL || size <= 0)
    return;

  // Initialiser le buffer pour éviter de lire de la mémoire sale
  memset(buffer, 0, size);

  if (argv == NULL || argv[0] == NULL) {
    snprintf(buffer, size, "Argument invalide");
    return;
  }

  // 2. Vérification de l'existence et de l'exécution du binaire
  // F_OK : fichier existe, X_OK : on a le droit de l'exécuter
  if (access(argv[0], F_OK | X_OK) != 0) {
    snprintf(buffer, size, "Binaire introuvable");
    return;
  }

  int pipefd[2];
  if (pipe(pipefd) == -1) {
    snprintf(buffer, size, "Erreur Pipe : %s", strerror(errno));
    return;
  }

  pid_t pid = fork();

  if (pid < 0) {
    snprintf(buffer, size, "Erreur Fork");
    close(pipefd[0]);
    close(pipefd[1]);
    return;
  }

  if (pid == 0) {
    // --- PROCESSUS ENFANT ---
    close(pipefd[0]); // Fermer la lecture côté enfant

    // Rediriger stdout vers le pipe
    if (dup2(pipefd[1], STDOUT_FILENO) == -1)
      _exit(1);

    // Rediriger stderr vers /dev/null
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull != -1) {
      dup2(devnull, STDERR_FILENO);
      close(devnull);
    }

    close(pipefd[1]);

    // Exécution sécurisée sans shell
    execv(argv[0], argv);

    // Si execv revient, c'est un échec critique
    _exit(127);
  } else {
    // --- PROCESSUS PARENT ---
    close(pipefd[1]); // Fermer l'écriture côté parent

    // Lecture sécurisée (on lit au maximum size-1 pour garder le \0)
    ssize_t n = read(pipefd[0], buffer, size - 1);
    if (n > 0) {
      buffer[n] = '\0';
      // Nettoyage sécurisé du retour à la ligne final
      buffer[strcspn(buffer, "\r\n")] = '\0';
    } else if (n == 0) {
      snprintf(buffer, size, "Aucune sortie");
    } else {
      snprintf(buffer, size, "Erreur lecture pipe");
    }

    close(pipefd[0]);

    // Attendre l'enfant pour éviter les processus zombies
    int status;
    waitpid(pid, &status, 0);
  }
}
// Vérifie si le réseau est activé globalement
gboolean is_networking_enabled() {
  char status[32]; // On augmente légèrement la taille par précaution

  // Préparation des arguments pour execv (le tableau doit finir par NULL)
  // On utilise le chemin absolu /usr/bin/nmcli pour la sécurité
  char *const cmd[] = {"/usr/bin/nmcli", "networking", "connectivity", NULL};

  // Appel de votre fonction sécurisée
  get_command_output_safe(cmd, status, sizeof(status));

  // nmcli connectivity peut renvoyer : full, limited, portal, none, unknown
  // On vérifie si le résultat contient "none" ou est vide (Erreur)
  if (strlen(status) == 0 || strstr(status, "none") != NULL) {
    return FALSE;
  }

  return TRUE;
}

int safe_ping_test() {
  pid_t pid = fork();

  if (pid == 0) { // Processus enfant
    // On redirige la sortie pour ne pas polluer la console
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);

    // On appelle le binaire directement via son chemin absolu
    // execv ne passe PAS par un shell, les injections sont IMPOSSIBLES
    char *args[] = {"ping", "-c", "1", "-W", "1", "8.8.8.8", NULL};
    execv("/usr/bin/ping", args);

    // Si execv échoue
    _exit(1);
  } else if (pid > 0) { // Processus parent
    int status;
    waitpid(pid, &status, 0);
    // On vérifie si le ping a réussi (code de sortie 0)
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0);
  }
  return 0; // Erreur fork
}

bool is_command_safe(const char *cmd) {
  // Caractères strictement interdits (Injections)
  const char *forbidden = ";$()`&>";

  // On autorise '|' car vous l'utilisez pour grep/awk
  // Mais on interdit tout ce qui permet d'enchaîner des commandes indépendantes
  if (strpbrk(cmd, forbidden) != NULL) {
    return false;
  }
  return true;
}

int get_exit_status_safe(char *const argv[]) {
  if (argv == NULL || argv[0] == NULL)
    return -1;

  pid_t pid = fork();
  if (pid == 0) {
    // Enfant : on cache les sorties
    int devnull = open("/dev/null", O_WRONLY);
    dup2(devnull, STDOUT_FILENO);
    dup2(devnull, STDERR_FILENO);
    close(devnull);

    execv(argv[0], argv);
    _exit(1); // Échec de l'exécution
  } else if (pid > 0) {
    int status;
    waitpid(pid, &status, 0);
    // Retourne 0 si la commande a réussi (ex: ping a répondu)
    return (WIFEXITED(status) ? WEXITSTATUS(status) : -1);
  }
  return -1;
}
// --- Actions ---

// Met à jour les infos du rapport
void refresh_info(GtkWidget *label) {
  if (label == NULL || !GTK_IS_LABEL(label))
    return;

  char dns_status[128], ip_addr[128], final_text[1024], net_enabled_text[128];

  // 1. État global
  gboolean enabled = is_networking_enabled();
  snprintf(net_enabled_text, sizeof(net_enabled_text),
           enabled ? "<span color='green'>Activé</span>"
                   : "<span color='red'>Désactivé</span>");

  // 2. Test DNS (UTILISATION SAFE MAINTENANT)
  char *const ping_cmd[] = {"/usr/bin/ping", "-c", "1", "-W", "1",
                            "8.8.8.8",       NULL};
  int dns_res = get_exit_status_safe(ping_cmd);

  snprintf(dns_status, sizeof(dns_status),
           (dns_res == 0) ? "🟢 DNS OK" : "🔴 DNS Échec");

  // 3. IP Locale
  char *const ip_cmd[] = {"/usr/bin/ip", "-4", "-br", "addr", "show", NULL};
  char raw_ip[256] = {0};
  get_command_output_safe(ip_cmd, raw_ip, sizeof(raw_ip));

  if (strlen(raw_ip) > 0) {
    // On cherche "UP" dans la ligne pour isoler l'IP
    char *ptr = strstr(raw_ip, "UP");
    if (ptr) {
      // Extraction après "UP"
      sscanf(ptr, "UP %127s", ip_addr);
      char *slash = strchr(ip_addr, '/');
      if (slash)
        *slash = '\0';
    } else {
      strncpy(ip_addr, "Non connecté", sizeof(ip_addr));
    }
  } else {
    strncpy(ip_addr, "N/A", sizeof(ip_addr));
  }

  // 4. Construction du rapport final
  snprintf(final_text, sizeof(final_text),
           "<span size='large'><b>Rapport Réseau</b></span>\n\n"
           "<b>Réseau global :</b> %s\n"
           "<b>Statut DNS :</b> %s\n"
           "<b>Adresse IP :</b> %s",
           net_enabled_text, dns_status, ip_addr);

  gtk_label_set_markup(GTK_LABEL(label), final_text);
}

// Callback pour ouvrir nmtui dans un terminal
void open_nmtui(GtkWidget *widget G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED) {
  // 1. Vérification optionnelle des entrées (GTK)
  // Ici widget et data peuvent être NULL car on ne s'en sert pas,
  // mais par principe on évite de traiter des données corrompues.

  // 2. Vérification des binaires avant d'agir
  const char *term_path = "/usr/bin/qterminal";
  const char *nmtui_path = "/usr/bin/nmtui";

  if (access(term_path, X_OK) != 0 || access(nmtui_path, X_OK) != 0) {
    g_printerr("Erreur : qterminal ou nmtui est introuvable.\n");
    return;
  }

  // 3. Double Fork pour lancer le terminal en arrière-plan proprement
  pid_t pid = fork();
  if (pid < 0)
    return;

  if (pid == 0) {
    // Dans l'enfant
    // On refait un fork pour que le petit-fils soit adopté par init (orphelin)
    // Cela permet au terminal de continuer à vivre quand NetDoctor ferme.
    if (fork() == 0) {
      // On prépare les arguments pour qterminal
      // La syntaxe pour qterminal est : qterminal -e nmtui
      char *const args[] = {"qterminal", "-e", "nmtui", NULL};

      // On ferme les sorties pour ne pas polluer le terminal d'appel
      int devnull = open("/dev/null", O_WRONLY);
      dup2(devnull, STDOUT_FILENO);
      dup2(devnull, STDERR_FILENO);

      execv(term_path, args);
      _exit(1); // Ne devrait pas arriver si access() a réussi
    }
    _exit(0); // L'enfant meurt tout de suite
  }

  // Le parent (NetDoctor) continue sa route instantanément
  waitpid(pid, NULL, 0);
}

// Callback pour la case à cocher
// Callback pour la case à cocher
void on_toggle_network(GtkToggleButton *source, gpointer user_data) {
  // 1. Validation des entrées
  if (source == NULL || user_data == NULL || !GTK_IS_LABEL(user_data)) {
    return;
  }

  GtkWidget *label = (GtkWidget *)user_data;
  gboolean active = gtk_toggle_button_get_active(source);

  // 2. Préparation des arguments pour nmcli
  // On définit l'état cible : "on" ou "off"
  const char *state = active ? "on" : "off";
  char *const nmcli_cmd[] = {"/usr/bin/nmcli", "networking", (char *)state,
                             NULL};

  // 3. Exécution sécurisée
  // On utilise fork/execv (via une petite version simplifiée ou votre fonction
  // safe)
  pid_t pid = fork();
  if (pid == 0) {
    // Enfant : exécute la commande en silence
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull != -1) {
      dup2(devnull, STDOUT_FILENO);
      dup2(devnull, STDERR_FILENO);
      close(devnull);
    }
    execv(nmcli_cmd[0], nmcli_cmd);
    _exit(1);
  } else if (pid > 0) {
    // Parent : attend la fin de la commande avant de continuer
    waitpid(pid, NULL, 0);
  }

  // 4. Rafraîchissement de l'interface
  // On garde le petit délai car NetworkManager met quelques millisecondes à
  // mettre à jour l'état
  g_usleep(500000);
  refresh_info(label);
}

void restart_network(GtkWidget *widget G_GNUC_UNUSED, gpointer data) {
  // 1. Vérification de sécurité du pointeur
  if (data == NULL || !GTK_IS_LABEL(data)) {
    return;
  }

  // 2. Préparation des arguments pour pkexec
  // On appelle systemctl via pkexec pour obtenir les droits root
  char *const restart_cmd[] = {"/usr/bin/pkexec", "/usr/bin/systemctl",
                               "restart", "NetworkManager", NULL};

  // 3. Exécution via fork/execv
  pid_t pid = fork();
  if (pid < 0)
    return;

  if (pid == 0) {
    // Enfant : on ne redirige PAS stderr ici car si pkexec échoue
    // (ex: l'utilisateur annule), on veut voir l'erreur dans la console.
    execv(restart_cmd[0], restart_cmd);
    _exit(1);
  } else {
    // Parent : on attend que l'utilisateur finisse avec la fenêtre pkexec
    int status;
    waitpid(pid, &status, 0);

    // On vérifie si la commande a réussi
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
      // Petite pause d'une seconde pour laisser le service redémarrer
      g_usleep(1000000);
    }
  }

  // 4. Rafraîchissement de l'UI
  refresh_info(GTK_WIDGET(data));
}

// --- Main ---

int main(int argc, char *argv[]) {
  prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
  gtk_init(&argc, &argv);

  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "NetDoctor - Diagnostic");
  gtk_container_set_border_width(GTK_CONTAINER(window), 20);
  gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
  gtk_container_add(GTK_CONTAINER(window), vbox);

  // Icône
  GtkWidget *icon = gtk_image_new_from_icon_name("network-transmit-receive",
                                                 GTK_ICON_SIZE_DIALOG);
  gtk_box_pack_start(GTK_BOX(vbox), icon, FALSE, FALSE, 0);

  // Label d'infos
  GtkWidget *info_label = gtk_label_new(NULL);
  gtk_label_set_justify(GTK_LABEL(info_label), GTK_JUSTIFY_CENTER);
  gtk_box_pack_start(GTK_BOX(vbox), info_label, FALSE, FALSE, 0);

  // CASE À COCHER : Activer le réseau
  GtkWidget *check_net =
      gtk_check_button_new_with_label("Activer le réseau global");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_net),
                               is_networking_enabled());
  g_signal_connect(check_net, "toggled", G_CALLBACK(on_toggle_network),
                   info_label);
  gtk_box_pack_start(GTK_BOX(vbox), check_net, FALSE, FALSE, 5);

  // Bouton Configurer (nmtui)
  GtkWidget *btn_nmtui =
      gtk_button_new_with_label("🌐 Configurer les connexions (nmtui)");
  g_signal_connect(btn_nmtui, "clicked", G_CALLBACK(open_nmtui), NULL);
  gtk_box_pack_start(GTK_BOX(vbox), btn_nmtui, FALSE, FALSE, 0);

  // Bouton Redémarrer
  GtkWidget *btn_restart =
      gtk_button_new_with_label("🔄 Réinitialiser le service");
  g_signal_connect(btn_restart, "clicked", G_CALLBACK(restart_network),
                   info_label);
  gtk_box_pack_start(GTK_BOX(vbox), btn_restart, FALSE, FALSE, 0);

  // Bouton Fermer
  GtkWidget *btn_close = gtk_button_new_with_label("Quitter");
  g_signal_connect(btn_close, "clicked", G_CALLBACK(gtk_main_quit), NULL);
  gtk_box_pack_start(GTK_BOX(vbox), btn_close, FALSE, FALSE, 0);

  refresh_info(info_label);

  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
  gtk_widget_show_all(window);
  gtk_main();

  return 0;
}
