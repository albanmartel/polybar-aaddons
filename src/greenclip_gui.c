#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>

#define PROGRAMME_NAME "greenclip-gui"

// --- STYLE (Correction : suppression de text-align) ---
static void apply_style() {
  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_data(
      provider,
      "window { background-color: #242933; }"
      "button { background-image: none; background-color: #3b4252; color: "
      "#eceff4; border-radius: 6px; border: 1px solid #4c566a; padding: 10px; "
      "margin: 2px; }"
      "button:hover { background-color: #434c5e; border-color: #88c0d0; }"
      "label { font-family: 'Monospace'; font-size: 10pt; }"
      "entry { background-color: #3b4252; color: #eceff4; border-radius: 4px; "
      "padding: 10px; border: 1px solid #4c566a; }",
      -1, NULL);
  gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                            GTK_STYLE_PROVIDER(provider), 800);
  g_object_unref(provider);
}

// --- CALLBACK : CLIC SUR UN ÉLÉMENT (Correction : 100% SANS SHELL, adieu
// l'erreur sh) ---
static void on_item_clicked(GtkButton *btn, gpointer data) {
  if (data == NULL || !GTK_IS_BUTTON(btn))
    return;

  const char *text_to_copy = (const char *)data;

  // Utilisation du presse-papier natif de GTK
  GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_set_text(clipboard, text_to_copy, -1);
  gtk_clipboard_store(clipboard);

  gtk_main_quit();
}

static void free_text(gpointer data, GClosure *closure G_GNUC_UNUSED) {
  if (data != NULL) {
    g_free(data);
  }
}

// --- CHARGEMENT DE L'HISTORIQUE (Correction : Validation UTF-8) ---
static void load_greenclip_history(GtkWidget *listbox) {
  if (listbox == NULL || !GTK_IS_LIST_BOX(listbox))
    return;

  FILE *fp = popen("greenclip print | head -n 20", "r");
  if (!fp)
    return;

  char line[1024];
  while (fgets(line, sizeof(line), fp)) {
    line[strcspn(line, "\n")] = 0;

    if (strlen(line) > 0) {
      // Sécurité Pango : On valide et répare la chaîne pour s'assurer qu'elle
      // est en UTF-8 valide
      gchar *utf8_valid_text = g_utf8_make_valid(line, -1);

      if (utf8_valid_text != NULL) {
        char display_text[60];
        g_snprintf(display_text, sizeof(display_text), "%s", utf8_valid_text);
        if (strlen(utf8_valid_text) > 55) {
          strcat(display_text, "...");
        }

        GtkWidget *btn = gtk_button_new_with_label(display_text);
        gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
        gtk_widget_set_halign(btn, GTK_ALIGN_FILL);

        GtkWidget *label = gtk_bin_get_child(GTK_BIN(btn));
        if (GTK_IS_LABEL(label)) {
          gtk_label_set_xalign(GTK_LABEL(label), 0.0);
          gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
        }

        // On sauvegarde la version UTF-8 propre pour éviter tout crash au clic
        char *full_text = g_strdup(utf8_valid_text);
        g_signal_connect_data(btn, "clicked", G_CALLBACK(on_item_clicked),
                              full_text, (GClosureNotify)free_text, 0);

        gtk_container_add(GTK_CONTAINER(listbox), btn);
        g_free(utf8_valid_text);
      }
    }
  }
  pclose(fp);
}

// --- MAIN ---
int main(int argc, char *argv[]) {
  prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
  gtk_init(&argc, &argv);
  apply_style();

  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "Presse-papier");
  gtk_window_set_default_size(GTK_WINDOW(window), 420, 550);
  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 20);
  gtk_container_add(GTK_CONTAINER(window), vbox);

  GtkWidget *lbl = gtk_label_new(NULL);
  gtk_label_set_markup(
      GTK_LABEL(lbl),
      "<span size='large' weight='bold' foreground='#88c0d0'>📋 Historique "
      "Presse-papier</span>");
  gtk_box_pack_start(GTK_BOX(vbox), lbl, FALSE, FALSE, 5);

  GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
  GtkWidget *listbox = gtk_list_box_new();
  gtk_container_add(GTK_CONTAINER(scrolled), listbox);
  gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);

  load_greenclip_history(listbox);

  gtk_widget_show_all(window);
  gtk_main();

  return 0;
}