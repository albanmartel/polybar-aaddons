#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <ctype.h>

// --- NOM PROGRAMME ---
#define PROGRAMME_NAME "kbd-status"

// Définition des couleurs
#define COLOR_ON  "#CC5500"
/* #define COLOR_OFF "#707880" */
#define COLOR_OFF "-"

// Fonction pour récupérer la sortie d'une commande
void get_command_output(const char* cmd, char* buffer, size_t size) {
    FILE* fp = popen(cmd, "r");
    if (fp != NULL) {
        if (fgets(buffer, size, fp) == NULL) buffer[0] = '\0';
        pclose(fp);
    }
}

int main() {
    prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
    
    char layout[16] = "??";
    // Initialisation par défaut pour éviter l'erreur "maybe-uninitialized"
    const char *caps = COLOR_OFF;
    const char *num  = COLOR_OFF;

    // 1. Détection de la langue (Layout)
    get_command_output("setxkbmap -query | grep layout | awk '{print $2}'", layout, sizeof(layout));
    
    // Nettoyage et mise en majuscules (utilisation de toupper pour la propreté)
    for (int i = 0; layout[i]; i++) {
        if (layout[i] == '\n') {
            layout[i] = '\0';
            break;
        }
        layout[i] = (unsigned char)toupper(layout[i]);
    }

    // 2. Détection des états via xset
    FILE* fp = popen("xset q", "r");
    if (fp != NULL) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            // On cherche la ligne contenant les indicateurs
            if (strstr(line, "Caps Lock:")) {
                if (strstr(line, "Caps Lock:   on")) caps = COLOR_ON;
                if (strstr(line, "Num Lock:    on")) num = COLOR_ON;
                break;
            }
        }
        pclose(fp);
    }

    // 3. Affichage final formaté pour Polybar
    // Utilisation de doubles %% pour échapper le caractère % dans printf
    /*--- affiche un fond transparent ---*/
    printf("%%{B-} %s  %%{F%s}󰪛%%{F-}  %%{F%s}󰎤%%{F-}%%{B-}\n", 
            layout, 
            caps, 
            num);

    return 0;
}
