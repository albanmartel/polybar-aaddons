#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/prctl.h>

// --- NOM PROGRAMME ---
#define PROGRAMME_NAME "reseau"

// --- FONCTIONS ---

void forcer_reconnexion() {
    printf("Début du processus de reconnexion...\n");
    printf("----------------------------------------------------------------------\n");

    // Récupérer les interfaces connectées via nmcli
    FILE *fp = popen("nmcli -t -f DEVICE,STATE device status | grep ':connected' | cut -d: -f1", "r");
    if (fp == NULL) return;

    char interface[64];
    int count = 0;

    while (fgets(interface, sizeof(interface), fp) != NULL) {
        interface[strcspn(interface, "\n")] = 0; // Nettoyer le saut de ligne
        printf("🔄 Traitement de l'interface : %s\n", interface);
        
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "nmcli device disconnect '%s' > /dev/null 2>&1", interface);
        system(cmd);
        
        sleep(1);
        
        snprintf(cmd, sizeof(cmd), "nmcli device connect '%s' > /dev/null 2>&1", interface);
        if (system(cmd) == 0) {
            printf("  ✅ %s reconnectée.\n", interface);
        } else {
            printf("  ❌ Échec pour %s.\n", interface);
        }
        count++;
    }
    
    if (count == 0) printf("⚠️ Aucune interface connectée trouvée.\n");
    
    pclose(fp);
    printf("----------------------------------------------------------------------\nTerminé.\n");
}

void verifier_internet() {
    printf("📡 Test IP (8.8.8.8)...\n");
    if (system("ping -c 3 8.8.8.8 > /dev/null 2>&1") == 0) {
        printf("  ✅ Ping IP OK.\n");
        printf("🔎 Test DNS (google.fr)...\n");
        if (system("ping -c 3 google.fr > /dev/null 2>&1") == 0) {
            printf("  ✅ Internet OK.\n");
        } else {
            printf("  ⚠️ Erreur DNS.\n");
        }
    } else {
        printf("  ❌ Pas de réseau.\n");
    }
}

// --- MAIN ---

int main() {
	prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
    // Menu Rofi
    const char *options = "1. Vérifier la connexion\n2. Reconnecter les interfaces";
    char rofi_cmd[512];
    snprintf(rofi_cmd, sizeof(rofi_cmd), "echo -e \"%s\" | rofi -dmenu -p \"Réseau\" -width 30 -lines 2", options);

    FILE *pipe_rofi = popen(rofi_cmd, "r");
    if (pipe_rofi == NULL) return 1;

    char choice[128];
    if (fgets(choice, sizeof(choice), pipe_rofi) != NULL) {
        pclose(pipe_rofi); // Fermer le pipe de rofi avant d'ouvrir zenity

        // Préparation du pipe vers Zenity pour afficher la sortie en temps réel
        FILE *zenity_pipe = popen("zenity --text-info --title='Diagnostic Réseau' --width=450 --height=300", "w");
        if (zenity_pipe == NULL) return 1;

        // Rediriger stdout vers le pipe de zenity pour que printf écrive dans zenity
        // Note: Pour faire simple ici, on utilise un buffer ou on écrit directement dans le pipe
        if (strstr(choice, "Vérifier") != NULL) {
            // On détourne le flux vers zenity
            dup2(fileno(zenity_pipe), STDOUT_FILENO); 
            verifier_internet();
        } 
        else if (strstr(choice, "Reconnecter") != NULL) {
            dup2(fileno(zenity_pipe), STDOUT_FILENO);
            forcer_reconnexion();
        }

        fflush(stdout);
        pclose(zenity_pipe);
    } else {
        pclose(pipe_rofi);
    }

    return 0;
}
