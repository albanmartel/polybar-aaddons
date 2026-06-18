#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <unistd.h>

// --- NOM PROGRAMME ---
#define PROGRAMME_NAME "net-tool"

typedef struct {
  unsigned long long rx;
} NetBytes;

void get_active_interface(char *iface, size_t max_len) {
  // 1. Sécurité : Validation du pointeur et de la taille
  if (!iface || max_len == 0) {
    return;
  }

  // On initialise la chaîne à "vide" par défaut (au cas où on ne trouve rien)
  iface[0] = '\0';

  FILE *f = fopen("/proc/net/route", "r");
  if (!f) {
    return;
  }

  char line[256];
  // Sauter la ligne d'en-tête
  if (fgets(line, sizeof(line), f) == NULL) {
    fclose(f);
    return;
  }

  while (fgets(line, sizeof(line), f)) {
    char itf[32];
    unsigned int dest;

    // sscanf avec "%31s" pour éviter d'exploser le tableau temporaire 'itf'
    if (sscanf(line, "%31s %x", itf, &dest) == 2 && dest == 0) {
      // 2. Sécurité : On utilise strncpy pour éviter le Buffer Overflow
      strncpy(iface, itf, max_len - 1);
      iface[max_len - 1] =
          '\0'; // Sécurité pour s'assurer que la chaîne se termine par \0
      break;
    }
  }

  fclose(f);
}

NetBytes get_bytes(const char *iface) {
  NetBytes bytes = {0};

  // SÉCURITÉ : Validation du pointeur et de la chaîne
  if (!iface || iface[0] == '\0')
    return bytes;

  FILE *f = fopen("/proc/net/dev", "r");
  if (!f)
    return bytes;

  char line[512];
  while (fgets(line, sizeof(line), f)) {
    char itf[32];
    unsigned long long rx_bytes = 0;

    // SÉCURITÉ OPTIMISÉE : sscanf extrait proprement l'interface et ses octets
    // reçus. " %31[^:]" ignore les espaces initiaux et s'arrête strictement au
    // caractère ':' Cela évite les faux positifs si une interface s'appelle
    // "veth1" et l'autre "eth1".
    if (sscanf(line, " %31[^:]:%llu", itf, &rx_bytes) == 2) {
      if (strcmp(itf, iface) == 0) {
        bytes.rx = rx_bytes;
        break;
      }
    }
  }
  fclose(f);
  return bytes;
}

// --- NOUVELLE FONCTION D'AFFICHAGE ---
void format_and_print_speed(unsigned long long diff) {
  // CONTROLE 1 : Vitesse maximale théorique (ex: brider à 999 Go/s)
  // 999 Go/s en base 2 = 999 * 1024 * 1024 * 1024
  const unsigned long long MAX_VITESSE_POSSIBLE = 1072693248000ULL;

  if (diff > MAX_VITESSE_POSSIBLE) {
    diff = MAX_VITESSE_POSSIBLE; // On sature à la valeur max proprement
  }

  // Icône noire collée au texte
  printf("%%{F#000000}󱘖 %%{F-}");

  // CONSTANTES POUR LES SEUILS (Calculs stricts en Base 2)
  const unsigned long long KIO = 1024ULL;
  const unsigned long long MIO = 1024ULL * 1024ULL;
  const unsigned long long GIO = 1024ULL * 1024ULL * 1024ULL;

  // LOGIQUE DE COMPTEUR FIXE (3 chiffres + unité/s)
  if (diff < KIO) {
    if (diff > 999)
      diff = 999;
    printf("%03lluo/s", diff);

  } else if (diff < MIO) {
    unsigned long long k = diff / KIO;
    if (k > 999)
      k = 999;
    printf("%03lluKo/s", k);

  } else if (diff < GIO) {
    unsigned long long m = diff / MIO;
    if (m > 999)
      m = 999;
    printf("%03lluMo/s", m);

  } else {
    unsigned long long g = diff / GIO;
    if (g > 999)
      g = 999;
    printf("%03lluGo/s", g);
  }

  printf("\n");
}

int main() {
  prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
  char iface[32] = {0};
  get_active_interface(iface, sizeof(iface));

  if (iface[0] == '\0') {
    printf("%%{F#000000}󱘖 %%{F-}---o\n");
    return 0;
  }

  NetBytes b1 = get_bytes(iface);
  usleep(1000000);
  NetBytes b2 = get_bytes(iface);

  // Sécurité anti-rollover
  unsigned long long diff = (b2.rx >= b1.rx) ? (b2.rx - b1.rx) : 0;

  // Appel de la nouvelle fonction
  format_and_print_speed(diff);

  return 0;
}