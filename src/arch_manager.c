#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/prctl.h>

#define PROGRAMME_NAME "arch_manager"

typedef struct {
    GtkWidget *window;
    GtkWidget *text_view;
    GtkTextBuffer *buffer;
} LogWindow;

// --- LOGIQUE COMMUNE D'AFFICHAGE ---

void append_log(LogWindow *lw, const char *text) {
    if (!lw || !lw->buffer) return;
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(lw->buffer, &iter);
    gtk_text_buffer_insert(lw->buffer, &iter, text, -1);
    GtkTextMark *mark = gtk_text_buffer_get_insert(lw->buffer);
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(lw->text_view), mark, 0.0, TRUE, 0.0, 1.0);
    while (gtk_events_pending()) gtk_main_iteration();
}

LogWindow* create_log_window(const char *title, const char *css_color) {
    LogWindow *lw = g_malloc(sizeof(LogWindow));
    lw->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(lw->window), title);
    gtk_window_set_default_size(GTK_WINDOW(lw->window), 800, 500);
    gtk_window_set_position(GTK_WINDOW(lw->window), GTK_WIN_POS_CENTER);

    lw->text_view = gtk_text_view_new();
    lw->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(lw->text_view));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(lw->text_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(lw->text_view), TRUE);

    GtkCssProvider *provider = gtk_css_provider_new();
    char css[128];
    snprintf(css, sizeof(css), "textview text { background-color: #0f0f0f; color: %s; padding: 10px; }", css_color);
    gtk_css_provider_load_from_data(provider, css, -1, NULL);
    gtk_style_context_add_provider(gtk_widget_get_style_context(lw->text_view), 
        GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);

    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scrolled), lw->text_view);
    gtk_container_add(GTK_CONTAINER(lw->window), scrolled);
    gtk_widget_show_all(lw->window);
    return lw;
}

void run_task(const char *cmd, const char *title, const char *color) {
    LogWindow *lw = create_log_window(title, color);
    append_log(lw, "🚀 Initialisation de la tâche...\n\n");
    
    FILE *fp = popen(cmd, "r");
    if (fp) {
        char line[1024];
        while (fgets(line, sizeof(line), fp)) append_log(lw, line);
        int status = pclose(fp);
        append_log(lw, status == 0 ? "\n✅ Opération terminée avec succès." : "\n⚠️ Le processus a rencontré une erreur.");
    }
}

char* get_file_path(GtkWidget *parent, GtkFileChooserAction action, const char *default_name) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        (action == GTK_FILE_CHOOSER_ACTION_SAVE) ? "Enregistrer sous..." : "Ouvrir un fichier",
        GTK_WINDOW(parent), action, "_Annuler", GTK_RESPONSE_CANCEL,
        (action == GTK_FILE_CHOOSER_ACTION_SAVE) ? "_Enregistrer" : "_Ouvrir", GTK_RESPONSE_ACCEPT, NULL);

    if (default_name) gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), default_name);

    char *filename = NULL;
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    }
    gtk_widget_destroy(dialog);
    while (gtk_events_pending()) gtk_main_iteration();
    return filename;
}

// --- CALLBACKS PAR SYSTÈME (AVERTISSEMENTS CORRIGÉS) ---

void on_pacman_upd(GtkWidget *w, gpointer win) {
    (void)w;
    gtk_widget_destroy(GTK_WIDGET(win));
    run_task("PASS=$(zenity --password --title='Root Access'); echo $PASS | sudo -S pacman -Syu --noconfirm", "Pacman System Update", "#00d9ff");
}
void on_pacman_exp(GtkWidget *w, gpointer win) {
    (void)w;
    char *fn = get_file_path(win, GTK_FILE_CHOOSER_ACTION_SAVE, "pacman_list.txt");
    if (fn) {
        char cmd[1024]; snprintf(cmd, sizeof(cmd), "pacman -Qe --quiet > '%s'", fn);
        system(cmd); g_free(fn);
        run_task("echo 'Exportation Pacman terminée.'", "Export Pacman", "#00d9ff");
    }
}
void on_pacman_imp(GtkWidget *w, gpointer win) {
    (void)w;
    char *fn = get_file_path(win, GTK_FILE_CHOOSER_ACTION_OPEN, NULL);
    if (fn) {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "PASS=$(zenity --password --title='Root Access'); echo $PASS | sudo -S pacman -S --needed --noconfirm $(cat '%s')", fn);
        gtk_widget_destroy(GTK_WIDGET(win));
        run_task(cmd, "Importation Pacman", "#00d9ff"); g_free(fn);
    }
}

void on_yay_upd(GtkWidget *w, gpointer win) {
    (void)w;
    gtk_widget_destroy(GTK_WIDGET(win));
    run_task("yay -Syu --noconfirm", "Yay AUR Update", "#fbc02d");
}
void on_yay_exp(GtkWidget *w, gpointer win) {
    (void)w;
    char *fn = get_file_path(win, GTK_FILE_CHOOSER_ACTION_SAVE, "aur_list.txt");
    if (fn) {
        char cmd[1024]; snprintf(cmd, sizeof(cmd), "yay -Qm --quiet > '%s'", fn);
        system(cmd); g_free(fn);
        run_task("echo 'Exportation AUR terminée.'", "Export Yay", "#fbc02d");
    }
}
void on_yay_imp(GtkWidget *w, gpointer win) {
    (void)w;
    char *fn = get_file_path(win, GTK_FILE_CHOOSER_ACTION_OPEN, NULL);
    if (fn) {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "yay -S --needed --noconfirm $(cat '%s')", fn);
        gtk_widget_destroy(GTK_WIDGET(win));
        run_task(cmd, "Importation Yay", "#fbc02d"); g_free(fn);
    }
}

void on_flat_upd(GtkWidget *w, gpointer win) {
    (void)w;
    gtk_widget_destroy(GTK_WIDGET(win));
    run_task("flatpak --user update -y", "Flatpak Update", "#00ff00");
}
void on_flat_exp(GtkWidget *w, gpointer win) {
    (void)w;
    char *fn = get_file_path(win, GTK_FILE_CHOOSER_ACTION_SAVE, "flatpak_list.txt");
    if (fn) {
        char cmd[1024]; snprintf(cmd, sizeof(cmd), "flatpak list --app --columns=application --user > '%s'", fn);
        system(cmd); g_free(fn);
        run_task("echo 'Exportation Flatpak terminée.'", "Export Flatpak", "#00ff00");
    }
}
void on_flat_imp(GtkWidget *w, gpointer win) {
    (void)w;
    char *fn = get_file_path(win, GTK_FILE_CHOOSER_ACTION_OPEN, NULL);
    if (fn) {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "while read -r app; do flatpak install flathub \"$app\" --user -y; done < '%s'", fn);
        gtk_widget_destroy(GTK_WIDGET(win));
        run_task(cmd, "Importation Flatpak", "#00ff00"); g_free(fn);
    }
}

// --- CONSTRUCTION DE L'INTERFACE ---

GtkWidget* create_manager_tab(GtkWidget *win, const char *tab_icon, 
                             GCallback up_cb, GCallback ex_cb, GCallback im_cb) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 20);

    GtkWidget *btn_up = gtk_button_new_with_label("Mettre à jour tout");
    gtk_button_set_image(GTK_BUTTON(btn_up), gtk_image_new_from_icon_name(tab_icon, GTK_ICON_SIZE_BUTTON));
    
    GtkWidget *btn_ex = gtk_button_new_with_label("Exporter la liste");
    gtk_button_set_image(GTK_BUTTON(btn_ex), gtk_image_new_from_icon_name("document-save-as", GTK_ICON_SIZE_BUTTON));

    GtkWidget *btn_im = gtk_button_new_with_label("Importer une liste");
    gtk_button_set_image(GTK_BUTTON(btn_im), gtk_image_new_from_icon_name("document-open", GTK_ICON_SIZE_BUTTON));

    gtk_box_pack_start(GTK_BOX(vbox), btn_up, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btn_ex, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btn_im, TRUE, TRUE, 0);

    g_signal_connect(btn_up, "clicked", up_cb, win);
    g_signal_connect(btn_ex, "clicked", ex_cb, win);
    g_signal_connect(btn_im, "clicked", im_cb, win);

    return vbox;
}

int main(int argc, char *argv[]) {
    prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
    gtk_init(&argc, &argv);

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Arch Multi-Manager");
    gtk_window_set_default_size(GTK_WINDOW(win), 400, 350);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *notebook = gtk_notebook_new();
    gtk_container_add(GTK_CONTAINER(win), notebook);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), 
        create_manager_tab(win, "system-software-update", G_CALLBACK(on_pacman_upd), G_CALLBACK(on_pacman_exp), G_CALLBACK(on_pacman_imp)), 
        gtk_label_new("Pacman"));

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), 
        create_manager_tab(win, "software-update-available", G_CALLBACK(on_yay_upd), G_CALLBACK(on_yay_exp), G_CALLBACK(on_yay_imp)), 
        gtk_label_new("Yay (AUR)"));

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), 
        create_manager_tab(win, "package-x-generic", G_CALLBACK(on_flat_upd), G_CALLBACK(on_flat_exp), G_CALLBACK(on_flat_imp)), 
        gtk_label_new("Flatpak"));

    gtk_widget_show_all(win);
    gtk_main();
    return 0;
}
