#include <arpa/inet.h>
#include <glib.h>
#include <glib/gstrfuncs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <unistd.h>

// --- NOM PROGRAMME ---
#define PROGRAMME_NAME "reseau"
#define PUBLIC_DNS_IP "1.1.1.1"
#define PUBLIC_DNS_DOMAINE "free.fr"

// --- FONCTIONS ---

/**
 * @brief Vérifie de manière instantanée si le système possède une route réseau
 * active vers une IP spécifique sans envoyer de paquets.
 * @param ip_target L'adresse IP brute à tester (ex: "1.1.1.1" ou "8.8.8.8"). Ne
 * doit pas être NULL.
 * @return 1 si une interface locale est capable de router vers cette IP, 0
 * sinon.
 */
int is_network_connected(const char *ip_target) {
  // 1. SÉCURITÉ : Vérification du pointeur NULL et de la chaîne vide
  if (ip_target == NULL || strlen(ip_target) == 0) {
    fprintf(stderr,
            "Erreur [is_network_connected] : L'IP cible est NULL ou vide.\n");
    return 0;
  }

  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("Erreur [is_network_connected] : Impossible de créer le socket");
    return 0;
  }

  struct sockaddr_in serv;
  memset(&serv, 0, sizeof(serv));
  serv.sin_family = AF_INET;
  serv.sin_port = htons(53); // Port standard (DNS), requis pour la structure
                             // mais non requêté en UDP

  // 2. SÉCURITÉ : Conversion et validation stricte du format de l'IP
  // inet_pton renvoie 1 si le format de l'IP est valide, 0 ou -1 sinon.
  if (inet_pton(AF_INET, ip_target, &serv.sin_addr) != 1) {
    fprintf(stderr,
            "Erreur [is_network_connected] : L'adresse IP '%s' a un format "
            "invalide.\n",
            ip_target);
    close(sock);
    return 0;
  }

  // L'appel système connect() sur un socket UDP ne transmet AUCUN paquet sur le
  // réseau. Il interroge la table de routage du noyau Linux.
  int result = connect(sock, (const struct sockaddr *)&serv, sizeof(serv));

  close(sock);

  // Si result == 0, l'OS confirme qu'une interface est prête à router vers
  // cette IP.
  return (result == 0) ? 1 : 0;
}

/**
 * @brief ÉTAPE 2 : Teste la connectivité vers l'extérieur via une IP publique
 * brute.
 * @param ip_target L'adresse IP à tester (ex: "8.8.8.8"). Ne doit pas être
 * NULL.
 * @return 1 si l'IP répond, 0 en cas d'échec ou de timeout.
 */
int test_ip_link(const char *ip_target) {
  if (ip_target == NULL || strlen(ip_target) == 0) {
    fprintf(stderr, "Erreur [test_ip_link] : L'IP cible est NULL ou vide.\n");
    return 0;
  }

  char command[128];
  // -c 1 : 1 seul paquet / -W 2 : Timeout de 2 secondes
  snprintf(command, sizeof(command), "ping -c 1 -W 2 %s > /dev/null 2>&1",
           ip_target);

  return (system(command) == 0) ? 1 : 0;
}

/**
 * @brief ÉTAPE 3 : Teste la résolution de noms de domaine (DNS).
 * @param domain_target Le domaine à tester (ex: "google.fr"). Ne doit pas être
 * NULL.
 * @return 1 si le domaine est résolu et répond, 0 en cas d'erreur DNS.
 */
int test_dns_resolution(const char *domain_target) {
  if (domain_target == NULL || strlen(domain_target) == 0) {
    fprintf(
        stderr,
        "Erreur [test_dns_resolution] : Le domaine cible est NULL ou vide.\n");
    return 0;
  }

  char command[128];
  // -c 1 : 1 seul paquet / -W 2 : Timeout de 2 secondes
  snprintf(command, sizeof(command), "ping -c 1 -W 2 %s > /dev/null 2>&1",
           domain_target);

  return (system(command) == 0) ? 1 : 0;
}

/**
 * @brief Orchestre la vérification Internet globale avec une IP et un domaine
 * dynamiques.
 * @param ip L'adresse IP publique de référence (ex: "8.8.8.8" ou "1.1.1.1"). Ne
 * doit pas être NULL.
 * @param domain Le domaine de référence pour le test DNS (ex: "google.fr"). Ne
 * doit pas être NULL.
 */
void verifier_internet(const char *ip, const char *domain) {
  // 1. SÉCURITÉ : Validation des paramètres d'entrée
  if (ip == NULL || strlen(ip) == 0 || domain == NULL || strlen(domain) == 0) {
    fprintf(stderr, "Erreur [verifier_internet] : Les paramètres IP ou Domaine "
                    "sont invalides (NULL ou vides).\n");
    return;
  }

  printf("📊 Analyse de la connexion Internet (Cibles : %s / %s)...\n", ip,
         domain);

  // 2. ÉTAPE 1 : Test de la route locale (Fast-Pass)
  if (!is_network_connected(ip)) {
    printf(
        "  ❌ Pas de réseau (Aucune interface locale capable de joindre %s).\n",
        ip);
    return;
  }
  printf("  ⚙️ Interface locale : Active.\n");

  // 3. ÉTAPE 2 : Test du ping IP externe
  printf("  📡 Test de liaison IP (%s)...\n", ip);
  if (test_ip_link(ip)) {
    printf("    ✅ Liaison IP fonctionnelle.\n");

    // 4. ÉTAPE 3 : Test du ping DNS externe
    printf("  🔎 Test de résolution DNS (%s)...\n", domain);
    if (test_dns_resolution(domain)) {
      printf("    🎉 Internet OK (Accès total et DNS fonctionnel).\n");
    } else {
      printf("    ⚠️ Erreur DNS (Liaison IP OK mais impossible de traduire "
             "'%s').\n",
             domain);
    }
  } else {
    printf("  ❌ Échec de la liaison extérieure (L'IP %s ne répond pas. "
           "Problème de FAI/Box).\n",
           ip);
  }
}

/**
 * @brief Vérifie si un programme est installé sur le système, qu'il soit
 * spécifié par son nom seul (recherche dans le PATH) ou par un chemin absolu.
 * @param name_or_path Le nom de l'exécutable ou son chemin complet. Ne doit pas
 * être NULL.
 * @return 1 (Vrai) si le programme est présent et exécutable, 0 (Faux) sinon.
 */
int is_program_installed(const char *name_or_path) {
  // 1. Sécurité : Validation du paramètre d'entrée
  if (name_or_path == NULL || strlen(name_or_path) == 0) {
    fprintf(stderr, "Erreur [is_program_installed] : Le paramètre fourni est "
                    "NULL ou vide.\n");
    return 0;
  }

  // 2. Cas n°1 : C'est un chemin absolu ou relatif explicite (commence par '/'
  // ou './' ou '../')
  if (name_or_path[0] == '/' || g_str_has_prefix(name_or_path, "./") ||
      g_str_has_prefix(name_or_path, "../")) {
    // F_OK = Le fichier existe | X_OK = L'utilisateur a les droits d'exécution
    if (access(name_or_path, F_OK | X_OK) == 0) {
      return 1; // Trouvé et exécutable !
    }
    return 0; // Introuvable ou non exécutable
  }

  // 3. Cas n°2 : C'est un nom simple, on scanne tout le $PATH du système
  char *discovered_path = g_find_program_in_path(name_or_path);

  if (discovered_path != NULL) {
    // GLib a trouvé le binaire dans le PATH et a vérifié qu'il était exécutable
    g_free(
        discovered_path); // Règle d'or : On libère la mémoire allouée par GLib
    return 1;             // Vrai
  }

  // Le programme n'a été trouvé nulle part
  return 0; // Faux
}

/**
 * @brief Force la déconnexion puis la reconnexion de toutes les interfaces
 * réseau actuellement actives via NetworkManager.
 */
void forcer_reconnexion(void) {
  printf("Début du processus de reconnexion...\n");
  printf("---------------------------------------------------------------------"
         "-\n");

  // 1. SÉCURITÉ : Vérification de la présence des outils système indispensables
  if (!is_program_installed("nmcli")) {
    fprintf(stderr, "Erreur Critique [forcer_reconnexion] : 'nmcli' "
                    "(NetworkManager) n'est pas installé.\n");
    return;
  }
  if (!is_program_installed("grep") || !is_program_installed("cut")) {
    fprintf(stderr, "Erreur Critique [forcer_reconnexion] : Les utilitaires "
                    "système 'grep' ou 'cut' sont manquants.\n");
    return;
  }

  // Récupérer les interfaces connectées via nmcli
  FILE *fp = popen("nmcli -t -f DEVICE,STATE device status | grep ':connected' "
                   "| cut -d: -f1",
                   "r");
  if (fp == NULL) {
    perror("Erreur [forcer_reconnexion] : Impossible d'exécuter la commande de "
           "statut");
    return;
  }

  char interface[64];
  int count = 0;
  // Liste des caractères autorisés pour un nom d'interface Linux (lettres,
  // chiffres, points, tirets, underscores)
  const char *allowed_chars =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-_";

  while (fgets(interface, sizeof(interface), fp) != NULL) {
    interface[strcspn(interface, "\n")] = 0; // Nettoyer le saut de ligne

    // 2. SÉCURITÉ : On ignore l'interface si son nom est vide ou corrompu
    if (strlen(interface) == 0) {
      continue;
    }

    // 3. SÉCURITÉ ABSOLUE : Validation du nom de l'interface pour bloquer toute
    // injection Shell
    if (strspn(interface, allowed_chars) != strlen(interface)) {
      fprintf(stderr,
              "Avertissement [forcer_reconnexion] : Interface '%s' ignorée car "
              "elle contient des caractères invalides.\n",
              interface);
      continue;
    }

    printf("🔄 Traitement de l'interface : %s\n", interface);

    char cmd[128];

    // Déconnexion
    snprintf(cmd, sizeof(cmd), "nmcli device disconnect '%s' > /dev/null 2>&1",
             interface);
    system(cmd);

    sleep(1);

    // Reconnexion
    snprintf(cmd, sizeof(cmd), "nmcli device connect '%s' > /dev/null 2>&1",
             interface);
    if (system(cmd) == 0) {
      printf("  ✅ %s reconnectée.\n", interface);
    } else {
      printf("  ❌ Échec pour %s.\n", interface);
    }
    count++;
  }

  if (count == 0) {
    printf("⚠️ Aucune interface connectée trouvée.\n");
  }

  pclose(fp);
  printf("---------------------------------------------------------------------"
         "-\nTerminé.\n");
}

/**
 * @brief Lance l'interface graphique réseau (Rofi + Zenity) pour diagnostiquer
 * ou forcer la reconnexion des interfaces réseau.
 * @param ip L'adresse IP publique de référence pour les tests (ex: "8.8.8.8").
 * Ne doit pas être NULL.
 * @param domain Le domaine de référence pour le test DNS (ex: "google.fr"). Ne
 * doit pas être NULL.
 */
void run_network_gui_menu(const char *ip, const char *domain) {
  // 1. SÉCURITÉ : Validation des paramètres d'entrée
  if (ip == NULL || strlen(ip) == 0 || domain == NULL || strlen(domain) == 0) {
    fprintf(stderr, "Erreur [run_network_gui_menu] : Les paramètres IP ou "
                    "Domaine sont invalides (NULL ou vides).\n");
    return;
  }

  // 2. SÉCURITÉ : Vérification de la présence de Rofi et Zenity
  if (!is_program_installed("rofi")) {
    fprintf(stderr, "Erreur Critique [run_network_gui_menu] : 'rofi' n'est pas "
                    "installé sur le système.\n");
    return;
  }
  if (!is_program_installed("zenity")) {
    fprintf(stderr, "Erreur Critique [run_network_gui_menu] : 'zenity' n'est "
                    "pas installé sur le système.\n");
    return;
  }

  // On définit le nom du processus dans le gestionnaire de tâches
  prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);

  // Menu Rofi
  const char *options =
      "1. Vérifier la connexion\n2. Reconnecter les interfaces";
  char rofi_cmd[512];
  snprintf(rofi_cmd, sizeof(rofi_cmd),
           "echo -e \"%s\" | rofi -dmenu -p \"Réseau\" -width 30 -lines 2",
           options);

  FILE *pipe_rofi = popen(rofi_cmd, "r");
  if (pipe_rofi == NULL) {
    perror("Erreur [run_network_gui_menu] : Impossible d'ouvrir le pipe vers "
           "rofi");
    return;
  }

  char choice[128];
  if (fgets(choice, sizeof(choice), pipe_rofi) != NULL) {
    pclose(pipe_rofi); // Fermer le pipe de rofi dès qu'on a le choix

    // 3. Détermination du titre et de l'action selon le choix
    const char *zenity_title = "Action Réseau";
    int action_type = 0; // 1 = Vérifier, 2 = Reconnecter

    if (strstr(choice, "Vérifier") != NULL) {
      zenity_title = "Diagnostic Réseau";
      action_type = 1;
    } else if (strstr(choice, "Reconnecter") != NULL) {
      zenity_title = "Tentative de reconnexion réseau";
      action_type = 2;
    } else {
      // Choix inconnu ou l'utilisateur a tapé n'importe quoi
      return;
    }

    // 4. Construction de la commande Zenity dynamique
    char zenity_cmd[256];
    snprintf(zenity_cmd, sizeof(zenity_cmd),
             "zenity --text-info --title='%s' --width=450 --height=300",
             zenity_title);

    // Ouverture du pipe vers Zenity avec la commande personnalisée
    FILE *zenity_pipe = popen(zenity_cmd, "w");
    if (zenity_pipe == NULL) {
      perror("Erreur [run_network_gui_menu] : Impossible d'ouvrir le pipe vers "
             "zenity");
      return;
    }

    // Sauvegarde de STDOUT pour pouvoir le restaurer plus tard
    int stdout_backup = dup(STDOUT_FILENO);
    int zenity_fd = fileno(zenity_pipe);

    if (stdout_backup != -1 && zenity_fd != -1) {
      // Redirection de stdout vers Zenity
      if (dup2(zenity_fd, STDOUT_FILENO) != -1) {

        // Exécution de l'action pré-déterminée
        if (action_type == 1) {
          verifier_internet(ip, domain);
        } else if (action_type == 2) {
          forcer_reconnexion();
        }

        fflush(stdout);
      }

      // Restauration de la sortie standard d'origine
      dup2(stdout_backup, STDOUT_FILENO);
      close(stdout_backup);
    }

    pclose(zenity_pipe);
  } else {
    pclose(pipe_rofi);
  }
}

// --- MAIN ---
/**
 * @brief Point d'entrée principal du programme.
 */
int main(void) {
  // Désormais, c'est au niveau du main() (ou de ton menu GTK principal)
  // que tu choisis tes cibles de test.
  run_network_gui_menu(PUBLIC_DNS_IP, PUBLIC_DNS_DOMAINE);

  return EXIT_SUCCESS;
}
