#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>

// --- NOM PROGRAMME ---
#define PROGRAMME_NAME "powermenu"

/**
 * Exécute la commande appropriée selon l'environnement.
 * Sous Openbox/X11, lxqt-leave est souvent plus fiable pour communiquer avec le session manager.
 * Sous Wayland, loginctl est la norme.
 */
void execute_system_command(const char *action) {
    char cmd[128];
    int has_lxqt = (system("command -v lxqt-leave >/dev/null 2>&1") == 0);

    if (strcmp(action, "shutdown") == 0) {
        if (has_lxqt) strcpy(cmd, "lxqt-leave --shutdown");
        else strcpy(cmd, "loginctl poweroff");
    } 
    else if (strcmp(action, "reboot") == 0) {
        if (has_lxqt) strcpy(cmd, "lxqt-leave --reboot");
        else strcpy(cmd, "loginctl reboot");
    } 
    else if (strcmp(action, "suspend") == 0) {
        // loginctl suspend est généralement universel
        strcpy(cmd, "loginctl suspend");
    } 
    else if (strcmp(action, "logout") == 0) {
        if (getenv("WAYLAND_DISPLAY") != NULL) {
            strcpy(cmd, "loginctl terminate-session self");
        } else {
            strcpy(cmd, "openbox --exit");
        }
    }

    // Nettoyage rapide avant exécution
    if (getenv("DISPLAY") != NULL && system("command -v wmctrl >/dev/null 2>&1") == 0) {
        system("wmctrl -l | awk '{print $1}' | xargs -I{} wmctrl -ic {} 2>/dev/null");
    }

    system(cmd);
    gtk_main_quit();
}

gboolean confirmer(const char *message) {
    GtkWidget *dialog = gtk_message_dialog_new(NULL, 
        GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, 
        GTK_BUTTONS_YES_NO, "Voulez-vous vraiment %s ?", message);
    gtk_window_set_title(GTK_WINDOW(dialog), "Confirmation");
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
    gint result = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    return (result == GTK_RESPONSE_YES);
}

// Callbacks simplifiés
void on_power_off() { if (confirmer("éteindre l'ordinateur")) execute_system_command("shutdown"); }
void on_reboot()    { if (confirmer("redémarrer le système")) execute_system_command("reboot"); }
void on_suspend()   { execute_system_command("suspend"); }
void on_logout()    { if (confirmer("quitter la session")) execute_system_command("logout"); }

gboolean on_key_press(GtkWidget *widget G_GNUC_UNUSED, GdkEventKey *event, gpointer user_data G_GNUC_UNUSED) {
    if (event->keyval == GDK_KEY_Escape) {
        gtk_main_quit();
        return TRUE;
    }
    return FALSE;
}

GtkWidget* create_menu_button(const char *label_text, const char *icon_name, GCallback cb) {
    GtkWidget *button = gtk_button_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_DND);
    GtkWidget *label = gtk_label_new(label_text);
    
    gtk_container_set_border_width(GTK_CONTAINER(button), 5);
    gtk_box_pack_start(GTK_BOX(box), icon, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
    gtk_container_add(GTK_CONTAINER(button), box);
    g_signal_connect(button, "clicked", cb, NULL);
    return button;
}

int main(int argc, char *argv[]) {
	prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Menu Système");
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(window), 20);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

    // Style CSS
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        "button { min-width: 280px; padding: 12px; border-radius: 6px; font-size: 14px; }"
        "window { background-color: #2e3440; }", -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press), NULL);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    gtk_box_pack_start(GTK_BOX(vbox), create_menu_button("Éteindre", "system-shutdown", G_CALLBACK(on_power_off)), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), create_menu_button("Redémarrer", "system-reboot", G_CALLBACK(on_reboot)), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), create_menu_button("Mise en veille", "system-suspend", G_CALLBACK(on_suspend)), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), create_menu_button("Déconnexion", "system-log-out", G_CALLBACK(on_logout)), TRUE, TRUE, 0);

    gtk_widget_show_all(window);
    gtk_main();
    return 0;
}
