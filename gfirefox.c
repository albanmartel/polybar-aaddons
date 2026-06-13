#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

// --- CONFIGURATION DES CHEMINS ---
#define PROGRAMME_NAME "firefox_plume"
#define SOURCE_SUBPATH "/.config/mozilla/firefox/Modele"
#define TEMP_BASE_PATH "/tmp"
#define FIREFOX_BIN "firefox"
#define GTK_THEME_VAL "Adwaita"
#define MARGIN_MB 50

// --- MESSAGES UTILISATEUR (FRANÇAIS) ---
#define MSG_ESPACE_REQ "Espace requis : %ld Mo | Disponible : %ld Mo\n"
#define MSG_ERREUR_ESPACE                                                      \
  "Erreur : Espace insuffisant dans %s (marge de %d Mo requise) !\n"
#define MSG_PREPARATION "Préparation du profil temporaire...\n"
#define MSG_INJECTION                                                          \
  "Injection des préférences (Browserpass & DuckDuckGo)...\n"
#define MSG_LANCEMENT "Lancement de %s (Profil : %s)...\n"
#define MSG_FERMETURE "\nFermeture de Firefox détectée. Nettoyage...\n"
#define MSG_NETTOYAGE_OK "Profil temporaire supprimé avec succès.\n"
#define MSG_ERR_HOME "Erreur : Impossible de trouver la variable HOME.\n"
#define MSG_ERR_MODEL "Erreur lors de la copie du profil modèle.\n"

// --- FONCTIONS ---

/**
 * @brief Calcule l'espace disque disponible en Mo pour un point de montage
 * donné.
 * @param path Le chemin du répertoire ou point de montage (ex: "/home").
 * @return L'espace libre en Mo, ou -1 en cas d'erreur ou de chemin invalide.
 */
long get_free_space_mb(const char *path) {
  // 1. Sécurité : On vérifie si le pointeur est NULL
  if (path == NULL) {
    fprintf(stderr,
            "Erreur [get_free_space_mb] : Le pointeur de chemin est NULL.\n");
    return -1;
  }

  // 2. Sécurité : On vérifie si la chaîne est vide
  if (strlen(path) == 0) {
    fprintf(stderr,
            "Erreur [get_free_space_mb] : Le chemin fourni est vide.\n");
    return -1;
  }

  struct statvfs stat;

  // 3. Appel système sécurisé
  if (statvfs(path, &stat) != 0) {
    // Renvoie -1 si le chemin n'existe pas (ex: clé USB débranchée entre-temps)
    return -1;
  }

  // Calcul de l'espace libre en Mo
  return (stat.f_bsize * stat.f_bavail) / (1024 * 1024);
}

/**
 * @brief Calcule la taille d'un répertoire en Mo via la commande 'du'.
 * @param dir Le chemin du répertoire à analyser.
 * @return La taille en Mo, ou 0 en cas d'erreur ou de chemin invalide.
 */
long get_dir_size_mb(const char *dir) {
  // 1. Sécurité : On vérifie si le pointeur est NULL
  if (dir == NULL) {
    g_printerr("Erreur [get_dir_size_mb] : Le pointeur de dossier est NULL.\n");
    return 0;
  }

  // 2. Sécurité : On vérifie si la chaîne est vide
  if (strlen(dir) == 0) {
    g_printerr(
        "Erreur [get_dir_size_mb] : Le chemin de dossier fourni est vide.\n");
    return 0;
  }

  // 3. Sécurité Absolue : On échappe le chemin pour empêcher les injections
  // Shell g_shell_quote entoure la chaîne de guillemets simples et gère les
  // caractères dangereux.
  char *safe_dir = g_shell_quote(dir);
  if (safe_dir == NULL)
    return 0;

  // On augmente un peu la taille du tampon pour accueillir les guillemets
  // d'échappement
  char cmd[1024];
  // Note : On enlève les \" autour de %s car g_shell_quote ajoute déjà ses
  // propres protections
  snprintf(cmd, sizeof(cmd), "du -sm %s | cut -f1", safe_dir);

  // On peut libérer safe_dir dès que la commande est formatée dans le tableau
  // 'cmd'
  g_free(safe_dir);

  FILE *fp = popen(cmd, "r");
  if (!fp) {
    g_printerr("Erreur [get_dir_size_mb] : Impossible d'exécuter popen.\n");
    return 0;
  }

  char result[16];
  if (fgets(result, sizeof(result), fp) == NULL) {
    pclose(fp);
    return 0;
  }

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
  snprintf(temp_profile, sizeof(temp_profile), "%s/firefox_plume_%ld",
           temp_base_path, (long)time(NULL));

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
  snprintf(cmd, sizeof(cmd), "cp -a \"%s/.\" \"%s/\"", source_model,
           temp_profile);
  if (system(cmd) != 0) {
    fprintf(stderr, MSG_ERR_MODEL);
    return 1;
  }

  // 3. Nettoyage préventif des fichiers de verrouillage
  snprintf(cmd, sizeof(cmd), "rm -f \"%s/.parentlock\" \"%s/lock\"",
           temp_profile, temp_profile);
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

    execlp(FIREFOX_BIN, FIREFOX_BIN, "--profile", temp_profile, "--no-remote",
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
