#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/prctl.h>

// --- NOM PROGRAMME ---
#define PROGRAMME_NAME "net-tool"

typedef struct {
    unsigned long long rx;
} NetBytes;

void get_active_interface(char *iface) {
    FILE *f = fopen("/proc/net/route", "r");
    if (!f) return;
    char line[256];
    fgets(line, sizeof(line), f);
    while (fgets(line, sizeof(line), f)) {
        char itf[32];
        unsigned int dest;
        if (sscanf(line, "%s %x", itf, &dest) == 2 && dest == 0) {
            strcpy(iface, itf);
            break;
        }
    }
    fclose(f);
}

NetBytes get_bytes(const char *iface) {
    NetBytes bytes = {0};
    FILE *f = fopen("/proc/net/dev", "r");
    if (!f) return bytes;
    char line[512];
    char search[64];
    snprintf(search, sizeof(search), "%s:", iface);
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
    get_active_interface(iface);
    
    if (iface[0] == '\0') {
        printf("%%{F#000000}󱘖%%{F-}--- o"); // Format stable même hors ligne
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
        printf("%03llu o", diff);
    } 
    else if (diff < 1000000) {
        // Affiche de 001 K à 999 K
        unsigned long long k = diff / 1024;
        if (k == 0) printf("999 o"); // Sécurité pour la transition
        else printf("%03llu K", k);
    } 
    else {
        // Affiche de 001 M à 999 M
        unsigned long long m = diff / 1048576;
        printf("%03llu M", m);
    }

    printf("\n");
    return 0;
}
