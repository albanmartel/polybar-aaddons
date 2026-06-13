#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <unistd.h>

// --- NOM PROGRAMME ---
#define PROGRAMME_NAME "cpu-tool"

// Devient un int pour propager l'état de réussite (1 = OK, 0 = Erreur)
int get_stats(unsigned long long *idle, unsigned long long *total) {
  // 1. Vérification des pointeurs d'entrée
  if (idle == NULL || total == NULL) {
    return 0;
  }

  // 2. Vérification de l'ouverture du fichier
  FILE *fp = fopen("/proc/stat", "r");
  if (fp == NULL) {
    return 0;
  }

  unsigned long long user, nice, system, idle_val, iowait, irq, softirq, steal;
  int success = 0;

  // 3. Vérification de la lecture des données
  if (fscanf(fp, "cpu %llu %llu %llu %llu %llu %llu %llu %llu", &user, &nice,
             &system, &idle_val, &iowait, &irq, &softirq, &steal) == 8) {
    *idle = idle_val + iowait;
    *total = user + nice + system + idle_val + iowait + irq + softirq + steal;
    success = 1; // Lecture réussie
  }

  // Le fclose est maintenant sécurisé car fp est garanti non NULL ici
  fclose(fp);
  return success;
}

int main() {
  prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
  unsigned long long idle1, total1, idle2, total2;

  // Première mesure
  get_stats(&idle1, &total1);

  // Attente d'une seconde pour voir l'évolution
  usleep(1000000);

  // Deuxième mesure
  get_stats(&idle2, &total2);

  unsigned long long totald = total2 - total1;
  unsigned long long idled = idle2 - idle1;

  double cpu_percent = 0.0;
  if (totald > 0) {
    cpu_percent = (double)(totald - idled) * 100.0 / (double)totald;
  }

  // Sécurité affichage
  if (cpu_percent > 99.0)
    cpu_percent = 99.0;
  if (cpu_percent < 0.0)
    cpu_percent = 0.0;

  // Ton format préféré : 2 chiffres fixes avec zéro de remplissage
  printf(" %02.0f%%\n", cpu_percent);

  return 0;
}
