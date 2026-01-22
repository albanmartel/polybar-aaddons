#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/prctl.h>

// --- NOM PROGRAMME ---
#define PROGRAMME_NAME "net-doctor"

// --- Fonctions utilitaires ---

void get_command_output(const char* cmd, char* buffer, int size) {
    FILE *fp = popen(cmd, "r");
    if (fp == NULL) {
        snprintf(buffer, size, "Erreur");
        return;
    }
    if (fgets(buffer, size, fp) == NULL) {
        snprintf(buffer, size, "N/A");
    }
    pclose(fp);
}

// Vérifie si le réseau est activé globalement
gboolean is_networking_enabled() {
    char status[16];
    get_command_output("nmcli networking connectivity", status, sizeof(status));
    return (strncmp(status, "none", 4) != 0);
}

// --- Actions ---

// Met à jour les infos du rapport
void refresh_info(GtkWidget *label) {
    char dns_status[128], ip_addr[128], final_text[512], net_enabled_text[64];

    // 1. État global
    gboolean enabled = is_networking_enabled();
    snprintf(net_enabled_text, sizeof(net_enabled_text), 
             enabled ? "<span color='green'>Activé</span>" : "<span color='red'>Désactivé</span>");

    // 2. Test DNS
    int dns_res = system("ping -c 1 -W 1 8.8.8.8 > /dev/null 2>&1");
    snprintf(dns_status, sizeof(dns_status), (dns_res == 0) ? "🟢 DNS OK" : "🔴 DNS Échec");

    // 3. IP Locale
    get_command_output("ip -4 addr show | grep 'inet ' | grep -v '127.0.0.1' | awk '{print $2}' | cut -d/ -f1 | head -n1", ip_addr, sizeof(ip_addr));

    snprintf(final_text, sizeof(final_text), 
             "<span size='large'><b>Rapport Réseau</b></span>\n\n"
             "<b>Réseau global :</b> %s\n"
             "<b>Statut DNS :</b> %s\n"
             "<b>Adresse IP :</b> %s", 
             net_enabled_text, dns_status, ip_addr);

    gtk_label_set_markup(GTK_LABEL(label), final_text);
}

// Callback pour la case à cocher
void on_toggle_network(GtkToggleButton *source, gpointer user_data) {
    GtkWidget *label = (GtkWidget *)user_data;
    gboolean active = gtk_toggle_button_get_active(source);
    
    if (active) {
        system("nmcli networking on");
    } else {
        system("nmcli networking off");
    }
    
    // Petite pause pour laisser NetworkManager réagir avant de rafraîchir
    g_usleep(500000); 
    refresh_info(label);
}

void restart_network(GtkWidget *widget G_GNUC_UNUSED, gpointer data) {
    system("pkexec systemctl restart NetworkManager");
    // On attend un peu et on rafraîchit
    g_usleep(1000000);
    refresh_info((GtkWidget *)data);
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
    GtkWidget *icon = gtk_image_new_from_icon_name("network-transmit-receive", GTK_ICON_SIZE_DIALOG);
    gtk_box_pack_start(GTK_BOX(vbox), icon, FALSE, FALSE, 0);

    // Label d'infos
    GtkWidget *info_label = gtk_label_new(NULL);
    gtk_label_set_justify(GTK_LABEL(info_label), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(vbox), info_label, FALSE, FALSE, 0);

    // CASE À COCHER : Activer le réseau
    GtkWidget *check_net = gtk_check_button_new_with_label("Activer le réseau global");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_net), is_networking_enabled());
    g_signal_connect(check_net, "toggled", G_CALLBACK(on_toggle_network), info_label);
    gtk_box_pack_start(GTK_BOX(vbox), check_net, FALSE, FALSE, 5);

    // Bouton Redémarrer
    GtkWidget *btn_restart = gtk_button_new_with_label("🔄 Réinitialiser le service");
    g_signal_connect(btn_restart, "clicked", G_CALLBACK(restart_network), info_label);
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
