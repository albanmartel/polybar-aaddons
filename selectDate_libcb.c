#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <libclipboard.h>

int main() {
    char date[128] = {0};
    char info_cmd[512];
    FILE *fp;

    // 1. Initialisation de libclipboard
    clipboard_c *cb = clipboard_new(NULL);
    if (cb == NULL) {
        fprintf(stderr, "Impossible d'initialiser libclipboard\n");
        return 1;
    }

    // 2. Appel de Zenity
    fp = popen("zenity --calendar --text \"Sélectionner une date\" --date-format \"%Y-%m-%d\"", "r");
    if (fp == NULL) {
        perror("Erreur Zenity");
        clipboard_free(cb);
        return 1;
    }

    if (fgets(date, sizeof(date), fp) != NULL) {
        date[strcspn(date, "\n")] = 0;
    }
    pclose(fp);

    // 3. Gestion du cas vide (Date courante)
    if (strlen(date) == 0) {
        time_t t = time(NULL);
        struct tm *tm_info = localtime(&t);
        strftime(date, sizeof(date), "%Y-%m-%d", tm_info);

        snprintf(info_cmd, sizeof(info_cmd), 
                 "zenity --info --text \"Pas de sélection.\nLa date courante : %s\nest copiée dans le presse-papier\"", date);
    } else {
        snprintf(info_cmd, sizeof(info_cmd), 
                 "zenity --info --text \"Date sélectionnée : %s\nest copiée dans le presse-papier\"", date);
    }

    // 4. Copie dans le presse-papier
    clipboard_set_text(cb, date);

    // 5. Affichage et notification
    printf("%s\n", date);
    system(info_cmd);

    clipboard_free(cb);
    return 0;
}