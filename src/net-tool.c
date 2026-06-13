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
      // 2. Sécurité : On utilise g_strlcpy ou strncpy pour éviter le Buffer
      // Overflow g_strlcpy (de GLib) ou strncpy garantit qu'on ne dépasse pas
      // max_len
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
  char search[64];

  // %62s: pour s'assurer que search[64] ne déborde jamais, même si iface est
  // trop long
  snprintf(search, sizeof(search), "%.62s:", iface);

  while (fgets(line, sizeof(line), f)) {
    char *ptr = strstr(line, search);
    if (ptr) {
      ptr += strlen(search);
      sscanf(ptr, "%llu", &bytes.rx);
      break;
    }
  }
  fclose(f);
  return bytes;
}

int main() {
  prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
  char iface[32] = {0};
  get_active_interface(iface, sizeof(iface));

  if (iface[0] == '\0') {
    printf("%%{F#000000}󱘖 %%{F-}---o"); // Format stable même hors ligne
    return 0;
  }

  NetBytes b1 = get_bytes(iface);
  usleep(1000000);
  NetBytes b2 = get_bytes(iface);

  unsigned long long diff = b2.rx - b1.rx;

  // Icône noire collée au texte
  printf("%%{F#000000}󱘖 %%{F-}");

  // LOGIQUE DE COMPTEUR FIXE (3 chiffres + 1 espace + 1 unité)
  if (diff < 1000) {
    // Affiche de 000 o à 999 o
    printf("%03lluo", diff);
  } else if (diff < 1000000) {
    // Affiche de 001 K à 999 K
    unsigned long long k = diff / 1024;
    if (k == 0)
      printf("999o"); // Sécurité pour la transition
    else
      printf("%03lluK", k);
  } else {
    // Affiche de 001 M à 999 M
    unsigned long long m = diff / 1048576;
    printf("%03lluM", m);
  }

  printf("\n");
  return 0;
}
