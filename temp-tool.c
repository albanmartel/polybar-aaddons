#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>

// --- NOM PROGRAMME ---
#define PROGRAMME_NAME "temp-tool"

int main() {
	prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
    // Sur la plupart des systèmes, thermal_zone0 est le CPU
    FILE *fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    
    if (fp == NULL) {
        // Si zone0 n'existe pas, on tente zone1 (souvent le cas sur certains laptops)
        fp = fopen("/sys/class/thermal/thermal_zone1/temp", "r");
    }

    if (fp == NULL) {
        printf(" --°C\n");
        return 1;
    }

    int temp_raw;
    if (fscanf(fp, "%d", &temp_raw) != 1) {
        printf(" ??°C\n");
        fclose(fp);
        return 1;
    }
    fclose(fp);

    // La valeur est en millidegrés (ex: 45000 pour 45°C)
    int temp = temp_raw / 1000;

    // Sécurité pour l'affichage : on bloque à 99 pour rester sur 2 chiffres
    if (temp > 99) temp = 99;
    if (temp < 0) temp = 0;

    // Format %02d : affiche toujours 2 chiffres (ex: 08°C, 42°C)
    printf(" %02d°C\n", temp);

    return 0;
}
