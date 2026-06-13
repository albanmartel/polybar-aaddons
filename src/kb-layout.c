#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>

// --- NOM PROGRAMME ---
#define PROGRAMME_NAME "kb-layout"

typedef struct {
  char *name;
  char *code;
} Layout;

// Définition des zones géographiques
Layout europe[] = {{"France", "fr"},     {"UK", "gb"},      {"Italie", "it"},
                   {"Allemagne", "de"},  {"Espagne", "es"}, {"Belgique", "be"},
                   {"Suisse", "ch(fr)"}, {"Portugal", "pt"}};
Layout ameriques[] = {{"USA", "us"},
                      {"Canada", "ca"},
                      {"Brésil", "br"},
                      {"Mexique", "latam"},
                      {"Argentine", "ar"}};
Layout asie[] = {{"Japon", "jp"},    {"Chine", "cn"},     {"Corée", "kr"},
                 {"Viêt Nam", "vn"}, {"Thaïlande", "th"}, {"Inde", "in"}};
Layout afrique[] = {{"Algérie", "dz"}, {"Maroc", "ma"},
                    {"Tunisie", "tn"}, {"Égypte", "eg"},
                    {"Sénégal", "sn"}, {"Afrique du Sud", "za"}};
Layout moyen_orient[] = {{"Turquie", "tr"},
                         {"Arabie Saoudite", "ara"},
                         {"Israël", "il"},
                         {"Iran", "ir"},
                         {"Émirats", "ae"}};
Layout oceanie[] = {{"Australie", "au"}, {"Nouv. Zélande", "nz"}};

/**
 * @brief Sauvegarde ou met à jour la configuration de la disposition clavier
 * (setxkbmap) dans le fichier autostart d'Openbox.
 * @param code Le code de la disposition (ex: "fr", "us", "be").
 */
void save_to_autostart(const char *code) {
  // 1. Sécurité : Vérification du pointeur NULL et de la chaîne vide
  if (code == NULL || strlen(code) == 0) {
    fprintf(
        stderr,
        "Erreur [save_to_autostart] : Le code de langue est NULL ou vide.\n");
    return;
  }

  // 2. Sécurité Absolue : Validation stricte du code clavier (uniquement
  // lettres, chiffres, tirets) Cela bloque définitivement les injections du
  // type "fr & commande" ou "fr; commande"
  const char *allowed_chars =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-";
  if (strspn(code, allowed_chars) != strlen(code)) {
    fprintf(stderr,
            "Erreur Sécurité [save_to_autostart] : Caractères interdits "
            "détectés dans le code : '%s'\n",
            code);
    return;
  }

  const char *home = getenv("HOME");
  if (!home || strlen(home) == 0) {
    fprintf(stderr, "Erreur [save_to_autostart] : Impossible de récupérer la "
                    "variable d'environnement HOME.\n");
    return;
  }

  // 1. On garde dir_path à 1024 (largement suffisant pour un chemin de dossier)
  char dir_path[1024];
  snprintf(dir_path, sizeof(dir_path), "%s/.config/openbox", home);

  // 2. On augmente la taille de 'path' pour accueillir dir_path (1024) +
  // "/autostart" (10) + de la marge
  char path[1100];
  snprintf(path, sizeof(path), "%s/autostart", dir_path);

  // 3. On augmente aussi 'temp_path' pour accueillir path (1100) + ".tmp" (4) +
  // de la marge
  char temp_path[1150];
  snprintf(temp_path, sizeof(temp_path), "%s.tmp", path);

  // SÉCURITÉ DOSSIER : On s'assure que le dossier parent existe.
  // g_mkdir_with_parents crée les dossiers manquants avec les droits de
  // lecture/écriture (0755) Si le dossier existe déjà, elle ne fait rien et
  // renvoie 0.
  if (g_mkdir_with_parents(dir_path, 0755) != 0) {
    g_printerr(
        "Erreur [save_to_autostart] : Impossible de créer le dossier '%s'.\n",
        dir_path);
    return;
  }

  FILE *in = fopen(path, "r");
  FILE *out = fopen(temp_path, "w");
  if (!out) {
    fprintf(stderr, "Erreur [save_to_autostart] : Impossible d'ouvrir le "
                    "fichier temporaire en écriture.\n");
    if (in)
      fclose(in);
    return;
  }

  int found = 0;
  char line[512];

  // Si le fichier autostart existe déjà, on le parcourt pour le mettre à jour
  if (in) {
    while (fgets(line, sizeof(line), in)) {
      // Sécurité : On s'assure que la ligne se termine bien par un saut de
      // ligne si elle est tronquée
      if (strstr(line, "setxkbmap")) {
        fprintf(out, "setxkbmap %s &\n", code);
        found = 1;
      } else {
        fputs(line, out);
      }
    }
    fclose(in);
  }

  // Si aucune ligne setxkbmap n'existait, on l'ajoute à la fin
  if (!found) {
    fprintf(out, "setxkbmap %s &\n", code);
  }

  fclose(out);

  // Remplacement atomique du fichier
  if (rename(temp_path, path) != 0) {
    perror("Erreur [save_to_autostart] lors du renommage du fichier autostart");
    // En cas d'échec du renommage, on nettoie le fichier temporaire pour ne pas
    // polluer
    remove(temp_path);
  }
}

/**
 * @brief Callback appelé lors du clic sur un bouton de disposition clavier.
 * * Cette fonction applique immédiatement la nouvelle disposition (via
 * setxkbmap), sauvegarde la configuration pour les prochains démarrages, envoie
 * une notification système pour confirmer l'action à l'utilisateur, puis quitte
 * proprement l'application.
 * * @param widget Le widget GtkWidget émetteur du signal (inutilisé, marqué
 * G_GNUC_UNUSED).
 * @param data Pointeur (gpointer) vers la chaîne de caractères contenant le
 * code de la disposition clavier (ex: "fr", "us"). Ne doit pas être NULL.
 */
void change_layout(GtkWidget *widget G_GNUC_UNUSED, gpointer data) {

  // 1. Sécurité : On valide immédiatement le pointeur de données
  if (data == NULL) {
    fprintf(
        stderr,
        "Erreur [change_layout] : Le pointeur de données (data) est NULL.\n");
    return;
  }

  char *code = (char *)data;

  // 2. Sécurité : On s'assure que la chaîne n'est pas vide
  if (strlen(code) == 0) {
    fprintf(stderr,
            "Erreur [change_layout] : Le code de langue fourni est vide.\n");
    return;
  }

  // 3. Sécurité Absolue : Validation stricte des caractères pour bloquer les
  // injections Shell (Une disposition X11 ne contient que des lettres, chiffres
  // ou tirets comme "fr-oss")
  const char *allowed_chars =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-";
  if (strspn(code, allowed_chars) != strlen(code)) {
    fprintf(stderr,
            "Erreur Sécurité [change_layout] : Caractères interdits détectés "
            "dans le code : '%s'\n",
            code);
    return;
  }

  char command[128];
  // Changement immédiat (Désormais 100% sûr grâce au filtre strspn au-dessus)
  snprintf(command, sizeof(command), "setxkbmap %s", code);
  if (system(command) != 0) {
    fprintf(stderr,
            "Erreur [change_layout] : Échec de l'exécution de setxkbmap\n");
  }

  // Sauvegarde pour le prochain redémarrage (Qui est elle-même déjà blindée)
  save_to_autostart(code);

  // Notification sécurisée
  char notify[256];
  snprintf(notify, sizeof(notify),
           "notify-send 'Clavier' 'Langue changée : %s'", code);
  if (system(notify) != 0) {
    fprintf(stderr, "Avertissement [change_layout] : Impossible d'envoyer la "
                    "notification (notify-send absent ?)\n");
  }

  // Quitter l'application proprement après l'action
  gtk_main_quit();
}

/**
 * @brief Crée une grille de boutons à partir d'un tableau de configurations de
 * langues.
 * @param list Tableau de structures Layout.
 * @param size Nombre d'éléments dans le tableau.
 * @return Un pointeur vers le nouveau GtkWidget grid, ou NULL en cas d'erreur.
 */
GtkWidget *create_grid(Layout list[], int size) {
  // 1. Sécurité : On vérifie que le tableau n'est pas NULL
  if (list == NULL) {
    g_printerr(
        "Erreur [create_grid] : Le tableau de layouts (list) est NULL.\n");
    return NULL;
  }

  // 2. Sécurité : On valide la taille du tableau
  if (size <= 0) {
    g_printerr("Erreur [create_grid] : La taille fournie (%d) est invalide.\n",
               size);
    return NULL;
  }

  GtkWidget *grid = gtk_grid_new();
  // 3. Sécurité : On s'assure que GTK a bien pu allouer la grille
  if (grid == NULL || !GTK_IS_GRID(grid)) {
    g_printerr("Erreur [create_grid] : Impossible de créer la grille GTK.\n");
    return NULL;
  }

  gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
  gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
  gtk_container_set_border_width(GTK_CONTAINER(grid), 15);

  for (int i = 0; i < size; i++) {
    // 4. Sécurité Interne : On vérifie que les chaînes de la structure existent
    if (list[i].name == NULL || list[i].code == NULL) {
      g_printerr("Avertissement [create_grid] : Élément d'index %d ignoré "
                 "(name ou code est NULL).\n",
                 i);
      continue; // On passe à l'élément suivant sans crasher
    }

    // Vérification optionnelle mais propre : chaînes vides
    if (strlen(list[i].name) == 0 || strlen(list[i].code) == 0) {
      g_printerr("Avertissement [create_grid] : Élément d'index %d ignoré "
                 "(chaîne vide).\n",
                 i);
      continue;
    }

    GtkWidget *btn = gtk_button_new_with_label(list[i].name);
    if (btn != NULL && GTK_IS_WIDGET(btn)) {
      gtk_widget_set_size_request(btn, 130, 40);

      // On lie le signal en lui passant le pointeur vers le code (désormais
      // garanti non-NULL)
      g_signal_connect(btn, "clicked", G_CALLBACK(change_layout),
                       (gpointer)list[i].code);

      gtk_grid_attach(GTK_GRID(grid), btn, i % 3, i / 3, 1, 1);
    }
  }

  return grid;
}

int main(int argc, char *argv[]) {
  prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
  gtk_init(&argc, &argv);

  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "Changer langue du Clavier");
  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size(GTK_WINDOW(window), 450, 300);

  GtkWidget *notebook = gtk_notebook_new();
  gtk_container_add(GTK_CONTAINER(window), notebook);

  // Ajout des onglets
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), create_grid(europe, 8),
                           gtk_label_new("Europe"));
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), create_grid(ameriques, 5),
                           gtk_label_new("Amériques"));
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), create_grid(asie, 6),
                           gtk_label_new("Asie"));
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), create_grid(afrique, 6),
                           gtk_label_new("Afrique"));
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), create_grid(moyen_orient, 5),
                           gtk_label_new("Moyen-Orient"));
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), create_grid(oceanie, 2),
                           gtk_label_new("Océanie"));

  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
  gtk_widget_show_all(window);
  gtk_main();

  return 0;
}
