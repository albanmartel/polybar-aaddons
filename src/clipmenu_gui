#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>

#define PROGRAMME_NAME "clipmenu-gui"

// Structure pour passer plusieurs données aux boutons d'action d'une ligne
typedef struct {
  char *full_text;       // Le texte original de l'entrée
  GtkWidget *row_widget; // Le widget parent (la ligne dans la ListBox)
  GtkWidget *listbox;    // La ListBox globale
} ClipboardRowData;

// --- STYLE CSS REVISITÉ ET SÉCURISÉ ---
static void apply_style(void) {
  GtkCssProvider *provider = gtk_css_provider_new();
  if (!provider)
    return;

  gtk_css_provider_load_from_data(
      provider,
      "window { background-color: #242933; }"
      "button { background-image: none; background-color: #3b4252; color: "
      "#eceff4; "
      "         border-radius: 6px; border: 1px solid #4c566a; padding: 6px "
      "10px; margin: 2px; }"
      "button:hover { background-color: #434c5e; border-color: #88c0d0; }"
      ".btn-danger { background-color: #bf616a; color: #eceff4; border-color: "
      "#bf616a; }"
      ".btn-danger:hover { background-color: #d08770; border-color: #d08770; }"
      ".btn-action { padding: 4px 8px; font-size: 9pt; }"
      "listrow { background-color: #2e3440; border-bottom: 1px solid #3b4252; "
      "padding: 4px; }"
      "listrow:selected { background-color: #434c5e; }"
      "label, listrow label { font-family: 'Monospace'; font-size: 10pt; "
      "color: #d8dee9 !important; }"
      "listrow:selected label { color: #eceff4 !important; }"
      "entry { background-color: #3b4252; color: #eceff4; border-radius: 4px; "
      "        padding: 10px; border: 1px solid #4c566a; }",
      -1, NULL);

  GdkScreen *screen = gdk_screen_get_default();
  if (screen) {
    gtk_style_context_add_provider_for_screen(
        screen, GTK_STYLE_PROVIDER(provider), 800);
  }
  g_object_unref(provider);
}

// Fonction de libération de mémoire pour la structure de ligne
static void free_row_data(gpointer data) {
  ClipboardRowData *row_data = (ClipboardRowData *)data;
  if (row_data != NULL) {
    if (row_data->full_text) {
      g_free(row_data->full_text);
      row_data->full_text = NULL;
    }
    g_free(row_data);
  }
}

// --- ACTION 1 : COPIER LA LIGNE ---
static void on_copy_clicked(GtkButton *btn, gpointer data) {
  if (!GTK_IS_BUTTON(btn) || !data)
    return;

  const ClipboardRowData *row_data = (const ClipboardRowData *)data;
  if (!row_data->full_text)
    return;

  GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  if (clipboard) {
    gtk_clipboard_set_text(clipboard, row_data->full_text, -1);
    gtk_clipboard_store(clipboard);
  }

  gtk_main_quit();
}

// --- ACTION 2 : SUPPRIMER UNE LIGNE ---
static void on_delete_clicked(GtkButton *btn, gpointer data) {
  if (!GTK_IS_BUTTON(btn) || !data)
    return;

  const ClipboardRowData *row_data = (const ClipboardRowData *)data;
  if (row_data->row_widget && GTK_IS_WIDGET(row_data->row_widget)) {
    gtk_widget_destroy(row_data->row_widget);
  }
}

// --- ACTION 3 : MODIFIER UNE LIGNE ---
static void on_edit_clicked(GtkButton *btn, gpointer data) {
  if (!GTK_IS_BUTTON(btn) || !data)
    return;

  ClipboardRowData *row_data = (ClipboardRowData *)data;
  if (!row_data->row_widget || !GTK_IS_WIDGET(row_data->row_widget) ||
      !row_data->full_text)
    return;

  GtkWidget *window = gtk_widget_get_toplevel(row_data->row_widget);
  if (!GTK_IS_WINDOW(window))
    return;

  GtkWidget *dialog = gtk_dialog_new_with_buttons(
      "Modifier l'entrée", GTK_WINDOW(window),
      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, "Enregistrer",
      GTK_RESPONSE_ACCEPT, "Annuler", GTK_RESPONSE_REJECT, NULL);
  if (!dialog)
    return;

  GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *entry = gtk_entry_new();
  if (!entry) {
    gtk_widget_destroy(dialog);
    return;
  }

  gtk_entry_set_text(GTK_ENTRY(entry), row_data->full_text);
  gtk_container_add(GTK_CONTAINER(content_area), entry);
  gtk_widget_show_all(dialog);

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
    const char *new_text = gtk_entry_get_text(GTK_ENTRY(entry));
    if (new_text && strlen(new_text) > 0) {
      g_free(row_data->full_text);
      row_data->full_text = g_strdup(new_text);

      GList *children =
          gtk_container_get_children(GTK_CONTAINER(row_data->row_widget));
      if (children) {
        if (GTK_IS_LABEL(children->data)) {
          char display_text[60];
          g_snprintf(display_text, sizeof(display_text), "%s", new_text);
          if (strlen(new_text) > 55) {
            display_text[55] = '\0';
            g_strlcat(display_text, "...",
                      sizeof(display_text)); // Remplacement sécurisé de strcat
          }
          gtk_label_set_text(GTK_LABEL(children->data), display_text);
        }
        g_list_free(children);
      }

      GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
      if (clipboard && row_data->full_text) {
        gtk_clipboard_set_text(clipboard, row_data->full_text, -1);
        gtk_clipboard_store(clipboard);
      }
    }
  }
  gtk_widget_destroy(dialog);
}

// --- ACTION 4 : SUPPRIMER TOUT L'HISTORIQUE ---
static void on_clear_all_clicked(GtkButton *btn, gpointer data) {
  if (!GTK_IS_BUTTON(btn) || !data || !GTK_IS_WIDGET(data))
    return;

  GtkWidget *listbox = GTK_WIDGET(data);
  if (!GTK_IS_LIST_BOX(listbox))
    return;

  GList *children = gtk_container_get_children(GTK_CONTAINER(listbox));
  if (children) {
    for (GList *iter = children; iter != NULL; iter = g_list_next(iter)) {
      if (iter->data && GTK_IS_WIDGET(iter->data)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
      }
    }
    g_list_free(children);
  }

  // Utilisation sécurisée : l'appel système ne dépend d'aucune variable
  // utilisateur non vérifiée
  int ret = system("clipdel -d '.*' 2>/dev/null || rm -f /run/user/$(id "
                   "-u)/clipmenu.*/line_cache 2>/dev/null");
  (void)ret;
}

// --- CHARGEMENT DE L'HISTORIQUE SÉCURISÉ ---
static void load_clipmenu_history(GtkWidget *listbox) {
  if (listbox == NULL || !GTK_IS_LIST_BOX(listbox))
    return;

  FILE *fp = popen("sh -c '"
                   "CACHE_DIR=$(ls -d /run/user/$(id -u)/clipmenu.*.$(whoami) "
                   "2>/dev/null | head -n 1); "
                   "if [ -d \"$CACHE_DIR\" ]; then "
                   "  ls -t \"$CACHE_DIR\" | grep -v 'line_cache' | grep -v "
                   "'lock' | head -n 20 | while read -r file; do "
                   "    cat \"$CACHE_DIR/$file\" 2>/dev/null | head -n 1; "
                   "  done; "
                   "fi'",
                   "r");

  if (!fp)
    return;

  char line[1024];
  while (fgets(line, sizeof(line), fp)) {
    line[strcspn(line, "\n")] = 0;

    size_t len = strlen(line);
    if (len == 0)
      continue;

    gchar *utf8_valid_text = g_utf8_make_valid(line, -1);
    if (utf8_valid_text != NULL) {

      GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
      if (!row_box) {
        g_free(utf8_valid_text);
        continue;
      }

      char display_text[45];
      g_snprintf(display_text, sizeof(display_text), "%s", utf8_valid_text);
      if (strlen(utf8_valid_text) > 40) {
        display_text[40] = '\0';
        display_text[41] = '.';
        display_text[42] = '.';
        display_text[43] = '.';
        display_text[44] = '\0';
      }

      GtkWidget *lbl_text = gtk_label_new(display_text);
      if (!lbl_text) {
        gtk_widget_destroy(row_box);
        g_free(utf8_valid_text);
        continue;
      }
      gtk_label_set_xalign(GTK_LABEL(lbl_text), 0.0);
      gtk_label_set_ellipsize(GTK_LABEL(lbl_text), PANGO_ELLIPSIZE_END);
      gtk_box_pack_start(GTK_BOX(row_box), lbl_text, TRUE, TRUE, 5);

      // Allocation sécurisée de notre structure
      ClipboardRowData *row_data = g_new0(ClipboardRowData, 1);
      if (!row_data) {
        gtk_widget_destroy(row_box);
        g_free(utf8_valid_text);
        continue;
      }
      row_data->full_text = g_strdup(utf8_valid_text);
      row_data->row_widget = row_box;
      row_data->listbox = listbox;

      // Bouton 1 : COPIER 📋
      GtkWidget *btn_copy = gtk_button_new_with_label("📋");
      if (btn_copy) {
        gtk_widget_set_tooltip_text(btn_copy, "Copier dans le presse-papiers");
        gtk_style_context_add_class(gtk_widget_get_style_context(btn_copy),
                                    "btn-action");
        g_signal_connect_data(btn_copy, "clicked", G_CALLBACK(on_copy_clicked),
                              row_data, (GClosureNotify)NULL, 0);
        gtk_box_pack_start(GTK_BOX(row_box), btn_copy, FALSE, FALSE, 0);
      }

      // Bouton 2 : MODIFIER ✏️
      GtkWidget *btn_edit = gtk_button_new_with_label("✏️");
      if (btn_edit) {
        gtk_widget_set_tooltip_text(btn_edit, "Modifier le texte");
        gtk_style_context_add_class(gtk_widget_get_style_context(btn_edit),
                                    "btn-action");
        g_signal_connect_data(btn_edit, "clicked", G_CALLBACK(on_edit_clicked),
                              row_data, (GClosureNotify)NULL, 0);
        gtk_box_pack_start(GTK_BOX(row_box), btn_edit, FALSE, FALSE, 0);
      }

      // Bouton 3 : SUPPRIMER LA LIGNE 🗑️
      GtkWidget *btn_del = gtk_button_new_with_label("🗑️");
      if (btn_del) {
        gtk_widget_set_tooltip_text(btn_del, "Supprimer de la liste");
        gtk_style_context_add_class(gtk_widget_get_style_context(btn_del),
                                    "btn-action");
        gtk_style_context_add_class(gtk_widget_get_style_context(btn_del),
                                    "btn-danger");
        g_signal_connect_data(btn_del, "clicked", G_CALLBACK(on_delete_clicked),
                              row_data, (GClosureNotify)NULL, 0);
        gtk_box_pack_start(GTK_BOX(row_box), btn_del, FALSE, FALSE, 0);
      }

      GtkWidget *listbox_row = gtk_list_box_row_new();
      if (listbox_row) {
        gtk_container_add(GTK_CONTAINER(listbox_row), row_box);
        gtk_container_add(GTK_CONTAINER(listbox), listbox_row);
        g_object_set_data_full(G_OBJECT(listbox_row), "row-data", row_data,
                               free_row_data);
      } else {
        free_row_data(row_data);
      }

      g_free(utf8_valid_text);
    }
  }
  pclose(fp);
}

// --- FONCTION DISTINCTE : CRÉATION DE L'INTERFACE ---
int creer_interface_presse_papiers(int argc, char *argv[]) {
  // On change le nom du processus système
  prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
  
  // Initialisation de GTK
  gtk_init(&argc, &argv);
  apply_style();

  // Fenêtre principale
  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  if (!window) return 1;
  gtk_window_set_title(GTK_WINDOW(window), "Gestionnaire de Presse-papiers");
  gtk_window_set_default_size(GTK_WINDOW(window), 480, 580);
  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

  // Boîte verticale principale
  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
  if (!vbox) return 1;
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 15);
  gtk_container_add(GTK_CONTAINER(window), vbox);

  // Titre de l'application
  GtkWidget *lbl = gtk_label_new(NULL);
  if (lbl) {
    gtk_label_set_markup(GTK_LABEL(lbl),
                         "<span size='large' weight='bold' "
                         "foreground='#88c0d0'>📋 Presse-papier Avancé</span>");
    gtk_box_pack_start(GTK_BOX(vbox), lbl, FALSE, FALSE, 5);
  }

  // Zone de défilement et liste
  GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
  GtkWidget *listbox = gtk_list_box_new();
  if (scrolled && listbox) {
    gtk_container_add(GTK_CONTAINER(scrolled), listbox);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);
  }

  // Bouton Tout Effacer (placé en bas)
  GtkWidget *btn_clear_all = gtk_button_new_with_label("🚨 Tout effacer l'historique");
  if (btn_clear_all && listbox) {
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_clear_all), "btn-danger");
    g_signal_connect(btn_clear_all, "clicked", G_CALLBACK(on_clear_all_clicked), listbox);
    gtk_box_pack_end(GTK_BOX(vbox), btn_clear_all, FALSE, FALSE, 5);
  }

  // Chargement de l'historique si la liste existe
  if (listbox) {
    load_clipmenu_history(listbox);
  }

  // Affichage et lancement de la boucle principale GTK
  gtk_widget_show_all(window);
  gtk_main();

  return 0; // Tout s'est bien passé
}

// --- MAIN ---
int main(int argc, char *argv[]) {
    // On lance l'interface et on récupère son code de retour (0 ou 1)
    int status = creer_interface_presse_papiers(argc, argv);
    
    return status;
}