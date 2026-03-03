#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/prctl.h>

// --- NOM PROGRAMME ---
#define PROGRAMME_NAME "syslog_report"

#define TEMP_FILE "/tmp/syslog_report_alban.txt"

// --- PROTOTYPES (Indispensable pour la compilation) ---
void generate_full_report();
void hard_clean_logs(GtkWidget *widget, gpointer data);
void on_vacuum_clicked(GtkWidget *widget, gpointer data);

// --- STYLES ET COULEURS (Version sécurisée) ---
void insert_line_with_style(GtkTextBuffer *buffer, const char *line) {
    // 1. Vérification des entrées (Pointeurs nuls)
    if (!GTK_IS_TEXT_BUFFER(buffer) || line == NULL) {
        return; 
    }

    // 2. Vérification de la longueur de la ligne
    // Si la ligne est vide (juste un \0), on n'insère rien ou juste un saut de ligne
    size_t len = strlen(line);
    if (len == 0) return;

    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(buffer, &iter);

    // 3. Vérification de l'existence des tags avant utilisation
    GtkTextTagTable *table = gtk_text_buffer_get_tag_table(buffer);
    gboolean has_title = gtk_text_tag_table_lookup(table, "title_style") != NULL;
    gboolean has_error = gtk_text_tag_table_lookup(table, "error_style") != NULL;

    // 4. Logique d'insertion avec garde-fous
    if (len >= 4 && strncmp(line, "=== ", 4) == 0) {
        if (has_title) {
            gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, line, -1, "title_style", NULL);
        } else {
            gtk_text_buffer_insert(buffer, &iter, line, -1); // Fallback sans style
        }
    } 
    else if (strcasestr(line, "error") || strcasestr(line, "segfault") || 
             strcasestr(line, "fail")  || strcasestr(line, "critical")) {
        if (has_error) {
            gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, line, -1, "error_style", NULL);
        } else {
            gtk_text_buffer_insert(buffer, &iter, line, -1);
        }
    } 
    else {
        // Insertion standard pour le reste
        gtk_text_buffer_insert(buffer, &iter, line, -1);
    }
}

// --- ACTION : COPIER (Version sécurisée) ---
void on_copy_clicked(GtkWidget *widget, gpointer data) {
    // 1. Vérification des entrées (Safety first)
    // On s'assure que widget et data ne sont pas NULL et que data est bien un GtkTextBuffer
    if (!GTK_IS_WIDGET(widget) || !GTK_IS_TEXT_BUFFER(data)) {
        fprintf(stderr, "Erreur : on_copy_clicked a reçu des données invalides.\n");
        return;
    }

    GtkTextBuffer *buffer = GTK_TEXT_BUFFER(data);
    GtkTextIter start, end;
    
    // 2. Vérifier si le buffer contient du texte
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    if (gtk_text_iter_equal(&start, &end)) {
        // Le buffer est vide, rien à copier
        gtk_button_set_label(GTK_BUTTON(widget), "⚠️ Rien à copier");
        return;
    }

    // 3. Récupération sécurisée du texte
    char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    
    if (text != NULL) {
        // 4. Accès au presse-papier avec vérification
        GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
        if (clipboard != NULL) {
            gtk_clipboard_set_text(clipboard, text, -1);
            gtk_button_set_label(GTK_BUTTON(widget), "✅ Copié !");
        } else {
            gtk_button_set_label(GTK_BUTTON(widget), "❌ Erreur Clipboard");
        }
        
        // 5. Toujours libérer la mémoire allouée par GTK
        g_free(text);
    }
}

// --- ACTION : ENREGISTRER SOUS (Version sécurisée) ---
void on_save_clicked(GtkWidget *widget G_GNUC_UNUSED, gpointer data) {
    // 1. Vérification de la fenêtre parente
    if (!GTK_IS_WINDOW(data)) {
        fprintf(stderr, "Erreur : La donnée passée à on_save_clicked n'est pas une GtkWindow.\n");
        return;
    }
    GtkWindow *parent_window = GTK_WINDOW(data);
    
    // 2. Récupération sécurisée du GtkTextView et de son Buffer
    GtkTextView *text_view = GTK_TEXT_VIEW(g_object_get_data(G_OBJECT(parent_window), "view"));
    if (!text_view || !GTK_IS_TEXT_VIEW(text_view)) {
        fprintf(stderr, "Erreur : Impossible de récupérer le TextView pour l'enregistrement.\n");
        return;
    }
    
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(text_view);
    if (!buffer) return;

    // 3. Vérifier si le buffer est vide
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    if (gtk_text_iter_equal(&start, &end)) {
        return;
    }

    // 4. Préparation du nom par défaut
    time_t t = time(NULL);
    
    // CORRECTION : On déclare un tableau de 4096 caractères (le parking)
    char default_name[4096]; 

    // CORRECTION : On utilise le tableau pour stocker la chaîne vide
    memset(default_name, 0, sizeof(default_name));

    struct tm *info_temps = localtime(&t);
    
    // CORRECTION : strftime va maintenant remplir correctement notre tableau
    if (t == (time_t)-1 || info_temps == NULL || 
        strftime(default_name, sizeof(default_name), "%Y-%m-%d_%Hh%M_Journaux_System.txt", info_temps) == 0) {
        strncpy(default_name, "Rapport_Systeme.txt", sizeof(default_name));
    }

    // 5. Création et configuration du dialogue
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Enregistrer le rapport", parent_window,
                                         GTK_FILE_CHOOSER_ACTION_SAVE,
                                         "_Annuler", GTK_RESPONSE_CANCEL,
                                         "_Enregistrer", GTK_RESPONSE_ACCEPT, NULL);
    
    GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
    gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);
    
    // Maintenant default_name contient bien une phrase, GTK peut l'utiliser
    gtk_file_chooser_set_current_name(chooser, default_name);

    // 6. Exécution du dialogue
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(chooser);
        
        if (filename != NULL) {
            char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
            
            if (text != NULL) {
                GError *error = NULL;
                if (!g_file_set_contents(filename, text, -1, &error)) {
                    fprintf(stderr, "Erreur d'écriture : %s\n", error->message);
                    g_error_free(error);
                }
                g_free(text);
            }
            g_free(filename);
        }
    }
    
    gtk_widget_destroy(dialog);
}


// --- COLLECTE DES DONNÉES (ROOT) (Version sécurisée) ---
void generate_full_report() {
    // 1. Vérification de la constante TEMP_FILE
    if (TEMP_FILE == NULL || strlen(TEMP_FILE) == 0) {
        fprintf(stderr, "Erreur : Chemin du fichier temporaire invalide.\n");
        return;
    }

    // 2. Utilisation de heap (malloc) au lieu de la pile (stack) pour une chaîne si large
    // 16384 octets offrent une marge confortable pour la commande
    size_t cmd_size = 16384;
    char *full_cmd = malloc(cmd_size);
    if (full_cmd == NULL) {
        fprintf(stderr, "Erreur : Échec d'allocation mémoire pour la commande.\n");
        return;
    }

    // 3. Construction sécurisée avec snprintf (vérifie si on dépasse cmd_size)
    int written = snprintf(full_cmd, cmd_size,
        "pkexec sh -c \""
        "echo '=== 1. RÉSUMÉ STATISTIQUE ===' > %1$s && "
        "{ journalctl -p 3 --since '1 month ago' --no-pager | awk '{print $5}' | sed 's/\\\\\\[.*\\\\\\]//g; s/://g' | sort | uniq -c | sort -nr | head -n 15 ; } >> %1$s 2>/dev/null && "
        "echo '\n=== 2. CONDENSÉ MULTICOUCHE ===' >> %1$s && "
        "journalctl -p 3 -b --no-pager >> %1$s 2>/dev/null && "
        "echo '\n=== 3. KERNEL (Matériel) ===' >> %1$s && "
        "journalctl -k -p 3 -b --no-pager >> %1$s 2>/dev/null && "
        "echo '\n=== 4. SEGFAULTS (Dmesg) ===' >> %1$s && "
        "{ dmesg | grep -i 'segfault' || echo 'Aucun segfault détecté' ; } >> %1$s 2>/dev/null && "
        "echo '\n=== 5. SYSTEM SERVICES ===' >> %1$s && "
        "journalctl -u '*' -p 3 -n 20 --no-pager >> %1$s 2>/dev/null && "
        "echo '\n=== 6. AUTH (Échecs) ===' >> %1$s && "
        "(grep 'Failure' /var/log/auth.log 2>/dev/null || echo 'Aucun accès auth.log') >> %1$s 2>/dev/null && "
        "echo '\n=== 7. APPLICATIFS (Coredump) ===' >> %1$s && "
        "coredumpctl list --no-legend 2>/dev/null | tail -n 15 >> %1$s 2>/dev/null && "
        "echo '\n=== 8. MISES À JOUR (Pacman) ===' >> %1$s && "
        "tail -n 20 /var/log/pacman.log >> %1$s 2>/dev/null && "
        "echo '\n=== 9. IMPRIMANTES (CUPS) ===' >> %1$s && "
        "journalctl -u cups -p 3 -n 10 --no-pager >> %1$s 2>/dev/null\"",
        TEMP_FILE);

    // Vérification si la commande a été tronquée
    if (written < 0 || (size_t)written >= cmd_size) {
        fprintf(stderr, "Erreur : La commande de rapport est trop longue pour le tampon.\n");
        free(full_cmd);
        return;
    }

    // 4. Exécution et gestion propre du retour
    int result = system(full_cmd);
    
    if (result != 0) {
        FILE *f = fopen(TEMP_FILE, "w");
        if (f) {
            fprintf(f, "=== ERREUR ===\nL'authentification a échoué (pkexec), l'action a été annulée ou une commande a échoué.\n");
            fclose(f);
        }
    }

    // 5. Libération de la mémoire
    free(full_cmd);
}

// -- NETTOYAGE DES LOGS (ROOT) --
void hard_clean_logs(GtkWidget *widget, gpointer data) {
    // 1. SÉCURITÉ : On vérifie que si un widget est passé, c'est bien un objet GTK valide.
    // Si la fonction est appelée par un bouton, 'widget' doit être un GtkWidget.
    if (widget != NULL && !GTK_IS_WIDGET(widget)) {
        fprintf(stderr, "Erreur critique : hard_clean_logs appelé avec un widget invalide.\n");
        return;
    }

    // 2. LOGIQUE SYSTÈME
    const char *command = "pkexec sh -c 'rm -rf /var/log/journal/* && systemctl restart systemd-journald'";    
    
    printf("Lancement du nettoyage radical...\n");
    
    // On capture le code de retour pour savoir si l'utilisateur a annulé le mot de passe
    int status = system(command);
    
    if (status == 0) {
        printf("Nettoyage réussi.\n");
    } else {
        // Code non-zéro : souvent l'utilisateur a cliqué sur "Annuler" dans pkexec
        fprintf(stderr, "Le nettoyage a été annulé ou a échoué (code %d).\n", status);
    }

    // 3. MISE À JOUR DU RAPPORT
    // On génère le nouveau rapport sur le disque
    generate_full_report();

    // 4. MISE À JOUR DE L'INTERFACE (si data contient le buffer)
    if (data != NULL && GTK_IS_TEXT_BUFFER(data)) {
        GtkTextBuffer *buffer = GTK_TEXT_BUFFER(data);
        // On vide la fenêtre
        gtk_text_buffer_set_text(buffer, "", 0);
        
        // On recharge le fichier temporaire (ton code de lecture habituel)
        FILE *fp = fopen(TEMP_FILE, "r");
        if (fp) {
            char line[4096];
            while (fgets(line, sizeof(line), fp)) {
                insert_line_with_style(buffer, line);
            }
            fclose(fp);
        }
    }
}

// --- MISE À JOUR DE L'AFFICHAGE (Version sécurisée) ---
void refresh_display(GtkTextBuffer *buffer) {
    // 1. Vérification de l'entrée (Safety first)
    if (!buffer || !GTK_IS_TEXT_BUFFER(buffer)) {
        fprintf(stderr, "Erreur : refresh_display a reçu un buffer invalide.\n");
        return;
    }

    // 2. Effacer proprement le contenu actuel
    // On utilise 0 ou -1 pour indiquer la fin de la chaîne
    gtk_text_buffer_set_text(buffer, "", 0);

    // 3. Vérification du chemin du fichier temporaire
    if (TEMP_FILE == NULL) return;

    // 4. Tentative d'ouverture du fichier
    FILE *fp = fopen(TEMP_FILE, "r");
    if (fp == NULL) {
        // Si le fichier n'existe pas, on insère un message d'erreur dans le buffer
        GtkTextIter iter;
        gtk_text_buffer_get_end_iter(buffer, &iter);
        gtk_text_buffer_insert(buffer, &iter, "⚠️ Erreur : Impossible de lire le rapport temporaire.\n", -1);
        return;
    }

    // 5. Lecture ligne par ligne avec un tampon sécurisé
    char line[4096]; // Correction : un tableau, pas un simple char
    while (fgets(line, sizeof(line), fp)) {
        // On vérifie que la ligne n'est pas corrompue avant l'insertion
        if (strlen(line) > 0) {
            insert_line_with_style(buffer, line);
        }
    }

    // 6. Fermeture sécurisée
    fclose(fp);

    // 7. Petit bonus : scroll automatique vers le haut pour le nouveau rapport
    GtkTextIter start;
    gtk_text_buffer_get_start_iter(buffer, &start);
    // On pourrait forcer le défilement ici si nécessaire
}

// --- ACTION : NETTOYAGE (Intermédiaire GTK) ---
void on_vacuum_clicked(GtkWidget *widget, gpointer data) {
    // 1. "data est en fait notre buffer"
    GtkTextBuffer *buffer = GTK_TEXT_BUFFER(data);

    gtk_button_set_label(GTK_BUTTON(widget), "⏳ Nettoyage...");
    
    // On passe widget et data pour satisfaire la signature de hard_clean_logs
    hard_clean_logs(widget, data);

    // MISE À JOUR DE L'AFFICHAGE
    refresh_display(buffer);

    gtk_button_set_label(GTK_BUTTON(widget), "✅ Journal vide");
}


int main(int argc, char *argv[]) {
    prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
    generate_full_report();

    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Rapporteur Système Arch");
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 800);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *text_view = gtk_text_view_new();
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(text_view), TRUE); // Optionnel : pour un look "log"
    gtk_container_add(GTK_CONTAINER(scrolled), text_view);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);

    // Stockage du pointeur vers la vue pour la fonction Save
    g_object_set_data(G_OBJECT(window), "view", text_view);

    gtk_text_buffer_create_tag(buffer, "error_style", "foreground", "red", "weight", PANGO_WEIGHT_BOLD, NULL);
    gtk_text_buffer_create_tag(buffer, "title_style", "foreground", "#0072ff", "weight", PANGO_WEIGHT_BOLD, "scale", 1.2, NULL);

    FILE *fp = fopen(TEMP_FILE, "r");
    if (fp) {
        char line[4096];
        while (fgets(line, sizeof(line), fp)) insert_line_with_style(buffer, line);
        fclose(fp);
    }

    // --- BARRE DE BOUTONS ---
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_set_homogeneous(GTK_BOX(hbox), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

    GtkWidget *btn_copy = gtk_button_new_with_label("📋 Copier");
    g_signal_connect(btn_copy, "clicked", G_CALLBACK(on_copy_clicked), buffer);
    gtk_box_pack_start(GTK_BOX(hbox), btn_copy, TRUE, TRUE, 0);

    GtkWidget *btn_save = gtk_button_new_with_label("💾 Enregistrer");
    g_signal_connect(btn_save, "clicked", G_CALLBACK(on_save_clicked), window);
    gtk_box_pack_start(GTK_BOX(hbox), btn_save, TRUE, TRUE, 0);

    GtkWidget *btn_vacuum = gtk_button_new_with_label("🧹 Nettoyer");
    g_signal_connect(btn_vacuum, "clicked", G_CALLBACK(on_vacuum_clicked), buffer);
    gtk_box_pack_start(GTK_BOX(hbox), btn_vacuum, TRUE, TRUE, 0);

    gtk_widget_show_all(window);
    gtk_main();

    remove(TEMP_FILE);
    return 0;
}
