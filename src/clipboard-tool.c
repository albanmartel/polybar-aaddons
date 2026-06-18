#include <fcntl.h> // Nécessaire pour O_RDWR, O_CREAT
#include <libclipboard.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <unistd.h>

#define PROGRAMME_NAME "clipboard_tool"
#define HIST_LIMIT 50

void append_to_history(const char *text, const char *hist_path) {
  if (text == NULL || hist_path == NULL) {
    fprintf(stderr,
            "Erreur : Pointeurs d'entrée NULL dans append_to_history\n");
    return;
  }
  if (strlen(text) == 0 || strlen(hist_path) == 0) {
    return;
  }

  char *clean_text = malloc(strlen(text) + 1);
  if (!clean_text)
    return;

  strcpy(clean_text, text);
  for (int i = 0; clean_text[i]; i++) {
    if (clean_text[i] == '\n' || clean_text[i] == '\r') {
      clean_text[i] = ' ';
    }
  }

  FILE *f = fopen(hist_path, "r");
  if (f) {
    char first_line[4096] = {0};
    if (fgets(first_line, sizeof(first_line), f)) {
      first_line[strcspn(first_line, "\n")] = 0;
      if (strcmp(first_line, clean_text) == 0) {
        fclose(f);
        free(clean_text);
        return;
      }
    }
    fclose(f);
  }

  char *lines[HIST_LIMIT] = {0};
  int line_count = 0;

  f = fopen(hist_path, "r");
  if (f) {
    char buf[4096];
    while (fgets(buf, sizeof(buf), f) && line_count < (HIST_LIMIT - 1)) {
      char *allocated_line = strdup(buf);
      if (allocated_line) {
        lines[line_count++] = allocated_line;
      }
    }
    fclose(f);
  }

  f = fopen(hist_path, "w");
  if (f) {
    fprintf(f, "%s\n", clean_text);
    for (int i = 0; i < line_count; i++) {
      fprintf(f, "%s", lines[i]);
      free(lines[i]);
    }
    fclose(f);
  }
  free(clean_text);
}

int main() {
  // 1. Définir le nom du processus système
  prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);

  // 2. Récupérer le dossier temporaire pour le fichier de verrouillage (Lock)
  char lock_path[PATH_MAX];
  const char *runtime_dir = getenv("XDG_RUNTIME_DIR");

  if (runtime_dir && strlen(runtime_dir) > 0) {
    snprintf(lock_path, sizeof(lock_path), "%s/clipboard_tool.lock",
             runtime_dir);
  } else {
    snprintf(lock_path, sizeof(lock_path), "/tmp/clipboard_tool.lock");
  }

  // 3. --- MÉCANISME SINGLETON ---
  // On ouvre (ou crée) le fichier de verrou
  int lock_fd = open(lock_path, O_RDWR | O_CREAT, 0666);
  if (lock_fd < 0) {
    fprintf(stderr, "Erreur : Impossible de créer le fichier de verrou.\n");
    return 1;
  }

  // On tente de poser un verrou exclusif (F_TLOCK).
  // Si lockf renvoie une valeur négative, c'est qu'un autre clipboard_tool
  // tourne déjà !
  if (lockf(lock_fd, F_TLOCK, 0) < 0) {
    // Une instance tourne déjà : on quitte immédiatement en silence.
    // Polybar lancera son echo "󱘟" sans accumuler un processus fantôme.
    close(lock_fd);
    return 0;
  }
  // ------------------------------

  // 4. Initialisation de libclipboard (Une seule fois !)
  clipboard_c *cb = clipboard_new(NULL);
  if (!cb) {
    fprintf(stderr, "Impossible d'initialiser libclipboard (X11 saturé ?)\n");
    close(lock_fd);
    return 1;
  }

  // Configuration du chemin historique
  char hist_path[PATH_MAX];
  if (runtime_dir && strlen(runtime_dir) > 0) {
    snprintf(hist_path, sizeof(hist_path), "%s/polybar_clipboard.hist",
             runtime_dir);
  } else {
    snprintf(hist_path, sizeof(hist_path), "/tmp/polybar_clipboard.hist");
  }

  char *last_clip = NULL;

  while (1) {
    char *current_clip = clipboard_text(cb);

    if (current_clip && strlen(current_clip) > 0) {
      if (!last_clip || strcmp(current_clip, last_clip) != 0) {
        if (last_clip)
          free(last_clip);
        last_clip = strdup(current_clip);

        append_to_history(current_clip, hist_path);
      }
    }
    if (current_clip)
      free(current_clip);

    sleep(1);
  }

  if (last_clip)
    free(last_clip);
  clipboard_free(cb);
  close(lock_fd); // Libère le verrou à la fermeture proprement
  return 0;
}
