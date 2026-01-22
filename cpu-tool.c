#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/prctl.h>

// --- NOM PROGRAMME ---
#define PROGRAMME_NAME "cpu-tool"

void get_stats(unsigned long long *idle, unsigned long long *total) {
    FILE *fp = fopen("/proc/stat", "r");
    unsigned long long user, nice, system, idle_val, iowait, irq, softirq, steal;
    if (fscanf(fp, "cpu %llu %llu %llu %llu %llu %llu %llu %llu", 
               &user, &nice, &system, &idle_val, &iowait, &irq, &softirq, &steal) == 8) {
        *idle = idle_val + iowait;
        *total = user + nice + system + idle_val + iowait + irq + softirq + steal;
    }
    fclose(fp);
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
    if (cpu_percent > 99.0) cpu_percent = 99.0;
    if (cpu_percent < 0.0) cpu_percent = 0.0;

    // Ton format préféré : 2 chiffres fixes avec zéro de remplissage
    printf("  %02.0f%%\n", cpu_percent);

    return 0;
}
