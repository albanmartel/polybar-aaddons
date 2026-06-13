#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>

// --- NOM PROGRAMME ---
#define PROGRAMME_NAME "kbd-status"

// Définition des couleurs
#define COLOR_ON "#CC5500"   // Orange
#define COLOR_BASE "#000000" // Noir forcé
#define COLOR_OFF "#707880"  // Noir pour les icônes éteintes

/**
 * @brief Récupère la première ligne de la sortie d'une commande shell.
 * @param cmd La commande système à exécuter (ex: "uname -r"). Ne doit pas être
 * NULL.
 * @param buffer Le tampon de destination pour stocker le résultat. Ne doit pas
 * être NULL.
 * @param size La taille maximale du tampon de destination (doit être > 0).
 */
void get_command_output(const char *cmd, char *buffer, size_t size) {
  // 1. Sécurité : On valide d'abord le buffer et sa taille
  if (buffer == NULL || size == 0) {
    // Si le buffer existe mais que size est à 0, on ne peut rien faire.
    // Si buffer est valide, on s'assure qu'il commence par une chaîne vide.
    if (buffer != NULL && size > 0) {
      buffer[0] = '\0';
    }
    fprintf(stderr,
            "Erreur [get_command_output] : Buffer invalide ou taille nulle.\n");
    return;
  }

  // Par sécurité, on initialise le buffer à une chaîne vide immédiatement
  buffer[0] = '\0';

  // 2. Sécurité : On vérifie que la commande n'est pas NULL ou vide
  if (cmd == NULL || strlen(cmd) == 0) {
    fprintf(
        stderr,
        "Erreur [get_command_output] : La commande (cmd) est NULL ou vide.\n");
    return;
  }

  // 3. Exécution sécurisée de l'appel système
  FILE *fp = popen(cmd, "r");
  if (fp != NULL) {
    // fgets est sûr car il respecte la taille 'size' fournie
    if (fgets(buffer, size, fp) == NULL) {
      buffer[0] = '\0'; // On s'assure que la chaîne reste propre en cas d'échec
                        // de lecture
    }
    pclose(fp);
  } else {
    fprintf(stderr,
            "Erreur [get_command_output] : Impossible d'exécuter popen pour la "
            "commande : '%s'\n",
            cmd);
  }
}

int main() {
  prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);

  char layout[16] = "??";
  // Initialisation par défaut pour éviter l'erreur "maybe-uninitialized"
  const char *caps = COLOR_OFF;
  const char *num = COLOR_OFF;

  // 1. Détection de la langue (Layout)
  get_command_output("setxkbmap -query | grep layout | awk '{print $2}'",
                     layout, sizeof(layout));

  // Nettoyage et mise en majuscules (utilisation de toupper pour la propreté)
  for (int i = 0; layout[i]; i++) {
    if (layout[i] == '\n') {
      layout[i] = '\0';
      break;
    }
    layout[i] = (unsigned char)toupper(layout[i]);
  }

  // 2. Détection des états via xset
  FILE *fp = popen("xset q", "r");
  if (fp != NULL) {
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
      // On cherche la ligne contenant les indicateurs
      if (strstr(line, "Caps Lock:")) {
        if (strstr(line, "Caps Lock:   on"))
          caps = COLOR_ON;
        if (strstr(line, "Num Lock:    on"))
          num = COLOR_ON;
        break;
      }
    }
    pclose(fp);
  }

  // 3. Affichage final formaté pour Polybar
  // Utilisation de doubles %% pour échapper le caractère % dans printf
  /*--- affiche un fond transparent ---*/
  printf("%%{F%s} %s  %%{F%s}󰪛%%{F-}  %%{F%s}󰎤%%{F-}\n", COLOR_BASE,
         layout, caps, num);

  return 0;
}
