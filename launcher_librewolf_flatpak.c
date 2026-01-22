#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/prctl.h>

// --- NOM PROGRAMME ---
#define PROGRAMME_NAME "launcher_librewolf"

// Vérifier l'espace libre dans /tmp
long get_free_space_mb(const char *path) {
    struct statvfs stat;
    if (statvfs(path, &stat) != 0) return -1;
    return (stat.f_bsize * stat.f_bavail) / (1024 * 1024);
}

// Obtenir la taille du modèle
long get_dir_size_mb(const char *dir) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "du -sm \"%s\" | cut -f1", dir);
    FILE *fp = popen(cmd, "r");
    if (fp == NULL) return 0;
    char result[16];
    if (fgets(result, sizeof(result), fp) == NULL) { pclose(fp); return 0; }
    pclose(fp);
    return atol(result);
}

int main() {
	prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
    char *home = getenv("HOME");
    char source_model[512];
    char temp_profile[512];
    char cmd[2048];

    // --- ADAPTATION FLATPAK ---
    // Chemin vers votre modèle (que vous avez créé dans ~/.var/app/... ou ailleurs)
    snprintf(source_model, sizeof(source_model), "%s/.var/app/io.gitlab.librewolf-community/.librewolf/Modele", home);
    
    // Dossier temporaire
    snprintf(temp_profile, sizeof(temp_profile), "/tmp/lw_temp_%ld", (long)time(NULL));

    // 1. Vérifications
    long required = get_dir_size_mb(source_model);
    long available = get_free_space_mb("/tmp");

    printf("Modèle : %s\n", source_model);
    printf("Espace requis : %ld Mo | Disponible : %ld Mo\n", required, available);

    if (available < (required + 100)) { // Marge de 100Mo conseillée pour Flatpak
        fprintf(stderr, "Erreur : Espace insuffisant dans /tmp !\n");
        return 1;
    }

    // 2. Création et Copie
    if (mkdir(temp_profile, 0700) != 0) {
        perror("Erreur création dossier /tmp");
        return 1;
    }

    printf("Clonage du profil...\n");
    snprintf(cmd, sizeof(cmd), "cp -a \"%s/.\" \"%s/\"", source_model, temp_profile);
    if (system(cmd) != 0) {
        fprintf(stderr, "Erreur lors de la copie.\n");
        return 1;
    }

    // 3. Fork pour lancer Flatpak
    pid_t pid = fork();
    if (pid == 0) {
        // Lancement via Flatpak
        // --filesystem=/tmp est CRUCIAL pour que le bac à sable puisse lire le profil cloné
        execlp("flatpak", "flatpak", "run", 
               "--filesystem=/tmp", 
               "io.gitlab.librewolf-community", 
               "--profile", temp_profile, 
               "--no-remote", 
               (char *)NULL);
        
        perror("Erreur lors du lancement de Flatpak");
        exit(1);
    } else if (pid > 0) {
        wait(NULL); // Attend que l'utilisateur ferme LibreWolf
        
        // 4. Nettoyage
        printf("\nNettoyage du profil temporaire...\n");
        snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", temp_profile);
        system(cmd);
        printf("Session temporaire terminée.\n");
    }

    return 0;
}
