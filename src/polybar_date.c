#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <time.h>

// --- NOM PROGRAMME ---
#define PROGRAMME_NAME "polybar_date"

// --- FONCTIONS ---

/**
 * Gère uniquement l'affichage formaté pour Polybar
 */
void imprimer_format_polybar(const char *date, const char *heure) {
  if (date == NULL || heure == NULL) {
    fprintf(stderr,
            "Erreur : Pointeurs de date ou d'heure invalides (NULL).\n");
    return;
  }

  // --- RÉGLAGE FIN (Ajustez ces deux valeurs en pixels) ---
  // Largeur moyenne d'un caractère pour chaque police dans Polybar :
  const int LARGEUR_DATE_CHAR_PX = 6;  // Taille estimée pour T3 (date)
  const int LARGEUR_HEURE_CHAR_PX = 8; // Taille estimée pour T4 (heure)
  // --------------------------------------------------------

  // 1. Calcul des largeurs totales en pixels
  int total_date_px = strlen(date) * LARGEUR_DATE_CHAR_PX;
  int total_heure_px = strlen(heure) * LARGEUR_HEURE_CHAR_PX;

  // 2. On cherche à aligner les centres.
  // Décalage pour la date = (Largeur Heure / 2) - (Largeur Date / 2)
  int offset_date = (total_heure_px / 1) - (total_date_px / 2);

  /*-- Affichage formaté Polybar --*/
  // %{T4}%s      : On écrit d'abord l'HEURE (qui sert de base fixe)
  // %%{O-%d}     : On recule du pixel-offset total de l'heure pour revenir au
  // DEBUT exact du bloc
  // %%{O(%s%d)}  : On avance (ou recule) du petit offset pour centrer la date
  // %{T3}%s      : On écrit la DATE

  if (offset_date >= 0) {
    // Si la date est plus courte que l'heure, on avance positivement
    printf("%%{T4}%s%%{O-%d}%%{O+%d}%%{T3}%s\n", heure, total_heure_px,
           offset_date, date);
  } else {
    // Si la date dépasse de l'heure, on recule un peu plus
    printf("%%{T4}%s%%{O-%d}%%{O-%d}%%{T3}%s\n", heure, total_heure_px,
           -offset_date, date);
  }
}

/**
 * Récupère le temps actuel et prépare les chaînes de caractères
 */
void generer_date_polybar() {
  prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
  setlocale(LC_TIME, "fr_FR.UTF-8");

  time_t now;
  struct tm *tm_info;
  char date_buf[64];
  char time_buf[16];

  time(&now);
  tm_info = localtime(&now);

  // En C robuste, on pourrait aussi vérifier si localtime n'a pas échoué
  if (tm_info == NULL) {
    fprintf(stderr, "Erreur : Impossible de récupérer le temps local.\n");
    return;
  }

  // Formatage des données
  strftime(date_buf, sizeof(date_buf), "%a %d %h", tm_info);
  strftime(time_buf, sizeof(time_buf), "%H : %M", tm_info);

  // On transmet les buffers à la fonction d'affichage
  imprimer_format_polybar(date_buf, time_buf);
}

// --- MAIN ---
int main() {
  generer_date_polybar();
  return 0;
}