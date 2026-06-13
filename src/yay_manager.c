#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/prctl.h>

#define PROGRAMME_NAME "yay_manager"

typedef struct {
    GtkWidget *window;
    GtkWidget *text_view;
    GtkTextBuffer *buffer;
} LogWindow;

// Prototypes
void on_update_clicked(GtkButton *button, gpointer user_data);
void on_export_clicked(GtkButton *button, gpointer user_data);
void on_import_clicked(GtkButton *button, gpointer user_data);
void run_command_in_log_window(const char *command, const char *title);

// --- UTILITAIRES ---

void append_log(LogWindow *lw, const char *text) {
    if (!lw || !lw->buffer) return;
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(lw->buffer, &iter);
    gtk_text_buffer_insert(lw->buffer, &iter, text, -1);
    GtkTextMark *mark = gtk_text_buffer_get_insert(lw->buffer);
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(lw->text_view), mark, 0.0, TRUE, 0.0, 1.0);
    while (gtk_events_pending()) gtk_main_iteration();
}

LogWindow* create_log_window(const char *title) {
    LogWindow *lw = g_malloc(sizeof(LogWindow));
    lw->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(lw->window), title);
    gtk_window_set_default_size(GTK_WINDOW(lw->window), 750, 500);
    gtk_window_set_position(GTK_WINDOW(lw->window), GTK_WIN_POS_CENTER);

    lw->text_view = gtk_text_view_new();
    lw->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(lw->text_view));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(lw->text_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(lw->text_view), TRUE);

    // Style Terminal YAY (Jaune/Or sur Noir)
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, 
        "textview text { background-color: #0f0f0f; color: #fbc02d; font-family: monospace; }", -1, NULL);
    gtk_style_context_add_provider(gtk_widget_get_style_context(lw->text_view), 
        GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);

    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scrolled), lw->text_view);
    gtk_container_add(GTK_CONTAINER(lw->window), scrolled);
    gtk_widget_show_all(lw->window);
    return lw;
}

void run_command_in_log_window(const char *command, const char *title) {
    LogWindow *lw = create_log_window(title);
    append_log(lw, "🛠️ Appel de Yay (AUR Helper)...\n");
    append_log(lw, "----------------------------------------------------------\n");
    
    FILE *fp = popen(command, "r");
    if (fp) {
        char line[1024];
        while (fgets(line, sizeof(line), fp)) append_log(lw, line);
        int status = pclose(fp);
        append_log(lw, status == 0 ? "\n✅ Yay a terminé l'opération." : "\n⚠️ Opération annulée ou erreur détectée.");
    }
}

// --- ACTIONS ---

void on_update_clicked(GtkButton *b, gpointer win) {
    (void)b;
    gtk_widget_destroy(GTK_WIDGET(win));
    // Yay gérera la demande de mot de passe graphique via sudo_askpass ou polkit automatiquement
    run_command_in_log_window("yay -Syu --noconfirm", "Mise à jour AUR & Système (Yay)");
}

void on_export_clicked(GtkButton *b, gpointer win) {
    (void)b;
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Exporter liste AUR (Yay)", GTK_WINDOW(win), 
        GTK_FILE_CHOOSER_ACTION_SAVE, "_Annuler", GTK_RESPONSE_CANCEL, "_Enregistrer", GTK_RESPONSE_ACCEPT, NULL);
    
    time_t t = time(NULL); struct tm tm = *localtime(&t); char name[64];
    strftime(name, sizeof(name), "%Y-%m-%d_yay_aur_liste.txt", &tm);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), name);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        char cmd[1024];
        // On liste uniquement les paquets venant de l'AUR (-Qm)
        snprintf(cmd, sizeof(cmd), "yay -Qm --quiet > '%s'", fn);
        system(cmd);
        g_free(fn);
    }
    gtk_widget_destroy(dialog);
}

void on_import_clicked(GtkButton *b, gpointer win) {
    (void)b;
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Importer liste AUR", GTK_WINDOW(win), 
        GTK_FILE_CHOOSER_ACTION_OPEN, "_Annuler", GTK_RESPONSE_CANCEL, "_Ouvrir", GTK_RESPONSE_ACCEPT, NULL);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        char cmd[2048];
        // --needed évite de réinstaller ce qui est déjà là, --noconfirm pour l'automatisation
        snprintf(cmd, sizeof(cmd), "yay -S --needed --noconfirm $(cat '%s')", fn);
        gtk_widget_destroy(GTK_WIDGET(win));
        run_command_in_log_window(cmd, "Importation AUR (Yay)");
        g_free(fn);
    }
    gtk_widget_destroy(dialog);
}

// --- MAIN ---

int main(int argc, char *argv[]) {
    prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
    gtk_init(&argc, &argv);

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Yay (AUR) Manager");
    gtk_container_set_border_width(GTK_CONTAINER(win), 20);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(win), vbox);

    GtkWidget *btn_upd = gtk_button_new_with_mnemonic("_Mise à jour AUR");
    gtk_button_set_image(GTK_BUTTON(btn_upd), gtk_image_new_from_icon_name("software-update-available-symbolic", GTK_ICON_SIZE_BUTTON));
    
    GtkWidget *btn_exp = gtk_button_new_with_mnemonic("_Exporter AUR");
    gtk_button_set_image(GTK_BUTTON(btn_exp), gtk_image_new_from_icon_name("document-save-symbolic", GTK_ICON_SIZE_BUTTON));
    
    GtkWidget *btn_imp = gtk_button_new_with_mnemonic("_Importer AUR");
    gtk_button_set_image(GTK_BUTTON(btn_imp), gtk_image_new_from_icon_name("folder-download-symbolic", GTK_ICON_SIZE_BUTTON));

    GtkWidget *btn_quit = gtk_button_new_with_mnemonic("_Quitter");
    gtk_button_set_image(GTK_BUTTON(btn_quit), gtk_image_new_from_icon_name("application-exit-symbolic", GTK_ICON_SIZE_BUTTON));

    gtk_box_pack_start(GTK_BOX(vbox), btn_upd, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btn_exp, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btn_imp, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), btn_quit, TRUE, TRUE, 0);

    g_signal_connect(btn_upd, "clicked", G_CALLBACK(on_update_clicked), win);
    g_signal_connect(btn_exp, "clicked", G_CALLBACK(on_export_clicked), win);
    g_signal_connect(btn_imp, "clicked", G_CALLBACK(on_import_clicked), win);
    g_signal_connect(btn_quit, "clicked", G_CALLBACK(gtk_main_quit), NULL);

    gtk_widget_show_all(win);
    gtk_main();
    return 0;
}
