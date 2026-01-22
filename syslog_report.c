#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/prctl.h>

// --- NOM PROGRAMME ---
#define PROGRAMME_NAME "syslog_report"

#define TEMP_FILE "/tmp/syslog_report_alban.txt"

// --- STYLES ET COULEURS ---
void insert_line_with_style(GtkTextBuffer *buffer, const char *line) {
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(buffer, &iter);

    if (strncmp(line, "=== ", 4) == 0) {
        gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, line, -1, "title_style", NULL);
    } 
    else if (strcasestr(line, "error") || strcasestr(line, "segfault") || strcasestr(line, "fail") || strcasestr(line, "critical")) {
        gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, line, -1, "error_style", NULL);
    } 
    else {
        gtk_text_buffer_insert(buffer, &iter, line, -1);
    }
}

// --- ACTION : COPIER ---
void on_copy_clicked(GtkWidget *widget, gpointer data) {
    GtkTextBuffer *buffer = GTK_TEXT_BUFFER(data);
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), text, -1);
    g_free(text);
    gtk_button_set_label(GTK_BUTTON(widget), "✅ Copié !");
}

// --- ACTION : ENREGISTRER SOUS ---
// Correction appliquée ici : G_GNUC_UNUSED pour le paramètre widget
void on_save_clicked(GtkWidget *widget G_GNUC_UNUSED, gpointer data) {
    GtkWindow *parent_window = GTK_WINDOW(data);
    GtkWidget *dialog;
    
    // Génération du nom de fichier : 2026-01-05_14h25_Journaux_System.txt
    time_t t = time(NULL);
    char default_name[128];
    strftime(default_name, sizeof(default_name), "%Y-%m-%d_%Hh%M_Journaux_System.txt", localtime(&t));

    dialog = gtk_file_chooser_dialog_new("Enregistrer le rapport", parent_window,
                                         GTK_FILE_CHOOSER_ACTION_SAVE,
                                         "_Annuler", GTK_RESPONSE_CANCEL,
                                         "_Enregistrer", GTK_RESPONSE_ACCEPT, NULL);
    
    GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
    gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);
    gtk_file_chooser_set_current_name(chooser, default_name);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(chooser);
        GtkTextView *text_view = g_object_get_data(G_OBJECT(parent_window), "view");
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(text_view);
        GtkTextIter start, end;
        gtk_text_buffer_get_bounds(buffer, &start, &end);
        char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

        // Sauvegarde physique du fichier
        g_file_set_contents(filename, text, -1, NULL);
        
        g_free(text);
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

// --- COLLECTE DES DONNÉES (ROOT) ---
void generate_full_report() {
    char full_cmd[14000]; // Augmenté légèrement pour la sécurité
    snprintf(full_cmd, sizeof(full_cmd),
        "pkexec sh -c \""
        "echo '=== 1. RÉSUMÉ STATISTIQUE ===' > %s && "
        "{ journalctl -p 3 --since '1 month ago' --no-pager | awk '{print $5}' | sed 's/\\\\\\[.*\\\\\\]//g; s/://g' | sort | uniq -c | sort -nr | head -n 15 ; } >> %s 2>/dev/null && "
        "echo '\n=== 2. CONDENSÉ MULTICOUCHE ===' >> %s && "
        "journalctl -p 3 -b --no-pager >> %s 2>/dev/null && "
        "echo '\n=== 3. KERNEL (Matériel) ===' >> %s && "
        "journalctl -k -p 3 -b --no-pager >> %s 2>/dev/null && "
        "echo '\n=== 4. SEGFAULTS (Dmesg) ===' >> %s && "
        "{ dmesg | grep -i 'segfault' || echo 'Aucun segfault détecté' ; } >> %s 2>/dev/null && "
        "echo '\n=== 5. SYSTEM SERVICES ===' >> %s && "
        "journalctl -u '*' -p 3 -n 20 --no-pager >> %s 2>/dev/null && "
        "echo '\n=== 6. AUTH (Échecs) ===' >> %s && "
        "(grep 'Failure' /var/log/auth.log 2>/dev/null || echo 'Aucun accès auth.log') >> %s 2>/dev/null && "
        "echo '\n=== 7. APPLICATIFS (Coredump) ===' >> %s && "
        "coredumpctl list --no-legend 2>/dev/null | tail -n 15 >> %s 2>/dev/null && "
        "echo '\n=== 8. MISES À JOUR (Pacman) ===' >> %s && "
        "tail -n 20 /var/log/pacman.log >> %s 2>/dev/null && "
        "echo '\n=== 9. IMPRIMANTES (CUPS) ===' >> %s && "
        "journalctl -u cups -p 3 -n 10 --no-pager >> %s 2>/dev/null\"",
        TEMP_FILE, TEMP_FILE, TEMP_FILE, TEMP_FILE, TEMP_FILE, 
        TEMP_FILE, TEMP_FILE, TEMP_FILE, TEMP_FILE, TEMP_FILE, 
        TEMP_FILE, TEMP_FILE, TEMP_FILE, TEMP_FILE, TEMP_FILE, 
        TEMP_FILE, TEMP_FILE, TEMP_FILE);

    if (system(full_cmd) != 0) {
        FILE *f = fopen(TEMP_FILE, "w");
        if (f) { fprintf(f, "=== ERREUR ===\nL'authentification a échoué ou l'action a été annulée.\n"); fclose(f); }
    }
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
    g_signal_connect_swapped(btn_vacuum, "clicked", G_CALLBACK(system), "pkexec journalctl --vacuum-time=1s");
    gtk_box_pack_start(GTK_BOX(hbox), btn_vacuum, TRUE, TRUE, 0);

    gtk_widget_show_all(window);
    gtk_main();

    remove(TEMP_FILE);
    return 0;
}
