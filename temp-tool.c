#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>

// --- NOM PROGRAMME ---
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <string.h>
#include <unistd.h>

#define PROGRAMME_NAME "temp-tool"

int main() {
    prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
    FILE *fp = NULL;
    char path[256];
    int temp_raw = 0;

    // --- STRATÉGIE 1 : Le standard ARM / ACPI (Thermal Zones) ---
    // On boucle sur les 3 premières zones, car sur ARM, le CPU bouge souvent.
    for (int i = 0; i < 3; i++) {
        sprintf(path, "/sys/class/thermal/thermal_zone%d/temp", i);
        fp = fopen(path, "r");
        if (fp) {
            if (fscanf(fp, "%d", &temp_raw) == 1 && temp_raw > 0) {
                fclose(fp);
                goto found; // On a trouvé une valeur cohérente
            }
            fclose(fp);
        }
    }

    // --- STRATÉGIE 2 : Le standard x86 (hwmon pour AMD/Intel) ---
    for (int i = 0; i < 5; i++) {
        sprintf(path, "/sys/class/hwmon/hwmon%d/temp1_input", i);
        fp = fopen(path, "r");
        if (fp) {
            if (fscanf(fp, "%d", &temp_raw) == 1) {
                fclose(fp);
                goto found;
            }
            fclose(fp);
        }
    }

    // Si on arrive ici, rien n'a été trouvé
    printf(" --°C\n");
    return 1;

found:
    {
        int temp = temp_raw / 1000;
        if (temp > 99) temp = 99;
        if (temp < 0) temp = 0;
        printf(" %02d°C\n", temp);
    }
    
    return 0;
}
