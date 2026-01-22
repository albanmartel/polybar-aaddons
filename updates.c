#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>

// --- NOM PROGRAMME ---
#define PROGRAMME_NAME "update"

int main() {
	prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
    // 1. Ouvrir un "pipe" pour lire la sortie de la commande checkupdates
    // On utilise wc -l directement dans la commande pour récupérer uniquement le nombre
    FILE *fp = popen("checkupdates 2>/dev/null | wc -l", "r");
    
    if (fp == NULL) {
        // Si la commande échoue (ex: pacman-contrib non installé)
        printf("󰂭\n");
        return 1;
    }

    char buffer[16];
    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        // Convertir la chaîne lue en entier
        int updates = atoi(buffer);

        // 2. Logique d'affichage
        if (updates > 0) {
            // Icône de mise à jour (󰚰) + nombre
            printf("󰚰 %d\n", updates);
        } else {
            // Icône "système à jour" (󰄲)
            printf("󰄲\n");
        }
    }

    pclose(fp);
    return 0;
}
