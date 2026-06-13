#include <gtk/gtk.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <unistd.h>

/**
 * @file help_center.c
 * @brief Centre d'aide pour Arch Linux utilisant GTK+ 3.
 * * Fournit une interface pour consulter les raccourcis clavier,
 * les pages de manuel système et effectuer des recherches via Surfraw.
 */

#define PROGRAMME_NAME "help-center"

// --- RACCOURCIS ---
/** @brief Chaîne contenant la liste formatée des raccourcis clavier Openbox. */
const char *raccourcis_openbox_str =
    "--- OPENBOX---\n"
    "--- LANCEURS ---\n"
    "• Alt + F1       : Menu Rofi (Applications)\n"
    "• Win + Espace   : Menu Racine (Openbox)\n"
    "• Win + M        : Menu Jgmenu (Personnalisé)\n"
    "• Ctrl + Alt + T : Terminal Alacritty\n"
    "• Win + T        : QTerminal\n"
    "• Win + E        : Explorateur PCManFM\n"
    "• Win + P        : Gestion Imprimantes (CUPS)\n\n"
    "--- GESTION DES FENÊTRES ---\n"
    "• Alt + Tab      : Changer de fenêtre\n"
    "• Alt + F4       : Fermer la fenêtre\n"
    "• Win + Up       : Maximiser\n"
    "• Win + Down     : Restaurer la taille\n"
    "• Win + D        : Afficher le bureau\n\n"
    "--- PLACEMENT & MONITEURS ---\n"
    "• Win + Gauche   : Moitié Gauche\n"
    "• Win + Droite   : Moitié Droite\n"
    "• Win + N        : Déplacer vers moniteur suivant\n\n"
    "--- BUREAUX VIRTUELS ---\n"
    "• Ctrl + Alt + Gche/Drt : Naviguer entre les bureaux\n"
    "• Win + F1 / F2  : Aller au Bureau 1 ou 2\n\n"
    "--- AIDE ---\n"
    "• Win + H        : Afficher cette aide";

// --- MOTEURS DE RECHERCHE SURFRAW ---
/** @brief Liste des moteurs de recherche supportés par Surfraw. */
static const char *moteurs_surfraw[] = {"duckduckgo",
                                        "google",
                                        "bing",
                                        "stack",
                                        "wikipedia",
                                        "youtube",
                                        "acronym",
                                        "ads",
                                        "alioth",
                                        "amazon",
                                        "archpkg",
                                        "archwiki",
                                        "arxiv",
                                        "ask",
                                        "aur",
                                        "austlii",
                                        "bbcnews",
                                        "bookfinder",
                                        "bugmenot",
                                        "bugzilla",
                                        "cia",
                                        "cisco",
                                        "cite",
                                        "cliki",
                                        "cnn",
                                        "comlaw",
                                        "commandlinefu",
                                        "ctan",
                                        "currency",
                                        "cve",
                                        "debbugs",
                                        "debcodesearch",
                                        "debcontents",
                                        "deblists",
                                        "deblogs",
                                        "debpackages",
                                        "debpkghome",
                                        "debpts",
                                        "debsec",
                                        "debvcsbrowse",
                                        "debwiki",
                                        "deja",
                                        "discogs",
                                        "ebay",
                                        "etym",
                                        "excite",
                                        "f5",
                                        "finkpkg",
                                        "foldoc",
                                        "freebsd",
                                        "freedb",
                                        "freshmeat",
                                        "fsfdir",
                                        "gcache",
                                        "genbugs",
                                        "genportage",
                                        "github",
                                        "gmane",
                                        "gutenberg",
                                        "imdb",
                                        "ixquick",
                                        "jamendo",
                                        "javasun",
                                        "jquery",
                                        "l1sp",
                                        "lastfm",
                                        "leodict",
                                        "lsm",
                                        "macports",
                                        "mathworld",
                                        "mdn",
                                        "mininova",
                                        "musicbrainz",
                                        "mysqldoc",
                                        "netbsd",
                                        "nlab",
                                        "ntrs",
                                        "openbsd",
                                        "oraclesearch",
                                        "pgdoc",
                                        "pgpkeys",
                                        "phpdoc",
                                        "pin",
                                        "piratebay",
                                        "priberam",
                                        "pubmed",
                                        "rae",
                                        "rfc",
                                        "scholar",
                                        "scpan",
                                        "searx",
                                        "slashdot",
                                        "slinuxdoc",
                                        "sourceforge",
                                        "springer",
                                        "stockquote",
                                        "thesaurus",
                                        "translate",
                                        "urban",
                                        "w3css",
                                        "w3html",
                                        "w3link",
                                        "w3rdf",
                                        "wayback",
                                        "webster",
                                        "wiktionary",
                                        "woffle",
                                        "wolfram",
                                        "worldwidescience",
                                        "yahoo",
                                        "yandex"};

/**
 * @struct AppWidgets
 * @brief Structure regroupant les widgets principaux de l'application pour un
 * accès facile dans les callbacks.
 */
typedef struct {
  GtkWidget *window;
  GtkWidget *stack;
  GtkWidget *man_entry;
  GtkWidget *man_listbox;
  GtkWidget *surf_entry;
  GtkWidget *surf_combo;
  guint search_timeout_id;
} AppWidgets;

// --- STYLE ---
/**
 * @brief Applique le thème CSS (Nord Theme) à l'application.
 * Définit les couleurs de fond, le style des boutons et des champs de saisie.
 */
static void apply_style() {
  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_data(
      provider,
      "window { background-color: #242933; }"
      "button { background-image: none; background-color: #3b4252; color: "
      "#eceff4; border-radius: 6px; border: 1px solid #4c566a; padding: 12px; "
      "margin: 2px; }"
      "button:hover { background-color: #434c5e; border-color: #88c0d0; }"
      "entry { background-color: #3b4252; color: #eceff4; border-radius: 4px; "
      "padding: 10px; border: 1px solid #4c566a; }"
      "textview text { background-color: #2e3440; color: #d8dee9; font-family: "
      "'Monospace'; }",
      -1, NULL);
  gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                            GTK_STYLE_PROVIDER(provider), 800);
  g_object_unref(provider);
}

// --- UTILS ---
/**
 * @brief Lance une commande shell de manière asynchrone sans bloquer l'UI.
 * @param cmd La chaîne de commande à exécuter.
 */
static void safe_spawn(const char *cmd) {
  // 1. Vérification de sécurité : on s'assure que le pointeur n'est pas NULL
  if (cmd == NULL) {
    g_printerr("Erreur [safe_spawn] : La commande fournie est NULL.\n");
    return;
  }

  // 2. Optionnel : On vérifie que la commande n'est pas une chaîne vide ""
  if (strlen(cmd) == 0) {
    g_printerr("Erreur [safe_spawn] : La commande fournie est vide.\n");
    return;
  }

  GError *error = NULL;
  if (!g_spawn_command_line_async(cmd, &error)) {
    g_printerr("Erreur : %s\n", error->message);
    g_error_free(error);
  }
}

/**
 * @brief Libère la mémoire allouée pour le nom d'une page de manuel.
 * Utilisé comme callback de destruction pour les signaux.
 */
static void free_page_name(gpointer data, GClosure *closure G_GNUC_UNUSED) {
  // Sécurité : On ne libère que si le pointeur contient une adresse valide
  if (data != NULL) {
    g_free(data);
  }
}

// --- NAVIGATION (À placer AVANT create_back_button) ---
/** @brief Change la vue active du Stack vers la page d'accueil. */
void go_to_main(GtkWidget *w G_GNUC_UNUSED, gpointer d) {
  // Sécurité : On vérifie que 'd' n'est pas NULL et qu'il s'agit bien d'un
  // GtkStack
  if (d == NULL || !GTK_IS_STACK(d)) {
    g_printerr("Erreur [go_to_main] : Le pointeur GtkStack est invalide.\n");
    return;
  }
  gtk_stack_set_visible_child_name(GTK_STACK(d), "main");
}

/** @brief Change la vue active du Stack vers la page des manuels. */
void go_to_man(GtkWidget *w G_GNUC_UNUSED, gpointer d) {
  if (d == NULL || !GTK_IS_STACK(d)) {
    g_printerr("Erreur [go_to_man] : Le pointeur GtkStack est invalide.\n");
    return;
  }
  gtk_stack_set_visible_child_name(GTK_STACK(d), "man");
}

/** @brief Change la vue active du Stack vers la page de recherche web. */
void go_to_surf(GtkWidget *w G_GNUC_UNUSED, gpointer d) {
  if (d == NULL || !GTK_IS_STACK(d)) {
    g_printerr("Erreur [go_to_surf] : Le pointeur GtkStack est invalide.\n");
    return;
  }
  gtk_stack_set_visible_child_name(GTK_STACK(d), "surf");
}

// --- FONCTIONS DE REFACTORISATION ---
/**
 * @brief Ajoute plusieurs widgets à un GtkBox en une seule fois.
 * @param box Le conteneur GtkBox cible.
 * @param ... Liste de pointeurs GtkWidget terminant par NULL.
 */
static void box_pack_many(GtkBox *box, ...) {
  // 1. Sécurité : On vérifie que la boîte destinataire existe et est bien un
  // GtkBox
  if (box == NULL || !GTK_IS_BOX(box)) {
    g_printerr("Erreur [box_pack_many] : Le conteneur GtkBox est invalide.\n");
    return;
  }

  va_list args;
  va_start(args, box);
  GtkWidget *child;

  // 2. On parcourt les arguments variables
  while ((child = va_arg(args, GtkWidget *)) != NULL) {
    // Sécurité : On vérifie que le widget à insérer est valide pour éviter un
    // crash
    if (GTK_IS_WIDGET(child)) {
      gtk_box_pack_start(box, child, FALSE, FALSE, 0);
    } else {
      g_printerr("Avertissement [box_pack_many] : Un argument fourni n'est pas "
                 "un GtkWidget valide (ignoré).\n");
    }
  }

  va_end(args);
}

/**
 * @brief Crée et ajoute un bouton de menu standard au conteneur.
 * @param box Le conteneur cible.
 * @param label Texte affiché sur le bouton.
 * @param callback Fonction à appeler lors du clic.
 * @param data Données utilisateur à passer au callback.
 */
static void add_menu_button(GtkBox *box, const char *label, GCallback callback,
                            gpointer data) {
  // 1. Validation du conteneur parent
  if (box == NULL || !GTK_IS_BOX(box)) {
    g_printerr(
        "Erreur [add_menu_button] : Le conteneur GtkBox est invalide.\n");
    return;
  }

  // 2. Validation du texte du bouton
  if (label == NULL || strlen(label) == 0) {
    g_printerr("Erreur [add_menu_button] : Le texte (label) fourni est NULL ou "
               "vide.\n");
    return;
  }

  // 3. Validation de la fonction de rappel (callback)
  if (callback == NULL) {
    g_printerr(
        "Erreur [add_menu_button] : Le callback fourni pour '%s' est NULL.\n",
        label);
    return;
  }

  // 4. Création et configuration sécurisée du bouton
  GtkWidget *btn = gtk_button_new_with_label(label);

  if (GTK_IS_WIDGET(btn)) {
    g_signal_connect(btn, "clicked", callback, data);
    gtk_box_pack_start(box, btn, FALSE, FALSE, 0);
  } else {
    g_printerr("Erreur [add_menu_button] : Échec de la création du GtkButton "
               "pour '%s'.\n",
               label);
  }
}

/**
 * @brief Crée un bouton "Retour" configuré pour revenir à la page principale.
 * @param app Pointeur vers la structure des widgets.
 * @return Un pointeur vers le nouveau GtkWidget bouton, ou NULL en cas
 * d'erreur.
 */
static GtkWidget *create_back_button(AppWidgets *app) {
  // 1. Sécurité : On vérifie que la structure principale n'est pas NULL
  if (app == NULL) {
    g_printerr(
        "Erreur [create_back_button] : Le pointeur AppWidgets est NULL.\n");
    return NULL;
  }

  // 2. Sécurité : On vérifie que le Stack existe dans la structure
  if (app->stack == NULL || !GTK_IS_STACK(app->stack)) {
    g_printerr("Erreur [create_back_button] : Le composant app->stack est "
               "invalide.\n");
    return NULL;
  }

  GtkWidget *btn = gtk_button_new_with_label("⬅  Retour");

  // 3. Sécurité : On vérifie que le bouton a bien pu être créé par GTK
  if (GTK_IS_WIDGET(btn)) {
    g_signal_connect(btn, "clicked", G_CALLBACK(go_to_main), app->stack);
    return btn;
  }

  return NULL;
}

// --- MANUELS ---
/**
 * @brief Ouvre une nouvelle fenêtre affichant le contenu d'une page de manuel.
 * @param page Nom de la page de manuel (ex: "grep").
 */
void show_man_internal(const char *page) {
  // 1. Sécurité : Vérification du pointeur NULL et chaîne vide
  if (page == NULL || strlen(page) == 0) {
    g_printerr(
        "Erreur [show_man_internal] : Le nom de la page est NULL ou vide.\n");
    return;
  }

  // 2. Sécurité absolue : On vérifie que la chaîne ne contient pas de
  // caractères malveillants Une page de manuel légitime ne contient que des
  // lettres, chiffres, tirets, underscores ou points (ex: "pwrite64",
  // "pthread_create")
  const char *allowed_chars =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.:";
  if (strspn(page, allowed_chars) != strlen(page)) {
    g_printerr("Erreur Sécurité [show_man_internal] : Caractères interdits "
               "détectés dans le nom de la page : '%s'\n",
               page);
    return;
  }

  GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  if (!GTK_IS_WIDGET(win))
    return;

  gtk_window_set_title(GTK_WINDOW(win), page);
  gtk_window_set_default_size(GTK_WINDOW(win), 700, 800);

  GtkWidget *view = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
  gtk_text_view_set_left_margin(GTK_TEXT_VIEW(view), 20);

  // Désormais sans danger grâce au filtre strspn au-dessus
  char *cmd = g_strdup_printf("man %s | col -b", page);
  FILE *fp = popen(cmd, "r");

  if (fp) {
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
      gtk_text_buffer_insert_at_cursor(buf, line, -1);
    }
    pclose(fp);
  } else {
    g_printerr("Erreur [show_man_internal] : Impossible d'exécuter popen pour "
               "la commande '%s'.\n",
               cmd);
  }
  g_free(cmd);

  GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
  if (GTK_IS_WIDGET(scroll)) {
    gtk_container_add(GTK_CONTAINER(scroll), view);
    gtk_container_add(GTK_CONTAINER(win), scroll);
  }

  gtk_widget_show_all(win);
}

/**
 * @brief Callback appelé lors de la sélection d'une page de manuel dans la
 * liste.
 */
void on_man_selected(GtkButton *btn, gpointer data) {
  // 1. Sécurité : On vérifie que le widget émetteur est bien un bouton valide
  if (btn == NULL || !GTK_IS_BUTTON(btn)) {
    g_printerr("Erreur [on_man_selected] : Le widget émetteur est invalide.\n");
    return;
  }

  // 2. Sécurité : On s'assure que les données passées ne sont pas NULL
  if (data == NULL) {
    g_printerr(
        "Erreur [on_man_selected] : Le nom de la page (data) est NULL.\n");
    return;
  }

  // On passe la main à la fonction interne désormais sécurisée
  show_man_internal((const char *)data);
}

/**
 * @brief Exécute la recherche de manuels via 'man -k'.
 * Cette fonction est appelée après un court délai pour éviter de surcharger le
 * système.
 * @return Toujours FALSE pour arrêter le timer GSource.
 */
static gboolean perform_man_search(gpointer data) {
  // 1. Sécurité : On valide immédiatement le pointeur principal de la structure
  if (data == NULL) {
    g_printerr("Erreur [perform_man_search] : Les données AppWidgets (data) "
               "sont NULL.\n");
    return FALSE; // Retourne FALSE pour ne pas boucler le timer dans le vide
  }

  AppWidgets *app = (AppWidgets *)data;

  // 2. Sécurité : On s'assure que les widgets internes nécessaires existent et
  // sont du bon type
  if (app->man_entry == NULL || !GTK_IS_ENTRY(app->man_entry) ||
      app->man_listbox == NULL || !GTK_IS_LIST_BOX(app->man_listbox)) {
    g_printerr("Erreur [perform_man_search] : Les widgets de recherche "
               "(entry/listbox) sont invalides.\n");
    app->search_timeout_id = 0;
    return FALSE;
  }

  const char *text = gtk_entry_get_text(GTK_ENTRY(app->man_entry));

  // Nettoyage de la liste existante
  GList *children = gtk_container_get_children(GTK_CONTAINER(app->man_listbox));
  for (GList *l = children; l != NULL; l = l->next) {
    if (l->data != NULL && GTK_IS_WIDGET(l->data)) {
      gtk_widget_destroy(GTK_WIDGET(l->data));
    }
  }
  g_list_free(children);

  // Si le texte est trop court, on s'arrête proprement
  if (text == NULL || strlen(text) < 2) {
    app->search_timeout_id = 0;
    return FALSE;
  }

  // 3. Sécurité Absolue : On filtre la saisie de l'utilisateur avant de
  // l'envoyer à popen On autorise uniquement les caractères classiques d'une
  // commande (lettres, chiffres, tirets, points)
  const char *allowed_chars =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.";
  if (strspn(text, allowed_chars) != strlen(text)) {
    g_printerr("Avertissement Sécurité [perform_man_search] : Caractères "
               "invalides ignorés dans la recherche.\n");
    app->search_timeout_id = 0;
    return FALSE;
  }

  // Commande désormais sécurisée contre les injections
  char *cmd = g_strdup_printf("man -k %s | head -n 15", text);
  FILE *fp = popen(cmd, "r");

  if (fp) {
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
      line[strcspn(line, "\n")] = 0; // Nettoyage du saut de ligne

      char *copy = g_strdup(line);
      if (copy == NULL)
        continue;

      char *token = strtok(copy, " ");
      if (token) {
        GtkWidget *btn = gtk_button_new_with_label(line);
        if (GTK_IS_WIDGET(btn)) {
          gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
          gtk_widget_set_halign(btn, GTK_ALIGN_START);

          char *page = g_strdup(token);
          if (page != NULL) {
            g_signal_connect_data(btn, "clicked", G_CALLBACK(on_man_selected),
                                  page, (GClosureNotify)free_page_name, 0);
            gtk_container_add(GTK_CONTAINER(app->man_listbox), btn);
          }
        }
      }
      g_free(copy);
    }
    pclose(fp);
  }
  g_free(cmd);

  gtk_widget_show_all(app->man_listbox);
  app->search_timeout_id = 0;
  return FALSE; // Renvoie FALSE pour détruire le timer GSource à la fin de
                // l'exécution
}

/**
 * @brief Gère l'événement de changement de texte dans le champ de recherche
 * man. Initialise ou réinitialise le délai d'attente avant de lancer la
 * recherche.
 */
void on_man_search_changed(GtkSearchEntry *e, gpointer data) {

  // 1. Sécurité : On valide immédiatement le widget émetteur
  if (e == NULL || !GTK_IS_SEARCH_ENTRY(e)) {
    g_printerr("Erreur [on_man_search_changed] : Le widget GtkSearchEntry est "
               "invalide.\n");
    return;
  }

  // 2. Sécurité : On valide immédiatement le pointeur principal de la structure
  if (data == NULL) {
    g_printerr("Erreur [on_man_search_changed] : Les données AppWidgets (data) "
               "sont NULL.\n");
    return;
  }

  AppWidgets *app = (AppWidgets *)data;

  // Réinitialisation du timer s'il y en avait un en cours
  if (app->search_timeout_id != 0) {
    g_source_remove(app->search_timeout_id);
  }

  // Lancement du nouveau délai de 500ms
  app->search_timeout_id = g_timeout_add(500, perform_man_search, app);
}

// --- ACTIONS ---
/**
 * @brief Exécute une recherche Surfraw avec le moteur et la requête
 * sélectionnés. Ferme l'application après le lancement.
 */
void on_surfraw_exec(GtkWidget *w, gpointer data) {
  // 1. Sécurité : On valide immédiatement le widget émetteur (optionnel mais
  // propre)
  if (w == NULL || !GTK_IS_WIDGET(w)) {
    g_printerr("Erreur [on_surfraw_exec] : Widget émetteur invalide.\n");
    return;
  }

  // 2. Sécurité : On valide le pointeur principal de la structure
  if (data == NULL) {
    g_printerr("Erreur [on_surfraw_exec] : Les données AppWidgets (data) sont "
               "NULL.\n");
    return;
  }

  AppWidgets *app = (AppWidgets *)data;

  // 3. Sécurité : On valide les widgets internes avant de s'en servir
  if (app->surf_combo == NULL || !GTK_IS_COMBO_BOX_TEXT(app->surf_combo) ||
      app->surf_entry == NULL || !GTK_IS_ENTRY(app->surf_entry)) {
    g_printerr("Erreur [on_surfraw_exec] : Les widgets d'entrée Surfraw sont "
               "invalides.\n");
    return;
  }

  char *engine =
      gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(app->surf_combo));
  const char *query = gtk_entry_get_text(GTK_ENTRY(app->surf_entry));

  if (query && strlen(query) > 0) {
    // 4. Sécurité Absolue : On protège le moteur et la requête contre les
    // injections Shell
    char *safe_engine = g_shell_quote(engine ? engine : "duckduckgo");
    char *safe_query = g_shell_quote(query);

    // On peut maintenant assembler la commande sans aucun risque, même s'il y a
    // des ' ou des ;
    char *cmd = g_strdup_printf("surfraw %s %s", safe_engine, safe_query);

    safe_spawn(cmd);

    // Libération des chaînes sécurisées
    g_free(cmd);
    g_free(safe_engine);
    g_free(safe_query);

    // Quitter proprement l'application après le lancement
    gtk_main_quit();
  }

  g_free(engine);
}

/**
 * @brief Lance le script externe de rapport système.
 */
void launch_syslog(GtkWidget *w, gpointer d G_GNUC_UNUSED) {
  // 1. Sécurité : On vérifie que le widget émetteur est valide
  if (w == NULL || !GTK_IS_WIDGET(w)) {
    g_printerr("Erreur [launch_syslog] : Widget émetteur invalide.\n");
    return;
  }

  const char *script_path = "/usr/local/bin/syslog_report";

  // 2. Sécurité : On vérifie si l'exécutable est présent ET exécutable
  // F_OK = Existe ?, X_OK = Exécutable ? (Le binaire OR permet de tester les
  // deux)
  if (access(script_path, F_OK | X_OK) != 0) {
    g_printerr("Erreur [launch_syslog] : Le script '%s' est introuvable ou n'a "
               "pas les permissions d'exécution.\n",
               script_path);

    // Optionnel : On peut afficher une boîte de dialogue d'erreur à
    // l'utilisateur ici
    return; // On quitte la fonction SANS fermer l'application principale
  }

  // 3. Si tout est OK, on lance et on ferme l'UI
  safe_spawn(script_path);
  gtk_main_quit();
}

/**
 * @brief Affiche la fenêtre contextuelle des raccourcis clavier.
 */
/**
 * @brief Affiche la fenêtre contextuelle des raccourcis clavier.
 */
void on_shortcuts_clicked(GtkWidget *w G_GNUC_UNUSED,
                          gpointer d G_GNUC_UNUSED) {

  // 1. Sécurité : On vérifie que la chaîne de texte globale existe et n'est pas
  // NULL
  if (raccourcis_openbox_str == NULL) {
    g_printerr("Erreur [on_shortcuts_clicked] : La chaîne "
               "'raccourcis_openbox_str' est NULL.\n");
    return;
  }

  GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  // 2. Sécurité : On s'assure que la fenêtre a bien pu être créée par le
  // système
  if (win == NULL || !GTK_IS_WIDGET(win)) {
    g_printerr("Erreur [on_shortcuts_clicked] : Impossible de créer la fenêtre "
               "GTK.\n");
    return;
  }

  gtk_window_set_title(GTK_WINDOW(win), "Raccourcis");
  gtk_window_set_default_size(GTK_WINDOW(win), 500, 600);

  GtkWidget *view = gtk_text_view_new();
  if (view != NULL && GTK_IS_TEXT_VIEW(view)) {
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(view), 25);

    // Désormais totalement sûr grâce à la vérification du NULL en haut
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(view)),
                             raccourcis_openbox_str, -1);
  }

  GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);

  // 3. Sécurité : On vérifie les conteneurs avant d'imbriquer les widgets
  if (scroll != NULL && GTK_IS_CONTAINER(scroll) && GTK_IS_CONTAINER(win) &&
      GTK_IS_WIDGET(view)) {
    gtk_container_add(GTK_CONTAINER(scroll), view);
    gtk_container_add(GTK_CONTAINER(win), scroll);
  }

  gtk_widget_show_all(win);
}

// --- MAIN ---
/**
 * @brief Point d'entrée principal.
 * Initialise GTK, construit l'interface, configure le Stack et lance la boucle
 * principale.
 */
int main(int argc, char *argv[]) {
  prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
  gtk_init(&argc, &argv);
  apply_style();

  AppWidgets *app = g_malloc0(sizeof(AppWidgets));

  app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(app->window), "Arch Help Center");
  gtk_window_set_default_size(GTK_WINDOW(app->window), 420, 550);
  g_signal_connect(app->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

  app->stack = gtk_stack_new();
  gtk_stack_set_transition_type(GTK_STACK(app->stack),
                                GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
  gtk_container_add(GTK_CONTAINER(app->window), app->stack);

  // 1. PAGE ACCUEIL
  GtkWidget *vbox_main = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
  gtk_container_set_border_width(GTK_CONTAINER(vbox_main), 30);
  GtkWidget *lbl = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(lbl), "<span size='xx-large' weight='bold' "
                                       "foreground='#88c0d0'>Arch Help</span>");

  gtk_box_pack_start(GTK_BOX(vbox_main), lbl, FALSE, FALSE, 10);
  add_menu_button(GTK_BOX(vbox_main), "📖  Raccourcis Clavier",
                  G_CALLBACK(on_shortcuts_clicked), NULL);
  add_menu_button(GTK_BOX(vbox_main), "🔍  Manuels Système",
                  G_CALLBACK(go_to_man), app->stack);
  add_menu_button(GTK_BOX(vbox_main), "🌐  Recherche Web",
                  G_CALLBACK(go_to_surf), app->stack);
  add_menu_button(GTK_BOX(vbox_main), "🛠️  Rapport Système",
                  G_CALLBACK(launch_syslog), NULL);

  // 2. PAGE MAN
  GtkWidget *vbox_man = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
  gtk_container_set_border_width(GTK_CONTAINER(vbox_man), 15);
  app->man_entry = gtk_search_entry_new();
  app->man_listbox = gtk_list_box_new();
  g_signal_connect(app->man_entry, "search-changed",
                   G_CALLBACK(on_man_search_changed), app);
  GtkWidget *sc1 = gtk_scrolled_window_new(NULL, NULL);
  gtk_container_add(GTK_CONTAINER(sc1), app->man_listbox);

  box_pack_many(GTK_BOX(vbox_man), create_back_button(app), app->man_entry,
                NULL);
  gtk_box_pack_start(GTK_BOX(vbox_man), sc1, TRUE, TRUE, 0);

  // 3. PAGE SURF
  GtkWidget *vbox_surf = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
  gtk_container_set_border_width(GTK_CONTAINER(vbox_surf), 20);
  app->surf_combo = gtk_combo_box_text_new();
  for (guint i = 0; i < G_N_ELEMENTS(moteurs_surfraw); i++) {
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->surf_combo),
                                   moteurs_surfraw[i]);
  }
  gtk_combo_box_set_active(GTK_COMBO_BOX(app->surf_combo), 0);
  app->surf_entry = gtk_entry_new();
  g_signal_connect(app->surf_entry, "activate", G_CALLBACK(on_surfraw_exec),
                   app);
  GtkWidget *btn_go = gtk_button_new_with_label("Lancer la recherche");
  g_signal_connect(btn_go, "clicked", G_CALLBACK(on_surfraw_exec), app);

  box_pack_many(GTK_BOX(vbox_surf), create_back_button(app), app->surf_combo,
                app->surf_entry, btn_go, NULL);

  // AJOUT AU STACK
  struct {
    GtkWidget *w;
    const char *n;
  } pg[] = {{vbox_main, "main"}, {vbox_man, "man"}, {vbox_surf, "surf"}};
  for (guint i = 0; i < G_N_ELEMENTS(pg); i++)
    gtk_stack_add_named(GTK_STACK(app->stack), pg[i].w, pg[i].n);

  gtk_widget_show_all(app->window);
  gtk_main();

  if (app->search_timeout_id != 0)
    g_source_remove(app->search_timeout_id);
  g_free(app);
  return 0;
}