#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <stdarg.h> // Nécessaire pour box_pack_many

#define PROGRAMME_NAME "help-center"

// --- RACCOURCIS ---
const char *raccourcis_openbox_str = 
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
static const char *moteurs_surfraw[] = {
    "duckduckgo", "google", "bing", "stack", "wikipedia", "youtube",
    "acronym", "ads", "alioth", "amazon", "archpkg", "archwiki", "arxiv", "ask",
    "aur", "austlii", "bbcnews", "bookfinder", "bugmenot", "bugzilla", "cia",
    "cisco", "cite", "cliki", "cnn", "comlaw", "commandlinefu", "ctan", "currency",
    "cve", "debbugs", "debcodesearch", "debcontents", "deblists", "deblogs",
    "debpackages", "debpkghome", "debpts", "debsec", "debvcsbrowse", "debwiki",
    "deja", "discogs", "ebay", "etym", "excite", "f5", "finkpkg", "foldoc",
    "freebsd", "freedb", "freshmeat", "fsfdir", "gcache", "genbugs", "genportage",
    "github", "gmane", "gutenberg", "imdb", "ixquick", "jamendo", "javasun",
    "jquery", "l1sp", "lastfm", "leodict", "lsm", "macports", "mathworld", "mdn",
    "mininova", "musicbrainz", "mysqldoc", "netbsd", "nlab", "ntrs", "openbsd",
    "oraclesearch", "pgdoc", "pgpkeys", "phpdoc", "pin", "piratebay", "priberam",
    "pubmed", "rae", "rfc", "scholar", "scpan", "searx", "slashdot", "slinuxdoc",
    "sourceforge", "springer", "stockquote", "thesaurus", "translate", "urban",
    "w3css", "w3html", "w3link", "w3rdf", "wayback", "webster", "wiktionary",
    "woffle", "wolfram", "worldwidescience", "yahoo", "yandex"
};

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
static void apply_style() {
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        "window { background-color: #242933; }"
        "button { background-image: none; background-color: #3b4252; color: #eceff4; border-radius: 6px; border: 1px solid #4c566a; padding: 12px; margin: 2px; }"
        "button:hover { background-color: #434c5e; border-color: #88c0d0; }"
        "entry { background-color: #3b4252; color: #eceff4; border-radius: 4px; padding: 10px; border: 1px solid #4c566a; }"
        "textview text { background-color: #2e3440; color: #d8dee9; font-family: 'Monospace'; }", -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(provider), 800);
    g_object_unref(provider);
}

// --- UTILS ---
static void safe_spawn(const char *cmd) {
    GError *error = NULL;
    if (!g_spawn_command_line_async(cmd, &error)) {
        g_printerr("Erreur : %s\n", error->message);
        g_error_free(error);
    }
}

static void free_page_name(gpointer data, GClosure *closure G_GNUC_UNUSED) {
    g_free(data);
}

// --- NAVIGATION (À placer AVANT create_back_button) ---
void go_to_main(GtkWidget *w G_GNUC_UNUSED, gpointer d) { gtk_stack_set_visible_child_name(GTK_STACK(d), "main"); }
void go_to_man(GtkWidget *w G_GNUC_UNUSED, gpointer d) { gtk_stack_set_visible_child_name(GTK_STACK(d), "man"); }
void go_to_surf(GtkWidget *w G_GNUC_UNUSED, gpointer d) { gtk_stack_set_visible_child_name(GTK_STACK(d), "surf"); }

// --- FONCTIONS DE REFACTORISATION ---
static void box_pack_many(GtkBox *box, ...) {
    va_list args;
    va_start(args, box);
    GtkWidget *child;
    while ((child = va_arg(args, GtkWidget *)) != NULL) {
        gtk_box_pack_start(box, child, FALSE, FALSE, 0);
    }
    va_end(args);
}

static void add_menu_button(GtkBox *box, const char *label, GCallback callback, gpointer data) {
    GtkWidget *btn = gtk_button_new_with_label(label);
    g_signal_connect(btn, "clicked", callback, data);
    gtk_box_pack_start(box, btn, FALSE, FALSE, 0);
}

static GtkWidget* create_back_button(AppWidgets *app) {
    GtkWidget *btn = gtk_button_new_with_label("⬅  Retour");
    // Maintenant, go_to_main est connu du compilateur
    g_signal_connect(btn, "clicked", G_CALLBACK(go_to_main), app->stack);
    return btn;
}

// --- MANUELS ---
void show_man_internal(const char *page) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), page);
    gtk_window_set_default_size(GTK_WINDOW(win), 700, 800);
    GtkWidget *view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(view), 20);
    char *cmd = g_strdup_printf("man %s | col -b", page);
    FILE *fp = popen(cmd, "r");
    if (fp) {
        GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
        char line[1024];
        while (fgets(line, sizeof(line), fp)) gtk_text_buffer_insert_at_cursor(buf, line, -1);
        pclose(fp);
    }
    g_free(cmd);
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), view);
    gtk_container_add(GTK_CONTAINER(win), scroll);
    gtk_widget_show_all(win);
}

void on_man_selected(GtkButton *btn G_GNUC_UNUSED, gpointer data) {
    show_man_internal((const char *)data);
}

static gboolean perform_man_search(gpointer data) {
    AppWidgets *app = (AppWidgets *)data;
    const char *text = gtk_entry_get_text(GTK_ENTRY(app->man_entry));
    GList *children = gtk_container_get_children(GTK_CONTAINER(app->man_listbox));
    for (GList *l = children; l != NULL; l = l->next) gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);
    if (strlen(text) < 2) { app->search_timeout_id = 0; return FALSE; }
    char *cmd = g_strdup_printf("man -k %s | head -n 15", text);
    FILE *fp = popen(cmd, "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            char *copy = g_strdup(line);
            char *token = strtok(copy, " ");
            if (token) {
                GtkWidget *btn = gtk_button_new_with_label(line);
                gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
                gtk_widget_set_halign(btn, GTK_ALIGN_START);
                char *page = g_strdup(token);
                g_signal_connect_data(btn, "clicked", G_CALLBACK(on_man_selected), page, (GClosureNotify)free_page_name, 0);
                gtk_container_add(GTK_CONTAINER(app->man_listbox), btn);
            }
            g_free(copy);
        }
        pclose(fp);
    }
    g_free(cmd);
    gtk_widget_show_all(app->man_listbox);
    app->search_timeout_id = 0;
    return FALSE;
}

void on_man_search_changed(GtkSearchEntry *e G_GNUC_UNUSED, gpointer d) {
    AppWidgets *app = (AppWidgets *)d;
    if (app->search_timeout_id != 0) g_source_remove(app->search_timeout_id);
    app->search_timeout_id = g_timeout_add(500, perform_man_search, app);
}

// --- ACTIONS ---
void on_surfraw_exec(GtkWidget *w G_GNUC_UNUSED, gpointer data) {
    AppWidgets *app = (AppWidgets *)data;
    char *engine = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(app->surf_combo));
    const char *query = gtk_entry_get_text(GTK_ENTRY(app->surf_entry));
    if (query && strlen(query) > 0) {
        char *cmd = g_strdup_printf("surfraw %s '%s'", engine ? engine : "duckduckgo", query);
        safe_spawn(cmd);
        g_free(cmd);
        gtk_main_quit();
    }
    g_free(engine);
}

void launch_syslog(GtkWidget *w G_GNUC_UNUSED, gpointer d G_GNUC_UNUSED) {
    safe_spawn("/usr/local/bin/syslog_report");
    gtk_main_quit();
}

void on_shortcuts_clicked(GtkWidget *w G_GNUC_UNUSED, gpointer d G_GNUC_UNUSED) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Raccourcis");
    gtk_window_set_default_size(GTK_WINDOW(win), 500, 600);
    GtkWidget *view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(view), 25);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(view)), raccourcis_openbox_str, -1);
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), view);
    gtk_container_add(GTK_CONTAINER(win), scroll);
    gtk_widget_show_all(win);
}

// --- MAIN ---
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
    gtk_stack_set_transition_type(GTK_STACK(app->stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_container_add(GTK_CONTAINER(app->window), app->stack);

    // 1. PAGE ACCUEIL
    GtkWidget *vbox_main = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_container_set_border_width(GTK_CONTAINER(vbox_main), 30);
    GtkWidget *lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl), "<span size='xx-large' weight='bold' foreground='#88c0d0'>Arch Help</span>");
    
    gtk_box_pack_start(GTK_BOX(vbox_main), lbl, FALSE, FALSE, 10);
    add_menu_button(GTK_BOX(vbox_main), "📖  Raccourcis Clavier", G_CALLBACK(on_shortcuts_clicked), NULL);
    add_menu_button(GTK_BOX(vbox_main), "🔍  Manuels Système",    G_CALLBACK(go_to_man),           app->stack);
    add_menu_button(GTK_BOX(vbox_main), "🌐  Recherche Web",      G_CALLBACK(go_to_surf),          app->stack);
    add_menu_button(GTK_BOX(vbox_main), "🛠️  Rapport Système",    G_CALLBACK(launch_syslog),       NULL);

    // 2. PAGE MAN
    GtkWidget *vbox_man = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(vbox_man), 15);
    app->man_entry = gtk_search_entry_new();
    app->man_listbox = gtk_list_box_new();
    g_signal_connect(app->man_entry, "search-changed", G_CALLBACK(on_man_search_changed), app);
    GtkWidget *sc1 = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(sc1), app->man_listbox);
    
    box_pack_many(GTK_BOX(vbox_man), create_back_button(app), app->man_entry, NULL);
    gtk_box_pack_start(GTK_BOX(vbox_man), sc1, TRUE, TRUE, 0);

    // 3. PAGE SURF
    GtkWidget *vbox_surf = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(vbox_surf), 20);
    app->surf_combo = gtk_combo_box_text_new();
    for (guint i = 0; i < G_N_ELEMENTS(moteurs_surfraw); i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->surf_combo), moteurs_surfraw[i]);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->surf_combo), 0);
    app->surf_entry = gtk_entry_new();
    g_signal_connect(app->surf_entry, "activate", G_CALLBACK(on_surfraw_exec), app);
    GtkWidget *btn_go = gtk_button_new_with_label("Lancer la recherche");
    g_signal_connect(btn_go, "clicked", G_CALLBACK(on_surfraw_exec), app);
    
    box_pack_many(GTK_BOX(vbox_surf), create_back_button(app), app->surf_combo, app->surf_entry, btn_go, NULL);

    // AJOUT AU STACK
    struct { GtkWidget *w; const char *n; } pg[] = {{vbox_main, "main"}, {vbox_man, "man"}, {vbox_surf, "surf"}};
    for (guint i = 0; i < G_N_ELEMENTS(pg); i++) gtk_stack_add_named(GTK_STACK(app->stack), pg[i].w, pg[i].n);

    gtk_widget_show_all(app->window);
    gtk_main();

    if (app->search_timeout_id != 0) g_source_remove(app->search_timeout_id);
    g_free(app);
    return 0;
}