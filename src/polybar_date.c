#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <time.h>

#define PROGRAMME_NAME "polybar_date"

/* T6 pour le haut, T5 pour le bas */
#define POLICE_HEURE "T6"
#define POLICE_DATE "T5"

void imprimer_format_polybar(const char *date, const char *heure) {
  if (date == NULL || heure == NULL) return;

  // --- CONFIGURATION STRICTE DES LARGEURS (En pixels) ---
  const int LARGEUR_MAX_MODULE = 80; // La boîte interne pour les 2 lignes
  
  // MARGES DE SÉCURITÉ : Vos espaces pour bloquer les modules connexes !
  const int MARGE_ISOLEMENT_PX = 10; // 10px à gauche et 10px à droite
  
  const int LARGEUR_HEURE_CHAR = 8; 
  const int LARGEUR_DATE_CHAR = 6;
  // ------------------------------------------------------

  // 1. Calcul de la taille réelle des textes
  int deplacement_heure = (int)strlen(heure) * LARGEUR_HEURE_CHAR;
  int deplacement_date = (int)strlen(date) * LARGEUR_DATE_CHAR;

  // 2. Calcul du centrage dans la boîte de 80px
  int marge_gauche_heure = (LARGEUR_MAX_MODULE - deplacement_heure) / 2;
  int marge_gauche_date = (LARGEUR_MAX_MODULE - deplacement_date) / 2;

  if (marge_gauche_heure < 0) marge_gauche_heure = 0;
  if (marge_gauche_date < 0) marge_gauche_date = 0;

  // --- SÉCURITÉ GAUCHE : On pousse le module précédent pour faire de la place
  printf("%%{O%d}", MARGE_ISOLEMENT_PX);

  // LIGNE 1 : On avance, on écrit l'HEURE, puis on recule AU POINT ZÉRO
  printf("%%{O%d}%%{" POLICE_HEURE "}%s%%{O-%d}", 
          marge_gauche_heure, heure, marge_gauche_heure + deplacement_heure);

  // LIGNE 2 : On avance de sa marge, on écrit la DATE
  printf("%%{O%d}%%{" POLICE_DATE "}%s", 
          marge_gauche_date, date);

  // FIN DE LA BOÎTE : On termine de parcourir la boîte de 80px
  int position_actuelle = marge_gauche_date + deplacement_date;
  int reste_a_pousser = LARGEUR_MAX_MODULE - position_actuelle;
  if (reste_a_pousser > 0) {
    printf("%%{O%d}", reste_a_pousser);
  }

  // --- SÉCURITÉ DROITE : On ajoute l'espace final pour bloquer le module suivant
  printf("%%{O%d}", MARGE_ISOLEMENT_PX);

  printf("\n");
}

void generer_date_polybar() {
  prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
  setlocale(LC_TIME, "fr_FR.UTF-8");

  time_t now;
  struct tm *tm_info;
  char date_buf[64];
  char time_buf[16];

  time(&now);
  tm_info = localtime(&now);
  if (tm_info == NULL) return;

  // "mer. 17 juin" et "23 : 48"
  strftime(date_buf, sizeof(date_buf), "%a %d %h", tm_info);
  strftime(time_buf, sizeof(time_buf), "%H : %M", tm_info);

  imprimer_format_polybar(date_buf, time_buf);
}

int main() {
  generer_date_polybar();
  return 0;
}
