#include "glib.h"
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <sys/prctl.h>

// --- NOM PROGRAMME ---
#define PROGRAMME_NAME "gdesktop"
// --- DEFINITION CHEMIN CONFIGURATION SAUVEGARDEE ---
#define CONFIG_FILE ".config/gdesktop/gdesktop_pos.conf"

typedef struct {
  char *app_id;
  int offset_x;
  int offset_y;
} DragData;

// Libération de la mémoire de DragData
void free_drag_data(gpointer data) {
  DragData *dd = (DragData *)data;
  if (dd) {
    g_free(dd->app_id);
    g_free(dd);
  }
}

// --- GESTION DE LA CONFIGURATION ---
char *get_config_path() {
  return g_build_filename(g_get_home_dir(), CONFIG_FILE, NULL);
}

void save_position(const char *id, int x, int y) {
  // 1. Validation : SEUL 'id' est un pointeur à tester
  if (!id)
    return;

  char *path = get_config_path();
  if (!path)
    return;

  GKeyFile *key_file = g_key_file_new();
  GError *error = NULL;

  if (!g_key_file_load_from_file(key_file, path, G_KEY_FILE_NONE, &error)) {
    if (g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
      // Cas normal : Premier lancement, le fichier sera créé plus bas.
      g_clear_error(&error);
    } else {
      // Gros problème (Permissions, fichier corrompu, etc.)
      g_warning("Sauvegarde annulée. Impossible de lire le fichier existant "
                "'%s' : %s",
                path, error->message);
      g_clear_error(&error);

      // Sécurité : On quitte pour ne pas détruire le fichier existant
      g_key_file_free(key_file);
      g_free(path);
      return;
    }
  }

  // 2. Préparation des données (g_snprintf est déjà très bien)
  char pos[64];
  g_snprintf(pos, sizeof(pos), "%d,%d", x, y);
  g_key_file_set_string(key_file, "Positions", id, pos);

  // On extrait le nom du dossier (ex: /home/alban/.config/polyb)
  char *dirname = g_path_get_dirname(path);
  // On force la création du dossier et de ses parents s'ils n'existent pas
  g_mkdir_with_parents(dirname, 0700);
  g_free(dirname);

  // 3. Conversion et Écriture sécurisée
  gsize length = 0;
  gchar *data = g_key_file_to_data(key_file, &length, NULL);

  if (data) {
    GError *write_error = NULL;

    // g_file_set_contents est atomique (il écrit dans un fichier temporaire
    // puis renomme) C'est super safe, mais il faut écouter s'il échoue.
    if (!g_file_set_contents(path, data, length, &write_error)) {
      g_warning("Échec de la sauvegarde dans '%s' : %s", path,
                write_error->message);
      g_clear_error(&write_error);
    }
    g_free(data);
  }

  // 4. Nettoyage
  g_key_file_free(key_file);
  g_free(path);
}

void load_position(const char *id, int *x, int *y, int def_x, int def_y) {
  // 1. Initialisation par défaut
  if (x)
    *x = def_x;
  if (y)
    *y = def_y;

  // Validation des pointeurs obligatoires
  if (!id || !x || !y)
    return;

  char *path = get_config_path();
  if (!path)
    return;

  GKeyFile *key_file = g_key_file_new();
  GError *load_error = NULL; // Erreur dédiée au chargement

  // 2. Chargement du fichier
  if (g_key_file_load_from_file(key_file, path, G_KEY_FILE_NONE, &load_error)) {

    GError *get_error = NULL; // Nouvelle erreur dédiée à la lecture de la clé

    // 3. Récupération sécurisée de la chaîne
    char *val = g_key_file_get_string(key_file, "Positions", id, &get_error);

    if (val) {
      int parsed_x, parsed_y;
      // 4. Sécurisation du parsing
      if (sscanf(val, "%d,%d", &parsed_x, &parsed_y) == 2) {
        *x = parsed_x;
        *y = parsed_y;
      } else {
        g_warning("Format de position invalide pour l'ID '%s': '%s'", id, val);
      }
      g_free(val);
    } else {
      // Si la clé n'existe pas, on libère l'erreur de lecture
      g_clear_error(&get_error);
    }

  } else {
    // Si le fichier n'existe pas, ce n'est pas un drame, on gère proprement
    if (!g_error_matches(load_error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
      // On n'affiche un warning QUE si c'est autre chose qu'un fichier manquant
      // (ex: permission refusée)
      g_warning("Impossible de charger le fichier de config '%s' : %s", path,
                load_error->message);
    }
    g_clear_error(&load_error);
  }

  // 5. Nettoyage de la mémoire garanti
  g_key_file_free(key_file);
  g_free(path);
}

// --- ÉVÉNEMENTS SOURIS ---
static gboolean on_button_press(GtkWidget *widget, GdkEventButton *event,
                                gpointer data) {
  // 1. Vérification des paramètres essentiels de la fonction
  g_return_val_if_fail(GTK_IS_WIDGET(widget), FALSE);
  g_return_val_if_fail(event != NULL, FALSE);
  g_return_val_if_fail(data != NULL, FALSE);

  DragData *dd = (DragData *)data;

  // 2. Double-clic : Lancement de l'application
  if (event->type == GDK_2BUTTON_PRESS) {
    if (event->button == 1 || event->button == 3) {

      // Extraction et vérification de la présence de l'application associée
      GAppInfo *app_info =
          (GAppInfo *)g_object_get_data(G_OBJECT(widget), "app_info");

      // Sécurité : On s'assure que app_info existe et est valide avant le
      // lancement
      if (app_info && G_IS_APP_INFO(app_info)) {
        GdkDisplay *display = gdk_display_get_default();
        if (display) {
          GdkAppLaunchContext *context =
              gdk_display_get_app_launch_context(display);

          if (context) {
            g_app_info_launch(app_info, NULL, G_APP_LAUNCH_CONTEXT(context),
                              NULL);
            g_object_unref(context);
          }
        }
      } else {
        g_warning("on_button_press: Impossible de lancer l'application, "
                  "'app_info' est manquant ou invalide.");
      }
      return TRUE;
    }
  }

  // 3. Clic simple : Enregistrement des offsets pour le drag & drop
  if (event->type == GDK_BUTTON_PRESS) {
    // On force les offsets à 0 si jamais ils sont négatifs pour éviter les
    // sauts d'icône
    dd->offset_x = (event->x < 0.0) ? 0 : (int)event->x;
    dd->offset_y = (event->y < 0.0) ? 0 : (int)event->y;
  }

  return FALSE;
}

static gboolean on_motion(GtkWidget *widget, GdkEventMotion *event,
                          gpointer data) {

  // 1. Vérifications de sécurité initiales
  g_return_val_if_fail(GTK_IS_WIDGET(widget), FALSE);
  g_return_val_if_fail(event != NULL, FALSE);
  g_return_val_if_fail(data != NULL, FALSE);

  if (event->state & (GDK_BUTTON1_MASK | GDK_BUTTON3_MASK)) {
    DragData *dd = (DragData *)data;
    // Vérification supplémentaire de la structure DragData
    if (dd->app_id == NULL)
      return FALSE;

    // 2. Récupération et vérification du parent
    GtkWidget *parent = gtk_widget_get_parent(widget);
    if (!parent || !GTK_IS_FIXED(parent)) {
      return FALSE; // Si pas de parent ou si le parent n'est pas un GtkFixed,
                    // on arrête tout
    }

    int current_x, current_y;
    gtk_container_child_get(GTK_CONTAINER(parent), widget, "x", &current_x, "y",
                            &current_y, NULL);

    // 1. Calcul des positions théoriques demandées par la souris
    int new_x = current_x + (int)event->x - dd->offset_x;
    int new_y = current_y + (int)event->y - dd->offset_y;

    // 2. Récupération des dimensions du moniteur où se trouve la fenêtre
    GdkDisplay *display = gtk_widget_get_display(widget);
    GdkWindow *window = gtk_widget_get_window(widget);
    GdkMonitor *monitor = gdk_display_get_monitor_at_window(display, window);

    GdkRectangle geo;
    gdk_monitor_get_geometry(monitor, &geo);

    // 3. Récupération de la taille réelle du widget déplacé (l'icône)
    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    int widget_width = allocation.width;
    int widget_height = allocation.height;

    // 4. Sécurité géométrique : On bride (clamp) les coordonnées
    // Limite gauche et haute (pas de valeurs négatives)
    if (new_x < 0)
      new_x = 0;
    if (new_y < 0)
      new_y = 0;

    // Limite droite et basse (on soustrait la taille du widget pour qu'il
    // reste entièrement visible)
    if (new_x > geo.width - widget_width) {
      new_x = geo.width - widget_width;
    }
    if (new_y > geo.height - widget_height) {
      new_y = geo.height - widget_height;
    }

    // 5. Application et sauvegarde des positions sécurisées
    gtk_fixed_move(GTK_FIXED(parent), widget, new_x, new_y);
    save_position(dd->app_id, new_x, new_y);
  }
  return TRUE;
}

// --- CHARGEMENT DU BUREAU ---
void refresh_desktop(GtkWidget *fixed) {
  // SÉCURITÉ : Arrête immédiatement la fonction si 'fixed' est NULL
  // OU si ce n'est pas un conteneur de type GTK_FIXED.
  g_return_if_fail(fixed != NULL);
  g_return_if_fail(GTK_IS_FIXED(fixed));

  const char *desktop_path = g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP);
  if (!desktop_path)
    return;

  GDir *dir = g_dir_open(desktop_path, 0, NULL);
  if (!dir)
    return;

  GList *children = gtk_container_get_children(GTK_CONTAINER(fixed));
  for (GList *l = children; l != NULL; l = l->next) {
    gtk_widget_destroy(GTK_WIDGET(l->data));
  }
  g_list_free(children);

  int def_x = 50, def_y = 50;
  const gchar *filename;
  GdkRectangle geo;
  gdk_monitor_get_geometry(
      gdk_display_get_primary_monitor(gdk_display_get_default()), &geo);

  while ((filename = g_dir_read_name(dir)) != NULL) {
    if (g_str_has_suffix(filename, ".desktop")) {
      gchar *full_path = g_build_filename(desktop_path, filename, NULL);
      GAppInfo *app_info =
          G_APP_INFO(g_desktop_app_info_new_from_filename(full_path));
      if (app_info) {
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget *img = gtk_image_new_from_gicon(g_app_info_get_icon(app_info),
                                                  GTK_ICON_SIZE_DIALOG);
        GtkWidget *lbl = gtk_label_new(g_app_info_get_name(app_info));
        gtk_label_set_max_width_chars(GTK_LABEL(lbl), 12);
        gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_END);

        GtkWidget *event_box = gtk_event_box_new();
        gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box), FALSE);
        gtk_container_add(GTK_CONTAINER(event_box), box);
        gtk_box_pack_start(GTK_BOX(box), img, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(box), lbl, FALSE, FALSE, 0);

        int load_x, load_y;
        load_position(filename, &load_x, &load_y, def_x, def_y);

        DragData *dd = g_new0(DragData, 1);
        dd->app_id = g_strdup(filename);
        g_object_set_data(G_OBJECT(event_box), "app_info", app_info);
        // Utilisation de g_object_set_data_full pour libérer DragData
        // automatiquement à la destruction du widget
        g_object_set_data_full(G_OBJECT(event_box), "drag_data", dd,
                               free_drag_data);

        gtk_widget_add_events(event_box,
                              GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK);
        g_signal_connect(event_box, "button-press-event",
                         G_CALLBACK(on_button_press), dd);
        g_signal_connect(event_box, "motion-notify-event",
                         G_CALLBACK(on_motion), dd);

        gtk_fixed_put(GTK_FIXED(fixed), event_box, load_x, load_y);
        def_y += 120;
        if (def_y > geo.height - 100) {
          def_y = 50;
          def_x += 120;
        }
      }
      g_free(full_path);
    }
  }
  g_dir_close(dir);

  // Sécurisé par le g_return_if_fail du début
  gtk_widget_show_all(fixed);
}

static void on_desktop_changed(GFileMonitor *monitor G_GNUC_UNUSED,
                               GFile *file G_GNUC_UNUSED,
                               GFile *other G_GNUC_UNUSED,
                               GFileMonitorEvent event, gpointer fixed) {
  // 1. Vérification de sécurité sur l'événement
  if (event == G_FILE_MONITOR_EVENT_CREATED ||
      event == G_FILE_MONITOR_EVENT_DELETED) {

    // 2. Vérification du pointeur 'fixed'
    // GTK_IS_WIDGET vérifie à la fois que ce n'est pas NULL et que c'est un
    // widget valide
    if (GTK_IS_WIDGET(fixed)) {
      // On ne rafraîchit que si un fichier est créé ou supprimé
      refresh_desktop(GTK_WIDGET(fixed));
    } else {
      // Optionnel : Afficher un message d'avertissement dans la console pour
      // le debug
      g_warning("on_desktop_changed: 'fixed' n'est pas un GtkWidget valide ou "
                "est NULL !");
    }
  }
}

int main(int argc, char *argv[]) {
  prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);

  char *config_dir =
      g_build_filename(g_get_home_dir(), ".config", "gdesktop", NULL);
  g_mkdir_with_parents(config_dir, 0755);
  g_free(config_dir);

  gtk_init(&argc, &argv);

  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  GdkScreen *screen = gtk_widget_get_screen(window);
  GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
  if (visual != NULL && gdk_screen_is_composited(screen)) {
    gtk_widget_set_visual(window, visual);
  }

  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
  gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DESKTOP);
  gtk_widget_set_app_paintable(window, TRUE);

  GdkRectangle geo;
  gdk_monitor_get_geometry(
      gdk_display_get_primary_monitor(gdk_display_get_default()), &geo);
  gtk_window_set_default_size(GTK_WINDOW(window), geo.width, geo.height);

  GtkWidget *fixed = gtk_fixed_new();
  gtk_container_add(GTK_CONTAINER(window), fixed);

  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_data(provider,
                                  "label { color: white; text-shadow: 2px 2px "
                                  "3px black; font-weight: bold; }",
                                  -1, NULL);
  gtk_style_context_add_provider_for_screen(
      gdk_screen_get_default(), GTK_STYLE_PROVIDER(provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  refresh_desktop(fixed);

  const char *desktop_path = g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP);
  if (desktop_path) {
    GFile *desktop_file = g_file_new_for_path(desktop_path);
    GFileMonitor *monitor =
        g_file_monitor_directory(desktop_file, G_FILE_MONITOR_NONE, NULL, NULL);
    g_signal_connect(monitor, "changed", G_CALLBACK(on_desktop_changed), fixed);
    g_object_unref(desktop_file);
  }

  gtk_widget_show_all(window);
  gtk_main();
  return 0;
}
