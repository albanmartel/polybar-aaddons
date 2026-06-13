#include <errno.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <unistd.h>

#define PROGRAMME_NAME "launcher-qterminal"

/**
 * @brief Vérifie la présence de qterminal dans le PATH et l'exécute
 * en remplaçant le processus actuel.
 * @return Ne retourne jamais en cas de succès. Renvoie -1 en cas d'erreur.
 */
int launch_qterminal(void) {
  // 1. Sécurité : On cherche si qterminal est présent dans le $PATH
  char *qterminal_path = g_find_program_in_path("qterminal");

  if (qterminal_path == NULL) {
    fprintf(stderr, "Erreur Critique [launch_qterminal] : 'qterminal' est "
                    "introuvable dans le PATH de votre système.\n");
    return -1;
  }

  // Le binaire existe, on peut libérer la chaîne allouée par GLib
  g_free(qterminal_path);

  // 2. On définit le nom du processus dans le gestionnaire de tâches
  prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);

  // 3. Préparation des arguments pour QTerminal
  char *launcher_argv[] = {"qterminal", "--geometry", "900x700", NULL};

  // 4. Exécution (remplace le processus actuel)
  execvp(launcher_argv[0], launcher_argv);

  // Si execvp retourne, c'est qu'il y a eu un échec critique
  fprintf(stderr,
          "Erreur Critique [launch_qterminal] : Impossible de lancer qterminal "
          "(%s)\n",
          strerror(errno));
  return -1;
}

/**
 * @brief Point d'entrée principal du wrapper.
 */
int main(void) {
  if (launch_qterminal() == -1) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
