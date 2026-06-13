#include <ctype.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <time.h>

// --- CONFIGURATION ---
#define PROGRAMME_NAME "flatpak_manager"

// Structure pour transporter les widgets de la fenêtre de log
typedef struct {
  GtkWidget *window;
  GtkWidget *text_view;
  GtkTextBuffer *buffer;
  FILE *fp;
} LogWindow;

// Prototypes
void on_update_clicked(GtkButton *button, gpointer user_data);
void on_export_clicked(GtkButton *button, gpointer user_data);
void on_import_clicked(GtkButton *button, gpointer user_data);
void on_log_window_destroy(GtkWidget *widget, gpointer data);
void run_command_in_log_window(const char *command, const char *title);

// Fonction d'aide pour valider si une ligne est un AppID Flatpak valide
gboolean is_valid_flatpak_id(const char *str) {
  if (!str || *str == '\0')
    return FALSE;

  int dots = 0;
  for (int i = 0; str[i] != '\0'; i++) {
    char c = str[i];
    if (c == '.') {
      dots++;
    } else if (c != '-' && c != '_' && !isalnum(c)) {
      // Si on trouve un espace, un point-virgule, un guillemet, etc.
      return FALSE;
    }
  }
  // Un AppID valide a généralement au moins deux points (ex: org.gnome.Gedit)
  return (dots >= 1);
}

// --- UTILITAIRES DE LOG ---
void append_log(LogWindow *lw, const char *text) {
  // Vérifications standards à la mode GTK
  g_return_if_fail(lw != NULL);
  g_return_if_fail(lw->buffer != NULL);
  g_return_if_fail(text != NULL);

  GtkTextIter iter;
  gtk_text_buffer_get_end_iter(lw->buffer, &iter);
  gtk_text_buffer_insert(lw->buffer, &iter, text, -1);

  // Auto-scroll vers le bas
  GtkTextMark *mark = gtk_text_buffer_get_insert(lw->buffer);
  gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(lw->text_view), mark, 0.0, TRUE,
                               0.0, 1.0);

  // Force GTK à rafraîchir l'interface pendant l'exécution
  while (gtk_events_pending())
    gtk_main_iteration();
}

LogWindow *create_log_window(const char *title) {
  // Si title est NULL, la fonction s'arrête immédiatement,
  // affiche un warning dans la console et retourne NULL.
  g_return_val_if_fail(title != NULL, NULL);

  LogWindow *lw = g_malloc(sizeof(LogWindow));

  // 1. Initialisation fp à NULL par sécurité
  lw->fp = NULL;

  lw->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(lw->window), title);
  gtk_window_set_default_size(GTK_WINDOW(lw->window), 700, 450);
  gtk_window_set_position(GTK_WINDOW(lw->window), GTK_WIN_POS_CENTER);

  // 2. Connection du signal à la fonction de nettoyage
  g_signal_connect(lw->window, "destroy", G_CALLBACK(on_log_window_destroy),
                   lw);

  lw->text_view = gtk_text_view_new();
  // Récupération immédiate du buffer pour éviter les erreurs de segmentation
  lw->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(lw->text_view));

  gtk_text_view_set_editable(GTK_TEXT_VIEW(lw->text_view), FALSE);
  gtk_text_view_set_monospace(GTK_TEXT_VIEW(lw->text_view), TRUE);
  gtk_text_view_set_left_margin(GTK_TEXT_VIEW(lw->text_view), 10);

  // Style Terminal via CSS
  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_data(provider,
                                  "textview text { background-color: #1e1e1e; "
                                  "color: #00ff00; font-family: monospace; }",
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

  append_log(lw, "🚀 Lancement de l'opération Flatpak...\n");
  append_log(lw,
             "----------------------------------------------------------\n");

  FILE *fp = popen(command, "r");
  if (fp) {
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
      append_log(lw, line);
    }
    int status = pclose(fp);
    if (status == 0)
      append_log(lw, "\n✅ TERMINÉ : Succès.\n");
    else
      append_log(lw, "\n⚠️ TERMINÉ : Le processus a retourné une erreur.\n");
  } else {
    append_log(lw, "❌ ERREUR : Impossible d'exécuter la commande.\n");
  }
}

// --- CALLBACKS BOUTONS ---
// Fonction de rappel quand on ferme la fenêtre de log
void on_log_window_destroy(GtkWidget *widget, gpointer data) {
  (void)widget;

  // 1. On vérification immédiatement si les données utilisateur (data) sont
  // valides
  g_return_if_fail(data != NULL);

  // 2. On peut maintenant caster le pointeur en toute sécurité
  LogWindow *lw = (LogWindow *)data;

  if (lw->fp) {
    // Sous Linux, fermer le tube force le processus fils à recevoir un signal
    // SIGPIPE et à s'arrêter
    pclose(lw->fp);
    lw->fp = NULL;
  }

  // On libère la mémoire de la structure de log allouée par g_malloc dans
  // create_log_window
  g_free(lw);

  // On quitte enfin l'application proprement
  gtk_main_quit();
}

void on_update_clicked(GtkButton *b, gpointer win) {
  (void)b;

  // 1. On vérifie immédiatement si win est valide
  g_return_if_fail(win != NULL);

  // 2. On cache la fenêtre actuelle pour que l'utilisateur
  // voie tout de suite que l'action est prise en compte
  gtk_widget_hide(GTK_WIDGET(win));

  // 3. On lance la commande (la nouvelle fenêtre de log va s'ouvrir)
  run_command_in_log_window("flatpak --user update -y && flatpak --user repair",
                            "Mise à jour Flatpak");

  // 4. On détruit proprement l'ancienne fenêtre une fois que la commande est
  // lancée
  gtk_widget_hide(GTK_WIDGET(win));
}

void on_export_clicked(GtkButton *b, gpointer win) {
  (void)b;

  // 1. On vérifie immédiatement si win est valide
  g_return_if_fail(win != NULL);

  GtkWidget *dialog = gtk_file_chooser_dialog_new(
      "Exporter la liste", GTK_WINDOW(win), GTK_FILE_CHOOSER_ACTION_SAVE,
      "_Annuler", GTK_RESPONSE_CANCEL, "_Enregistrer", GTK_RESPONSE_ACCEPT,
      NULL);

  // Nom de fichier par défaut
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  char name[64];
  strftime(name, sizeof(name), "%Y-%m-%d_flatpak_liste.txt", &tm);
  gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), name);

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
    char *fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

    // SÉCURITÉ : g_shell_quote permet d'échapper correctement les apostrophes
    // et caractères spéciaux contenus dans le nom du fichier.
    char *quoted_fn = g_shell_quote(fn);

    char cmd[4096]; // Augmenté un peu la taille par sécurité
    snprintf(cmd, sizeof(cmd),
             "(flatpak list --app --columns=application --user > %s && "
             "flatpak list --app --columns=application --system >> %s) 2>&1",
             quoted_fn, quoted_fn);

    // On cache l'ancienne fenêtre d'abord
    gtk_widget_hide(GTK_WIDGET(win));

    // Fermeture du dialog de fichier pour ne pas bloquer l'affichage
    gtk_widget_destroy(dialog);
    dialog = NULL;

    // Lancement de la commande
    run_command_in_log_window(cmd, "Exportation Flatpak");

    // Maintenant on peut détruire proprement l'ancienne fenêtre
    gtk_widget_hide(GTK_WIDGET(win));

    // Libération de la mémoire GLib
    g_free(quoted_fn);
    g_free(fn);
  }

  // Si on a annulé, dialog n'est pas encore détruit, on le fait ici
  if (dialog) {
    gtk_widget_destroy(dialog);
  }
}

void on_import_clicked(GtkButton *b, gpointer win) {
  (void)b;
  g_return_if_fail(win != NULL);

  GtkWidget *dialog = gtk_file_chooser_dialog_new(
      "Importer une liste", GTK_WINDOW(win), GTK_FILE_CHOOSER_ACTION_OPEN,
      "_Annuler", GTK_RESPONSE_CANCEL, "_Ouvrir", GTK_RESPONSE_ACCEPT, NULL);

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
    char *fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

    // 1. Ouverture du fichier en mode lecture en C pour analyser le contenu
    FILE *file = fopen(fn, "r");
    if (!file) {
      GtkWidget *msg = gtk_message_dialog_new(
          GTK_WINDOW(dialog), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
          GTK_BUTTONS_OK, "Impossible d'ouvrir le fichier en lecture.");
      gtk_dialog_run(GTK_DIALOG(msg));
      gtk_widget_destroy(msg);
      g_free(fn);
      return;
    }

    // On prépare une chaîne dynamique GLib pour accumuler les commandes
    // d'installation
    GString *cmd_builder = g_string_new("( ");
    char line[256];
    int valid_apps_count = 0;

    while (fgets(line, sizeof(line), file)) {
      // Nettoyage des retours à la ligne (\n ou \r)
      g_strstrip(line);

      // On ignore les lignes vides ou les commentaires (commençant par #)
      if (line[0] == '\0' || line[0] == '#')
        continue;

      // 2. Validation du contenu de la ligne via votre fonction de filtrage
      if (is_valid_flatpak_id(line)) {
        // La ligne est sûre ! On l'ajoute à la suite de commandes
        g_string_append_printf(
            cmd_builder,
            "echo \"Installation de : %s\"; "
            "flatpak install flathub \"%s\" --user --noninteractive -y; ",
            line, line);
        valid_apps_count++;
      }
    }
    fclose(file);
    g_string_append(cmd_builder, ") 2>&1");

    // 3. Si aucune application valide n'a été trouvée
    if (valid_apps_count == 0) {
      GtkWidget *msg = gtk_message_dialog_new(
          GTK_WINDOW(dialog), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
          GTK_BUTTONS_OK,
          "Le fichier ne contient aucune liste d'applications Flatpak valide.");
      gtk_dialog_run(GTK_DIALOG(msg));
      gtk_widget_destroy(msg);

      g_string_free(cmd_builder, TRUE);
      g_free(fn);
      return;
    }

    // Transfert de la commande construite et sécurisée
    char *cmd = g_string_free(cmd_builder, FALSE);

    // --- CORRECTION FLUIDITÉ INTERFACE ---
    // 1. On cache la fenêtre principale pour laisser place aux logs
    gtk_widget_hide(GTK_WIDGET(win));

    // 2. On détruit immédiatement le sélecteur de fichier pour ne pas bloquer
    // l'écran
    gtk_widget_destroy(dialog);
    dialog = NULL;

    // 3. Lancement global sécurisé (ouvre la fenêtre de log)
    run_command_in_log_window(cmd, "Importation Flatpak");

    // Libération de la mémoire
    g_free(cmd);
    g_free(fn);
  }

  // Si l'utilisateur a fait "Annuler", le dialog n'est pas détruit au-dessus,
  // on le fait ici
  if (dialog) {
    gtk_widget_destroy(dialog);
  }
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

  // CORRECTION : Si on ferme manuellement la petite fenêtre principale (la
  // croix), on veut quitter le programme.
  g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_add(GTK_CONTAINER(win), vbox);

  // Boutons avec Mnémoniques (Alt+Lettre) et Icônes
  GtkWidget *btn_upd = gtk_button_new_with_mnemonic("_Mettre à jour");
  gtk_button_set_image(GTK_BUTTON(btn_upd),
                       gtk_image_new_from_icon_name("software-update-available",
                                                    GTK_ICON_SIZE_BUTTON));

  GtkWidget *btn_exp = gtk_button_new_with_mnemonic("_Exporter");
  gtk_button_set_image(
      GTK_BUTTON(btn_exp),
      gtk_image_new_from_icon_name("document-save-as", GTK_ICON_SIZE_BUTTON));

  GtkWidget *btn_imp = gtk_button_new_with_mnemonic("_Importer");
  gtk_button_set_image(
      GTK_BUTTON(btn_imp),
      gtk_image_new_from_icon_name("document-open", GTK_ICON_SIZE_BUTTON));

  GtkWidget *btn_quit = gtk_button_new_with_mnemonic("_Quitter");
  gtk_button_set_image(
      GTK_BUTTON(btn_quit),
      gtk_image_new_from_icon_name("application-exit", GTK_ICON_SIZE_BUTTON));

  // Ajout à la boîte verticale
  gtk_box_pack_start(GTK_BOX(vbox), btn_upd, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), btn_exp, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), btn_imp, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox),
                     gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE,
                     FALSE, 5);
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
