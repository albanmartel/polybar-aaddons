#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>

// --- NOM PROGRAMME ---
#define PROGRAMME_NAME "help-center"

typedef struct {
    GtkWidget *window;
    GtkWidget *stack;
    GtkWidget *man_entry;
    GtkWidget *man_listbox;
    GtkWidget *surf_entry;
    GtkWidget *surf_combo;
} AppWidgets;

// --- FONCTIONS DE NAVIGATION ---
// Utilisation de G_GNUC_UNUSED pour supprimer les alertes sur "widget"
void go_to_main(GtkWidget *widget G_GNUC_UNUSED, gpointer data) {
    gtk_stack_set_visible_child_name(GTK_STACK(data), "main");
}

void go_to_man(GtkWidget *widget G_GNUC_UNUSED, gpointer data) {
    gtk_stack_set_visible_child_name(GTK_STACK(data), "man");
}

void go_to_surf(GtkWidget *widget G_GNUC_UNUSED, gpointer data) {
    gtk_stack_set_visible_child_name(GTK_STACK(data), "surf");
}

// --- LOGIQUE MAN ---
void on_man_search_changed(GtkSearchEntry *entry, gpointer data) {
    AppWidgets *app = (AppWidgets *)data;
    const char *text = gtk_entry_get_text(GTK_ENTRY(entry));
    
    GList *children = gtk_container_get_children(GTK_CONTAINER(app->man_listbox));
    for (GList *l = children; l != NULL; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);

    if (strlen(text) < 2) return;

    // Utilisation de g_strdup_printf pour la sécurité au lieu de sprintf
    char *cmd = g_strdup_printf("man -k %s | head -n 10", text);
    FILE *fp = popen(cmd, "r");
    g_free(cmd);

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char *token = strtok(line, " ");
        if (token) {
            GtkWidget *btn = gtk_button_new_with_label(line);
            gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
            char *page = g_strdup(token);
            // Attention : g_strdup_printf ici crée une chaîne qui fuit si non libérée, 
            // mais system() est bloquant ici. Pour faire propre, on passe par un callback.
            g_signal_connect_swapped(btn, "clicked", G_CALLBACK(system), 
                g_strdup_printf("export QT_QPA_PLATFORM=xcb && konsole -e man %s", page));
            gtk_container_add(GTK_CONTAINER(app->man_listbox), btn);
            g_free(page);
        }
    }
    pclose(fp);
    gtk_widget_show_all(app->man_listbox);
}

// --- LOGIQUE SURFRAW ---
void on_surfraw_exec(GtkWidget *widget G_GNUC_UNUSED, gpointer data) {
    AppWidgets *app = (AppWidgets *)data;
    char *engine = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(app->surf_combo));
    const char *query = gtk_entry_get_text(GTK_ENTRY(app->surf_entry));
    
    if (strlen(query) > 0) {
        char *cmd = g_strdup_printf("surfraw %s '%s'", engine ? engine : "duckduckgo", query);
        system(cmd);
        g_free(cmd);
    }
    g_free(engine);
}

void launch_syslog(GtkWidget *widget G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED) {
    // Lance le script directement en arrière-plan
    system("~/.config/polybar/scripts/syslog_report &");
}

int main(int argc, char *argv[]) {
    prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
    gtk_init(&argc, &argv);
    
    AppWidgets *app = g_malloc0(sizeof(AppWidgets));

    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->window), "Centre d'Aide Arch");
    gtk_window_set_default_size(GTK_WINDOW(app->window), 400, 500);
    
    app->stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(app->stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);

    // --- PAGE 1 : MENU PRINCIPAL ---
    GtkWidget *vbox_main = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox_main), 20);

    GtkWidget *btn_shortcuts = gtk_button_new_with_label("📖 Guide des raccourcis");
    g_signal_connect_swapped(btn_shortcuts, "clicked", G_CALLBACK(system), 
        "zenity --text-info --title='Raccourcis' --width=400 --height=500 --text=\"--- LANCEURS ---\n• Alt + F1 : Rofi\n• Ctrl+Alt+T : Alacritty\n• Win+E : PCManFM\n\n--- FENETRES ---\n• Alt+Tab : Switch\n• Alt+F4 : Fermer\"");

    GtkWidget *btn_man = gtk_button_new_with_label("🔍 Recherche Man");
    g_signal_connect(btn_man, "clicked", G_CALLBACK(go_to_man), app->stack);

    GtkWidget *btn_surf = gtk_button_new_with_label("🌐 Recherche Web (Surfraw)");
    g_signal_connect(btn_surf, "clicked", G_CALLBACK(go_to_surf), app->stack);

    GtkWidget *btn_syslog = gtk_button_new_with_label("🛠️ Rapport Santé Système");
    g_signal_connect(btn_syslog, "clicked", G_CALLBACK(launch_syslog), NULL);

    gtk_box_pack_start(GTK_BOX(vbox_main), btn_shortcuts, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_main), btn_man, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_main), btn_surf, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_main), btn_syslog, TRUE, TRUE, 0);

    // --- PAGE 2 : RECHERCHE MAN ---
    GtkWidget *vbox_man = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *btn_back1 = gtk_button_new_with_label("⬅ Retour");
    g_signal_connect(btn_back1, "clicked", G_CALLBACK(go_to_main), app->stack);
    app->man_entry = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->man_entry), "Quel man cherchez-vous ?");
    app->man_listbox = gtk_list_box_new();
    g_signal_connect(app->man_entry, "search-changed", G_CALLBACK(on_man_search_changed), app);
    
    gtk_box_pack_start(GTK_BOX(vbox_man), btn_back1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_man), app->man_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_man), app->man_listbox, TRUE, TRUE, 0);

    // --- PAGE 3 : SURFRAW ---
    GtkWidget *vbox_surf = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *btn_back2 = gtk_button_new_with_label("⬅ Retour");
    g_signal_connect(btn_back2, "clicked", G_CALLBACK(go_to_main), app->stack);
    app->surf_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->surf_combo), "duckduckgo");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->surf_combo), "google");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->surf_combo), "archwiki");
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->surf_combo), 0);
    app->surf_entry = gtk_entry_new();
    GtkWidget *btn_go = gtk_button_new_with_label("OK (Lancer)");
    g_signal_connect(btn_go, "clicked", G_CALLBACK(on_surfraw_exec), app);

    gtk_box_pack_start(GTK_BOX(vbox_surf), btn_back2, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_surf), app->surf_combo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_surf), app->surf_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_surf), btn_go, FALSE, FALSE, 0);

    // --- ASSEMBLAGE ---
    gtk_stack_add_named(GTK_STACK(app->stack), vbox_main, "main");
    gtk_stack_add_named(GTK_STACK(app->stack), vbox_man, "man");
    gtk_stack_add_named(GTK_STACK(app->stack), vbox_surf, "surf");
    gtk_container_add(GTK_CONTAINER(app->window), app->stack);

    g_signal_connect(app->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_widget_show_all(app->window);
    gtk_main();

    return 0;
}
