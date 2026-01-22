#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/prctl.h>

// --- CONFIGURATION ---
#define PROGRAMME_NAME "flatpak_manager"

// Structure pour transporter les widgets de la fenêtre de log
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

// --- UTILITAIRES DE LOG ---

void append_log(LogWindow *lw, const char *text) {
    if (!lw || !lw->buffer) return;

    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(lw->buffer, &iter);
    gtk_text_buffer_insert(lw->buffer, &iter, text, -1);
    
    // Auto-scroll vers le bas
    GtkTextMark *mark = gtk_text_buffer_get_insert(lw->buffer);
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(lw->text_view), mark, 0.0, TRUE, 0.0, 1.0);
    
    // Force GTK à rafraîchir l'interface pendant l'exécution
    while (gtk_events_pending()) gtk_main_iteration();
}

LogWindow* create_log_window(const char *title) {
    LogWindow *lw = g_malloc(sizeof(LogWindow));
    
    lw->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(lw->window), title);
    gtk_window_set_default_size(GTK_WINDOW(lw->window), 700, 450);
    gtk_window_set_position(GTK_WINDOW(lw->window), GTK_WIN_POS_CENTER);

    lw->text_view = gtk_text_view_new();
    // Récupération immédiate du buffer pour éviter les erreurs de segmentation
    lw->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(lw->text_view));
    
    gtk_text_view_set_editable(GTK_TEXT_VIEW(lw->text_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(lw->text_view), TRUE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(lw->text_view), 10);

    // Style Terminal via CSS
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, 
        "textview text { background-color: #1e1e1e; color: #00ff00; font-family: monospace; }", -1, NULL);
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
    
    append_log(lw, "🚀 Lancement de l'opération Flatpak...\n");
    append_log(lw, "----------------------------------------------------------\n");

    FILE *fp = popen(command, "r");
    if (fp) {
        char line[1024];
        while (fgets(line, sizeof(line), fp)) {
            append_log(lw, line);
        }
        int status = pclose(fp);
        if (status == 0) append_log(lw, "\n✅ TERMINÉ : Succès.\n");
        else append_log(lw, "\n⚠️ TERMINÉ : Le processus a retourné une erreur.\n");
    } else {
        append_log(lw, "❌ ERREUR : Impossible d'exécuter la commande.\n");
    }
}

// --- CALLBACKS BOUTONS ---

void on_update_clicked(GtkButton *b, gpointer win) {
    (void)b;
    gtk_widget_destroy(GTK_WIDGET(win));
    run_command_in_log_window("flatpak --user update -y && flatpak --user repair", "Mise à jour Flatpak");
}

void on_export_clicked(GtkButton *b, gpointer win) {
    (void)b;
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Exporter la liste", GTK_WINDOW(win),
        GTK_FILE_CHOOSER_ACTION_SAVE, "_Annuler", GTK_RESPONSE_CANCEL, "_Enregistrer", GTK_RESPONSE_ACCEPT, NULL);
    
    // Nom de fichier par défaut
    time_t t = time(NULL); struct tm tm = *localtime(&t); char name[64];
    strftime(name, sizeof(name), "%Y-%m-%d_flatpak_liste.txt", &tm);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), name);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        char cmd[2048];
        // Correction de l'avertissement : 2 arguments %s pour 2 variables fn
        snprintf(cmd, sizeof(cmd), "flatpak list --app --columns=application --user > '%s' && flatpak list --app --columns=application --system >> '%s' 2>/dev/null", fn, fn);
        gtk_widget_destroy(GTK_WIDGET(win));
        run_command_in_log_window(cmd, "Exportation Flatpak");
        g_free(fn);
    }
    gtk_widget_destroy(dialog);
}

void on_import_clicked(GtkButton *b, gpointer win) {
    (void)b;
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Importer une liste", GTK_WINDOW(win),
        GTK_FILE_CHOOSER_ACTION_OPEN, "_Annuler", GTK_RESPONSE_CANCEL, "_Ouvrir", GTK_RESPONSE_ACCEPT, NULL);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "while read -r app; do echo \"Installation de : $app\"; flatpak install flathub \"$app\" --user --noninteractive -y; done < '%s'", fn);
        gtk_widget_destroy(GTK_WIDGET(win));
        run_command_in_log_window(cmd, "Importation Flatpak");
        g_free(fn);
    }
    gtk_widget_destroy(dialog);
}

// --- MAIN ---

int main(int argc, char *argv[]) {
    // Nom du processus pour htop/systemd
    prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);

    gtk_init(&argc, &argv);

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Flatpak Manager");
    gtk_container_set_border_width(GTK_CONTAINER(win), 20);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size(GTK_WINDOW(win), 300, -1);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(win), vbox);

    // Boutons avec Mnémoniques (Alt+Lettre) et Icônes
    GtkWidget *btn_upd = gtk_button_new_with_mnemonic("_Mettre à jour");
    gtk_button_set_image(GTK_BUTTON(btn_upd), gtk_image_new_from_icon_name("software-update-available", GTK_ICON_SIZE_BUTTON));
    
    GtkWidget *btn_exp = gtk_button_new_with_mnemonic("_Exporter");
    gtk_button_set_image(GTK_BUTTON(btn_exp), gtk_image_new_from_icon_name("document-save-as", GTK_ICON_SIZE_BUTTON));
    
    GtkWidget *btn_imp = gtk_button_new_with_mnemonic("_Importer");
    gtk_button_set_image(GTK_BUTTON(btn_imp), gtk_image_new_from_icon_name("document-open", GTK_ICON_SIZE_BUTTON));

    GtkWidget *btn_quit = gtk_button_new_with_mnemonic("_Quitter");
    gtk_button_set_image(GTK_BUTTON(btn_quit), gtk_image_new_from_icon_name("application-exit", GTK_ICON_SIZE_BUTTON));

    // Ajout à la boîte verticale
    gtk_box_pack_start(GTK_BOX(vbox), btn_upd, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btn_exp, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btn_imp, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), btn_quit, TRUE, TRUE, 0);

    // Signaux
    g_signal_connect(btn_upd, "clicked", G_CALLBACK(on_update_clicked), win);
    g_signal_connect(btn_exp, "clicked", G_CALLBACK(on_export_clicked), win);
    g_signal_connect(btn_imp, "clicked", G_CALLBACK(on_import_clicked), win);
    g_signal_connect(btn_quit, "clicked", G_CALLBACK(gtk_main_quit), NULL);

    gtk_widget_show_all(win);
    gtk_main();

    return 0;
}
