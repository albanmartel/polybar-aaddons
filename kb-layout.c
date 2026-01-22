#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/prctl.h>

// --- NOM PROGRAMME ---
#define PROGRAMME_NAME "kb-layout"

typedef struct { char *name; char *code; } Layout;

// Définition des zones géographiques
Layout europe[] = { {"France", "fr"}, {"UK", "gb"}, {"Italie", "it"}, {"Allemagne", "de"}, {"Espagne", "es"}, {"Belgique", "be"}, {"Suisse", "ch(fr)"}, {"Portugal", "pt"} };
Layout ameriques[] = { {"USA", "us"}, {"Canada", "ca"}, {"Brésil", "br"}, {"Mexique", "latam"}, {"Argentine", "ar"} };
Layout asie[] = { {"Japon", "jp"}, {"Chine", "cn"}, {"Corée", "kr"}, {"Viêt Nam", "vn"}, {"Thaïlande", "th"}, {"Inde", "in"} };
Layout afrique[] = { {"Algérie", "dz"}, {"Maroc", "ma"}, {"Tunisie", "tn"}, {"Égypte", "eg"}, {"Sénégal", "sn"}, {"Afrique du Sud", "za"} };
Layout moyen_orient[] = { {"Turquie", "tr"}, {"Arabie Saoudite", "ara"}, {"Israël", "il"}, {"Iran", "ir"}, {"Émirats", "ae"} };
Layout oceanie[] = { {"Australie", "au"}, {"Nouv. Zélande", "nz"} };

void save_to_autostart(const char *code) {
    char path[1024], temp_path[1100]; // Augmentation de la taille pour éviter la troncature
    const char *home = getenv("HOME");
    if (!home) return;

    snprintf(path, sizeof(path), "%s/.config/openbox/autostart", home);
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", path);

    FILE *in = fopen(path, "r");
    FILE *out = fopen(temp_path, "w");
    if (!out) {
        if (in) fclose(in);
        return;
    }

    int found = 0;
    char line[512];
    if (in) {
        while (fgets(line, sizeof(line), in)) {
            if (strstr(line, "setxkbmap")) {
                fprintf(out, "setxkbmap %s &\n", code);
                found = 1;
            } else {
                fputs(line, out);
            }
        }
        fclose(in);
    }
    
    if (!found) {
        fprintf(out, "setxkbmap %s &\n", code);
    }
    
    fclose(out);
    if (rename(temp_path, path) != 0) {
        perror("Erreur lors du renommage du fichier autostart");
    }
}

// Correction du paramètre inutilisé avec G_GNUC_UNUSED
void change_layout(GtkWidget *widget G_GNUC_UNUSED, gpointer data) {
    char *code = (char *)data;
    char command[128];
    
    // Changement immédiat
    snprintf(command, sizeof(command), "setxkbmap %s", code);
    if (system(command) != 0) {
        fprintf(stderr, "Erreur lors de l'exécution de setxkbmap\n");
    }

    // Sauvegarde pour le prochain redémarrage
    save_to_autostart(code);
    
    // Notification
    char notify[256];
    snprintf(notify, sizeof(notify), "notify-send 'Clavier' 'Langue changée : %s'", code);
    if (system(notify) != 0) {
        // Optionnel : gérer l'erreur si notify-send échoue
    }
    
    gtk_main_quit();
}

GtkWidget* create_grid(Layout list[], int size) {
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 15);

    for (int i = 0; i < size; i++) {
        GtkWidget *btn = gtk_button_new_with_label(list[i].name);
        gtk_widget_set_size_request(btn, 130, 40);
        g_signal_connect(btn, "clicked", G_CALLBACK(change_layout), list[i].code);
        gtk_grid_attach(GTK_GRID(grid), btn, i % 3, i / 3, 1, 1);
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
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), create_grid(europe, 8), gtk_label_new("Europe"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), create_grid(ameriques, 5), gtk_label_new("Amériques"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), create_grid(asie, 6), gtk_label_new("Asie"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), create_grid(afrique, 6), gtk_label_new("Afrique"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), create_grid(moyen_orient, 5), gtk_label_new("Moyen-Orient"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), create_grid(oceanie, 2), gtk_label_new("Océanie"));

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_widget_show_all(window);
    gtk_main();
    
    return 0;
}
