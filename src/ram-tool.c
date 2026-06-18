#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <unistd.h>

#define PROGRAMME_NAME "ram-tool"
#define POLICE_UNIQUE "T2"

/*--- FONCTION DE TRAITEMENT ET D'AFFICHAGE ---*/

void print_ram_usage(void) {
  // Vérification d'accès au fichier
  if (access("/proc/meminfo", R_OK) != 0) {
    printf("%%{" POLICE_UNIQUE "} --.-G\n");
    return;
  }

  FILE *fp = fopen("/proc/meminfo", "r");
  if (fp == NULL) {
    printf("%%{" POLICE_UNIQUE "} --.-G\n");
    return;
  }

  unsigned long total = 0;
  unsigned long available = 0;
  char line[256];
  int found = 0;

  // Lecture des données de la RAM
  while (fgets(line, sizeof(line), fp)) {
    if (strncmp(line, "MemTotal:", 9) == 0) {
      sscanf(line, "MemTotal: %lu", &total);
      found++;
    } else if (strncmp(line, "MemAvailable:", 13) == 0) {
      sscanf(line, "MemAvailable: %lu", &available);
      found++;
    }

    if (found == 2)
      break;
  }

  if (ferror(fp)) {
    fclose(fp);
    printf("%%{" POLICE_UNIQUE "} --.-G\n");
    return;
  }
  fclose(fp);

  // Validation
  if (found < 2 || total == 0) {
    printf("%%{" POLICE_UNIQUE "} --.-G\n");
    return;
  }

  // Calcul de la RAM utilisée en Go
  double used_gb = (double)(total - available) / (1024.0 * 1024.0);

  // Gardes-fous de sécurité
  if (used_gb > 99.9) used_gb = 99.9;
  if (used_gb < 0.0)  used_gb = 0.0;

  // --- CONFIGURATION DES ESPACES (En pixels) ---
  // On applique une marge de 8 pixels de chaque côté du module
  const int MARGE_ISOLEMENT_PX = 8; 

  // --- AFFICHAGE POLYBAR SÉCURISÉ ---
  printf("%%{" POLICE_UNIQUE "}");
  
  // 1. On pousse le module de gauche pour créer la zone tampon
  printf("%%{O%d}", MARGE_ISOLEMENT_PX);
  
  // 2. On affiche l'icône et la valeur (largeur fixe de 4 caractères pour le chiffre)
  printf(" %4.1fG", used_gb);
  
  // 3. On pousse le module de droite pour terminer l'isolation
  printf("%%{O%d}\n", MARGE_ISOLEMENT_PX);
}

// --- MAIN ---
int main(void) {
  prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
  print_ram_usage();
  return 0;
}
