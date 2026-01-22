#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/prctl.h>

// --- CONFIGURATION ---
#define PROGRAMME_NAME  "tasklist"
#define PATH_ICON_LIST  "/home/alban/.config/polybar/icons.list" 
#define MAX_ICONS       256

// Couleurs Polybar
#define COLOR_ACTIVE    "#3498db"
#define COLOR_INACTIVE  "#000000"

// Icônes de secours (Catégories)
#define ICON_DEFAULT    "󰖲"
#define ICON_TERM       ""
#define ICON_WEB        "󰈹"
#define ICON_EDIT       "󰷈"
#define ICON_FM         "󰉋" 
#define ICON_MEDIA      "󰕼"
#define ICON_GEAR       ""

typedef struct {
    char key[64];
    char icon[16];
} AppIcon;

AppIcon icon_table[MAX_ICONS];
int icon_count = 0;

// --- CHARGEMENT DES ICONES ---
void load_icons() {
    FILE *file = fopen(PATH_ICON_LIST, "r");
    if (!file) return;

    char line[128];
    while (fgets(line, sizeof(line), file) && icon_count < MAX_ICONS) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == ' ') continue;

        char *sep = strchr(line, ':');
        if (sep) {
            *sep = '\0';
            char *icon_ptr = sep + 1;
            icon_ptr[strcspn(icon_ptr, "\r\n")] = 0; 

            // %.63s dit : "copie au maximum 63 caractères"
            // Cela correspond exactement à la taille de ton tableau key[64]
            snprintf(icon_table[icon_count].key, sizeof(icon_table[icon_count].key), "%.63s", line);
            snprintf(icon_table[icon_count].icon, sizeof(icon_table[icon_count].icon), "%.15s", icon_ptr);
            
            icon_count++;
        }
    }
    fclose(file);
}

// --- LOGIQUE DE SELECTION D'ICONE ---
const char* get_icon(const char *class_name) {
    // 1. Recherche dans le fichier icons.list
    for (int i = 0; i < icon_count; i++) {
        if (strstr(class_name, icon_table[i].key) != NULL) {
            return icon_table[i].icon;
        }
    }

    // 2. Fallbacks automatiques par mots-clés
    if (strstr(class_name, "term") || strstr(class_name, "shell")) return ICON_TERM;
    if (strstr(class_name, "browser") || strstr(class_name, "web")) return ICON_WEB;
    if (strstr(class_name, "edit") || strstr(class_name, "write"))  return ICON_EDIT;
    if (strstr(class_name, "file") || strstr(class_name, "fm")) return ICON_FM;
    if (strstr(class_name, "player") || strstr(class_name, "video")) return ICON_MEDIA;
    if (strstr(class_name, "config") || strstr(class_name, "settings")) return ICON_GEAR;

    return ICON_DEFAULT;
}

// --- UTILITAIRES ---
void normalize_id(const char* raw_id, char* clean_id, size_t size) {
    const char* p = raw_id;
    if (strncmp(p, "0x", 2) == 0) p += 2;
    while (*p == '0' && *(p+1) != '\0') p++;
    snprintf(clean_id, size, "%s", p);
}

// --- PROGRAMME PRINCIPAL ---
int main() {
    prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
    load_icons();

    char active_win_id[64] = "";
    char line[1024];

    // Récupération fenêtre active
    FILE *fx = popen("xprop -root _NET_ACTIVE_WINDOW 2>/dev/null", "r");
    if (fx) {
        if (fgets(line, sizeof(line), fx)) {
            char *id_ptr = strrchr(line, ' ');
            if (id_ptr) {
                normalize_id(id_ptr + 1, active_win_id, sizeof(active_win_id));
                active_win_id[strcspn(active_win_id, "\n")] = 0;
            }
        }
        pclose(fx);
    }

    // Lecture des fenêtres ouvertes
    FILE *fw = popen("wmctrl -lx", "r");
    if (!fw) return 1;

    while (fgets(line, sizeof(line), fw)) {
        char id[32], desk[8], class[256], host[256], title[512];
        if (sscanf(line, "%31s %7s %255s %255s %511[^\n]", id, desk, class, host, title) >= 4) {
            
            // Filtres d'exclusion
            if (strstr(class, "polybar") || 
                strstr(class, "gdesktop")) { // Ajout de votre programme ici
                continue;
            }

            char class_low[256];
            snprintf(class_low, sizeof(class_low), "%s", class);
            for(int i = 0; class_low[i]; i++) class_low[i] = (unsigned char)tolower(class_low[i]);

            const char* icon = get_icon(class_low);
            char clean_current_id[64];
            normalize_id(id, clean_current_id, sizeof(clean_current_id));

            if (strcmp(clean_current_id, active_win_id) == 0) {
                printf("%%{F%s}%%{u%s}%%{+u}  %s  %%{-u}%%{F-} ", COLOR_ACTIVE, COLOR_ACTIVE, icon);
            } else {
                printf("%%{A1:wmctrl -i -a %s:}%%{F%s}  %s  %%{F-}%%{A} ", id, COLOR_INACTIVE, icon);
            }
        }
    }
    printf("\n");
    pclose(fw);
    return 0;
}