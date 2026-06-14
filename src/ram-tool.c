#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <unistd.h>

#define PROGRAMME_NAME "ram-tool"

/*--- FONCTION DE TRAITEMENT ET D'AFFICHAGE ---*/

void print_ram_usage(void) {
  // Vérification stricte d'accès : le fichier existe-t-il et est-il lisible ?
  if (access("/proc/meminfo", R_OK) != 0) {
    printf(" --.-G\n");
    return;
  }

  FILE *fp = fopen("/proc/meminfo", "r");
  if (fp == NULL) {
    printf(" --.-G\n");
    return;
  }

  unsigned long total = 0;
  unsigned long available = 0;
  char line[256];
  int found = 0;

  // Lecture optimisée
  while (fgets(line, sizeof(line), fp)) {
    if (strncmp(line, "MemTotal:", 9) == 0) {
      sscanf(line + 9, "%lu", &total);
      found++;
    } else if (strncmp(line, "MemAvailable:", 13) == 0) {
      sscanf(line + 13, "%lu", &available);
      found++;
    }

    if (found == 2)
      break;
  }

  // Gestion des erreurs de lecture matérielle/noyau
  if (ferror(fp)) {
    fclose(fp);
    printf(" --.-G\n");
    return;
  }

  fclose(fp);

  // Validation des données récupérées
  if (found < 2 || total == 0) {
    printf(" --.-G\n");
    return;
  }

  // Calcul de la RAM utilisée en Go
  double used_gb = (double)(total - available) / (1024.0 * 1024.0);

  // Gardes-fous (Sécurité pour le format d'affichage)
  if (used_gb > 99.9)
    used_gb = 99.9;
  if (used_gb < 0.0)
    used_gb = 0.0;

  // Affichage final propre
  printf(" %04.1fG\n", used_gb);
}

// --- MAIN (Épuré et minimaliste) ---

int main(void) {
  prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);

  // Appel de la fonction de traitement
  print_ram_usage();

  return 0;
}