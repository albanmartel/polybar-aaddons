#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/prctl.h>

// --- NOM PROGRAMME ---
#define PROGRAMME_NAME "launcher-qterminal"

int main(void) {
    // On définit le nom du processus dans le gestionnaire de tâches
    prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);

    // Préparation des arguments pour QTerminal
    // Note : Pour la police, QTerminal attend une chaîne au format "Nom,Taille"
    char *launcher_argv[] = {
        "qterminal", 
        "--geometry", "900x700",
        NULL 
    };

    // Exécution
    if (execvp(launcher_argv[0], launcher_argv) == -1) {
        fprintf(stderr, "Erreur : Impossible de lancer qterminal (%s)\n", strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
