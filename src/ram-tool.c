#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>

#define PROGRAMME_NAME "ram-tool"

int main() {
  prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);

  FILE *fp = fopen("/proc/meminfo", "r");
  if (fp == NULL) {
    printf(" --.-G\n");
    return 1;
  }

  // Initialisation par sécurité
  unsigned long total = 0;
  unsigned long available = 0;
  char line[256];
  int found = 0;

  // Lecture optimisée et robuste
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

  // --- GESTION STRICTE DES ERREURS DE LECTURE ---
  if (ferror(fp)) {
    // Une erreur de lecture s'est produite au niveau du système de
    // fichier/noyau
    fclose(fp);
    printf(" --.-G\n");
    return 1;
  }

  fclose(fp);

  // Si MemAvailable n'a pas été trouvé (vieux noyau ou environnement restreint)
  if (found < 2 || total == 0) {
    printf(" --.-G\n");
    return 1;
  }

  // Calcul de la RAM utilisée en Go
  double used_gb = (double)(total - available) / (1024.0 * 1024.0);

  // Sécurité pour le format
  if (used_gb > 99.9)
    used_gb = 99.9;
  if (used_gb < 0.0)
    used_gb = 0.0; // Au cas où available > total (rare bug noyau)

  // Affichage propre
  printf(" %04.1fG\n", used_gb);

  return 0;
}