#include <assert.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>

#define PROGRAMME_NAME "ram-monitor"

typedef struct {
  char name[128];
  double memory;
} ProcessInfo;

GtkTextBuffer *buffer;

int compare_proc(const void *a, const void *b) {
  assert(a != NULL && b != NULL);
  const ProcessInfo *procA = (const ProcessInfo *)a;
  const ProcessInfo *procB = (const ProcessInfo *)b;
  if (procB->memory > procA->memory)
    return 1;
  if (procB->memory < procA->memory)
    return -1;
  return 0;
}

// Nouvelle fonction pour obtenir le VRAI total de RAM consommée par le système
double get_system_real_used_ram(void) {
  FILE *fp = fopen("/proc/meminfo", "r");
  if (!fp)
    return 0.0;

  long total = 0, free_mem = 0, buffers = 0, cached = 0, reclaimable = 0;
  char label[64];
  long value;

  while (fscanf(fp, "%63s %ld kB", label, &value) == 2) {
    if (strcmp(label, "MemTotal:") == 0)
      total = value;
    else if (strcmp(label, "MemFree:") == 0)
      free_mem = value;
    else if (strcmp(label, "Buffers:") == 0)
      buffers = value;
    else if (strcmp(label, "Cached:") == 0)
      cached = value;
    else if (strcmp(label, "SReclaimable:") == 0)
      reclaimable = value;
  }
  fclose(fp);

  // Formule officielle du noyau Linux (et de htop) pour la RAM REELLEMENT
  // utilisée
  long used_kb = total - free_mem - buffers - cached - reclaimable;
  return (double)used_kb / 1024.0; // Conversion en Mo
}

char *get_sorted_ram_report() {
  ProcessInfo procs[1024];
  int count = 0;
  int monitor_index = -1;

  FILE *fp = popen("ps -eo rss,comm --no-headers", "r");
  if (!fp)
    return g_strdup("Erreur d'exécution de ps");

  char line[256];
  while (fgets(line, sizeof(line), fp) && count < 1024) {
    char name_tmp[128];
    long rss;

    if (sscanf(line, "%ld %127[^\n]", &rss, name_tmp) == 2) {
      if (strcmp(name_tmp, "ps") == 0)
        continue;

      if (strcmp(name_tmp, PROGRAMME_NAME) == 0) {
        double current_mem = (double)rss / 1024.0;
        if (monitor_index == -1) {
          g_strlcpy(procs[count].name, name_tmp, sizeof(procs[count].name));
          procs[count].memory = current_mem;
          monitor_index = count;
          count++;
        } else {
          procs[monitor_index].memory += current_mem;
        }
        continue;
      }

      if (rss > 500) {
        g_strlcpy(procs[count].name, name_tmp, sizeof(procs[count].name));
        procs[count].memory = (double)rss / 1024.0;
        count++;
      }
    }
  }
  pclose(fp);

  if (count == 0) {
    return g_strdup("Aucun processus actif trouvé.");
  }

  qsort(procs, count, sizeof(ProcessInfo), compare_proc);

  // On récupère la VRAIE valeur du système (ex: 3.8 Go)
  double real_system_total = get_system_real_used_ram();

  GString *report = g_string_new(NULL);
  g_string_append_printf(report, "  %-25s %s\n", "APPLICATION", "UTILISATION");
  g_string_append(report, "  ==========================================\n\n");

  for (int i = 0; i < count; i++) {
    g_string_append_printf(report, "  %-25s %8.2f Mo\n", procs[i].name,
                           procs[i].memory);
  }

  g_string_append_printf(report,
                         "\n  ==========================================\n"
                         "  VRAI TOTAL RAM : %.2f Mo (%.2f Go)\n",
                         real_system_total, real_system_total / 1024.0);

  return g_string_free(report, FALSE);
}

void on_refresh(GtkWidget *widget G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED) {
  char *report = get_sorted_ram_report();
  gtk_text_buffer_set_text(buffer, report, -1);
  g_free(report);
}

void on_save(GtkWidget *widget G_GNUC_UNUSED, gpointer window) {
  g_return_if_fail(GTK_IS_WINDOW(window));
  GtkWidget *dialog = gtk_file_chooser_dialog_new(
      "Enregistrer le rapport", GTK_WINDOW(window),
      GTK_FILE_CHOOSER_ACTION_SAVE, "_Annuler", GTK_RESPONSE_CANCEL,
      "_Sauvegarder", GTK_RESPONSE_ACCEPT, NULL);

  gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "conso_ram.txt");
  gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog),
                                                 TRUE);

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
    char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    g_file_set_contents(filename, text, -1, NULL);
    g_free(filename);
    g_free(text);
  }
  gtk_widget_destroy(dialog);
}

void create_monitor_window(void) {
  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "ArchMonitor RAM");
  gtk_window_set_default_size(GTK_WINDOW(window), 500, 650);
  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 15);
  gtk_container_add(GTK_CONTAINER(window), vbox);

  GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
  GtkWidget *text_view = gtk_text_view_new();
  gtk_text_view_set_monospace(GTK_TEXT_VIEW(text_view), TRUE);
  gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
  buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));

  gtk_container_add(GTK_CONTAINER(scrolled), text_view);
  gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);

  GtkWidget *bbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
  gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

  GtkWidget *btn_ref = gtk_button_new_with_label("🔄 Actualiser");
  g_signal_connect(btn_ref, "clicked", G_CALLBACK(on_refresh), NULL);
  gtk_container_add(GTK_CONTAINER(bbox), btn_ref);

  GtkWidget *btn_sav = gtk_button_new_with_label("💾 Enregistrer");
  g_signal_connect(btn_sav, "clicked", G_CALLBACK(on_save), window);
  gtk_container_add(GTK_CONTAINER(bbox), btn_sav);

  on_refresh(NULL, NULL);
  gtk_widget_show_all(window);
}

int main(int argc, char *argv[]) {
  prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
  gtk_init(&argc, &argv);
  create_monitor_window();
  gtk_main();
  return 0;
}