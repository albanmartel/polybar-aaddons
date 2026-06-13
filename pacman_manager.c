#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <time.h>

#define PROGRAMME_NAME "pacman_manager"

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
  if (!lw || !lw->buffer)
    return;
  GtkTextIter iter;
  gtk_text_buffer_get_end_iter(lw->buffer, &iter);
  gtk_text_buffer_insert(lw->buffer, &iter, text, -1);
  GtkTextMark *mark = gtk_text_buffer_get_insert(lw->buffer);
  gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(lw->text_view), mark, 0.0, TRUE,
                               0.0, 1.0);
  while (gtk_events_pending())
    gtk_main_iteration();
}

LogWindow *create_log_window(const char *title) {
  // Si title est NULL, la fonction s'arrête immédiatement,
  // affiche un warning dans la console et retourne NULL.
  g_return_val_if_fail(title != NULL, NULL);

  LogWindow *lw = g_malloc(sizeof(LogWindow));

  // 1. Initialisation fp à NULL par sécurité
  // lw->fp = NULL;

  lw->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(lw->window), title);
  gtk_window_set_default_size(GTK_WINDOW(lw->window), 700, 450);
  gtk_window_set_position(GTK_WINDOW(lw->window), GTK_WIN_POS_CENTER);

  lw->text_view = gtk_text_view_new();
  lw->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(lw->text_view));
  gtk_text_view_set_editable(GTK_TEXT_VIEW(lw->text_view), FALSE);
  gtk_text_view_set_monospace(GTK_TEXT_VIEW(lw->text_view), TRUE);

  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_data(provider,
                                  "textview text { background-color: #1a1a1a; "
                                  "color: #00d9ff; font-family: monospace; }",
                                  -1, NULL);
  gtk_style_context_add_provider(gtk_widget_get_style_context(lw->text_view),
                                 GTK_STYLE_PROVIDER(provider),
                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(provider);

  GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
  gtk_container_add(GTK_CONTAINER(scrolled), lw->text_view);
  gtk_container_add(GTK_CONTAINER(lw->window), scrolled);
  gtk_widget_show_all(lw->window);
  return lw;
}

void run_command_in_log_window(const char *command, const char *title) {
  // 1. On vérifie immédiatement si la commande et le title sont valides
  g_return_if_fail(command != NULL);
  g_return_if_fail(title != NULL);

  LogWindow *lw = create_log_window(title);
  if (!lw) {
    g_warning("Impossible de créer la fenêtre de log pour la commande : %s",
              command);
    return;
  }

  append_log(lw, "🔐 Vérification des droits et exécution...\n");

  FILE *fp = popen(command, "r");
  if (fp) {
    char line[1024];
    while (fgets(line, sizeof(line), fp))
      append_log(lw, line);
    int status = pclose(fp);
    append_log(lw, status == 0 ? "\n✅ Opération terminée."
                               : "\n⚠️ Échec ou annulation.");
  }
}

// --- ACTIONS ---

void on_update_clicked(GtkButton *b, gpointer win) {
  (void)b;
  gtk_widget_destroy(GTK_WIDGET(win));
  // pkexec demande le mot de passe proprement en GUI sur Arch
  run_command_in_log_window("pkexec pacman -Syu --noconfirm",
                            "Mise à jour Système (Pacman)");
}

void on_export_clicked(GtkButton *b, gpointer win) {
  (void)b;
  // 1. On vérification immédiatement si les données utilisateur (data) sont
  // valides
  g_return_if_fail(win != NULL);

  GtkWidget *dialog = gtk_file_chooser_dialog_new(
      "Exporter liste Pacman", GTK_WINDOW(win), GTK_FILE_CHOOSER_ACTION_SAVE,
      "_Annuler", GTK_RESPONSE_CANCEL, "_Enregistrer", GTK_RESPONSE_ACCEPT,
      NULL);

  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  char name[64];
  strftime(name, sizeof(name), "%Y-%m-%d_pacman_liste.txt", &tm);
  gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), name);

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
    char *fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

    char cmd[1024];
    // Commande pour lister uniquement les paquets explicitement installés
    snprintf(cmd, sizeof(cmd), "pacman -Qe --quiet > '%s'", fn);
    system(cmd); // Export simple
    g_print("Exporté dans %s\n", fn);
    g_free(fn);
  }
  gtk_widget_destroy(dialog);
}

void on_import_clicked(GtkButton *b, gpointer win) {
  (void)b;
  g_return_if_fail(win != NULL);

  GtkWidget *dialog = gtk_file_chooser_dialog_new(
      "Importer liste Pacman", GTK_WINDOW(win), GTK_FILE_CHOOSER_ACTION_OPEN,
      "_Annuler", GTK_RESPONSE_CANCEL, "_Ouvrir", GTK_RESPONSE_ACCEPT, NULL);

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
    char *fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    char cmd[2048];
    // On lit le fichier et on passe tout à pacman via pkexec
    snprintf(cmd, sizeof(cmd),
             "pkexec pacman -S --needed --noconfirm $(cat '%s')", fn);
    gtk_widget_destroy(GTK_WIDGET(win));
    run_command_in_log_window(cmd, "Importation / Installation Pacman");
    g_free(fn);
  }
  gtk_widget_destroy(dialog);
}

// --- MAIN ---

int main(int argc, char *argv[]) {
  prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
  gtk_init(&argc, &argv);

  GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(win), "Pacman Manager");
  gtk_container_set_border_width(GTK_CONTAINER(win), 20);
  gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
  g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_add(GTK_CONTAINER(win), vbox);

  GtkWidget *btn_upd = gtk_button_new_with_mnemonic("_Mise à jour Système");
  gtk_button_set_image(GTK_BUTTON(btn_upd),
                       gtk_image_new_from_icon_name("system-software-update",
                                                    GTK_ICON_SIZE_BUTTON));

  GtkWidget *btn_exp = gtk_button_new_with_mnemonic("_Exporter Paquets");
  gtk_button_set_image(
      GTK_BUTTON(btn_exp),
      gtk_image_new_from_icon_name("document-export", GTK_ICON_SIZE_BUTTON));

  GtkWidget *btn_imp = gtk_button_new_with_mnemonic("_Importer & Installer");
  gtk_button_set_image(
      GTK_BUTTON(btn_imp),
      gtk_image_new_from_icon_name("document-import", GTK_ICON_SIZE_BUTTON));

  GtkWidget *btn_quit = gtk_button_new_with_mnemonic("_Quitter");
  gtk_button_set_image(
      GTK_BUTTON(btn_quit),
      gtk_image_new_from_icon_name("application-exit", GTK_ICON_SIZE_BUTTON));

  gtk_box_pack_start(GTK_BOX(vbox), btn_upd, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), btn_exp, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), btn_imp, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox),
                     gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE,
                     FALSE, 5);
  gtk_box_pack_start(GTK_BOX(vbox), btn_quit, TRUE, TRUE, 0);

  g_signal_connect(btn_upd, "clicked", G_CALLBACK(on_update_clicked), win);
  g_signal_connect(btn_exp, "clicked", G_CALLBACK(on_export_clicked), win);
  g_signal_connect(btn_imp, "clicked", G_CALLBACK(on_import_clicked), win);
  g_signal_connect(btn_quit, "clicked", G_CALLBACK(gtk_main_quit), NULL);

  gtk_widget_show_all(win);
  gtk_main();
  return 0;
}
