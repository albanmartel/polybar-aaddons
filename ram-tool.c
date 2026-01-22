#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>

// --- NOM PROGRAMME ---
#define PROGRAMME_NAME "ram-tool"

int main() {
	prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
    FILE *fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        printf("  --.-G");
        return 1;
    }

    unsigned long total, available;
    char line[256];
    int found = 0;

    // On parcourt meminfo pour extraire MemTotal et MemAvailable
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "MemTotal: %lu kB", &total) == 1) found++;
        if (sscanf(line, "MemAvailable: %lu kB", &available) == 1) found++;
        if (found == 2) break;
    }
    fclose(fp);

    // Calcul de la RAM utilisée en Go
    double used_gb = (double)(total - available) / (1024.0 * 1024.0);

    // Sécurité pour ne pas dépasser 99.9G (format fixe)
    if (used_gb > 99.9) used_gb = 99.9;

    // Format %04.1f :
    // - 0 : remplit avec un zéro au lieu d'un espace
    // - 4 : largeur totale de 4 caractères (ex: 04.2)
    // - .1 : un chiffre après la virgule
    printf("  %04.1fG\n", used_gb);

    return 0;
}
