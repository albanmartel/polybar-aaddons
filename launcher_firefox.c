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

// --- CONFIGURATION DES CHEMINS ---
#define PROGRAMME_NAME "launcher_firefox"
#define SOURCE_SUBPATH "/.mozilla/firefox/Modele"
#define TEMP_BASE_PATH "/tmp"
#define FIREFOX_BIN    "firefox"
#define GTK_THEME_VAL  "Adwaita:dark"
#define MARGIN_MB      50

// --- MESSAGES UTILISATEUR (FRANÇAIS) ---
#define MSG_ESPACE_REQ    "Espace requis : %ld Mo | Disponible : %ld Mo\n"
#define MSG_ERREUR_ESPACE "Erreur : Espace insuffisant dans %s (marge de %d Mo requise) !\n"
#define MSG_PREPARATION   "Préparation du profil temporaire...\n"
#define MSG_INJECTION     "Injection des préférences (Browserpass & DuckDuckGo)...\n"
#define MSG_LANCEMENT     "Lancement de %s (Profil : %s)...\n"
#define MSG_FERMETURE     "\nFermeture de Firefox détectée. Nettoyage...\n"
#define MSG_NETTOYAGE_OK  "Profil temporaire supprimé avec succès.\n"
#define MSG_ERR_HOME      "Erreur : Impossible de trouver la variable HOME.\n"
#define MSG_ERR_MODEL     "Erreur lors de la copie du profil modèle.\n"

// --- FONCTIONS ---

long get_free_space_mb(const char *path) {
    struct statvfs stat;
    if (statvfs(path, &stat) != 0) return -1;
    return (stat.f_bsize * stat.f_bavail) / (1024 * 1024);
}

long get_dir_size_mb(const char *dir) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "du -sm \"%s\" | cut -f1", dir);
    FILE *fp = popen(cmd, "r");
    if (!fp) return 0;
    char result[16];
    if (fgets(result, sizeof(result), fp) == NULL) { pclose(fp); return 0; }
    pclose(fp);
    return atol(result);
}

int main() {
    prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
    
    char *home = getenv("HOME");
    if (!home) { 
        fprintf(stderr, MSG_ERR_HOME); 
        return 1; 
    }

    // --- Définition des Chemins ---
    char source_model[1024];
    char temp_base_path[256];
    char temp_profile[1024];
    char cmd[2500];

    // Détection dynamique de l'ID utilisateur pour /run/user/ID (RAM)
    snprintf(temp_base_path, sizeof(temp_base_path), "/run/user/%d", getuid());
    
    // Construction des chemins
    snprintf(source_model, sizeof(source_model), "%s%s", home, SOURCE_SUBPATH);
    snprintf(temp_profile, sizeof(temp_profile), "%s/firefox_plume_%ld", temp_base_path, (long)time(NULL));

    // 1. Vérifications d'espace disque en RAM
    long required = get_dir_size_mb(source_model);
    long available = get_free_space_mb(temp_base_path);
    printf(MSG_ESPACE_REQ, required, available);

    if (available < (required + MARGIN_MB)) {
        fprintf(stderr, MSG_ERREUR_ESPACE, temp_base_path, MARGIN_MB);
        return 1;
    }

    // 2. Création du dossier et Copie du modèle
    printf(MSG_PREPARATION);
    snprintf(cmd, sizeof(cmd), "cp -a \"%s/.\" \"%s/\"", source_model, temp_profile);
    if (system(cmd) != 0) { 
        fprintf(stderr, MSG_ERR_MODEL); 
        return 1; 
    }

    // 3. Nettoyage préventif des fichiers de verrouillage
    snprintf(cmd, sizeof(cmd), "rm -f \"%s/.parentlock\" \"%s/lock\"", temp_profile, temp_profile);
    system(cmd);

    // 4. Lancement de Firefox
    pid_t pid = fork();

    if (pid < 0) {
        perror("Erreur fork");
        return 1;
    }

    if (pid == 0) {
        // --- Processus Enfant ---
        // Forcer X11 si nécessaire et thème sombre
        setenv("GDK_BACKEND", "x11", 1);
        setenv("GTK_THEME", GTK_THEME_VAL, 1);
        
        printf(MSG_LANCEMENT, FIREFOX_BIN, temp_profile);
        
        execlp(FIREFOX_BIN, FIREFOX_BIN, 
               "--profile", temp_profile, 
               "--no-remote", 
               (char *)NULL);
        
        perror("Erreur execlp");
        exit(1);
    } else {
        // --- Processus Parent ---
        wait(NULL); // Attend que Firefox soit fermé
        
        printf(MSG_FERMETURE);
        snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", temp_profile);
        
        if (system(cmd) == 0) {
            printf(MSG_NETTOYAGE_OK);
        }
    }

    return 0;
}