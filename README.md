# Projet Polybar_aaddons

### Pourquoi ce projet ?

Ces outils ont été développés dans le but de créer un environnement
**Arch Linux + Openbox** minimaliste mais puissant, où chaque module est
optimisé pour consommer le moins de ressources possible tout en offrant
une interface utilisateur cohérente.

------------------------------------------------------------------------

## Résumé des fonctionnalités des modules Polybar en C. 

Ces outils sont destinés à enrichir et gérer un environnement de bureau léger openbox / polybar et personnalisé sur Arch Linux pour en faire un **écosystème de gestion pour Arch Linux**.

### 1. Gestion du Système et des Paquets

-   **`arch_manager.c`** : Une interface graphique (GTK) permettant de gérer les mises à jour système avec Pacman, ainsi que l'exportation et l'importation de listes de paquets.

-   **`flatpak_manager.c`** : Un outil similaire dédié spécifiquement aux applications Flatpak, offrant des options pour mettre à jour, exporter ou importer les configurations d'applications.

-   **`pacman_manager.c`** : Une interface dédiée à Pacman. Contrairement à `arch_manager`, elle est plus focalisée sur les fonctions essentielles : mise à jour complète du système, export de la liste des paquets installés et import/réinstallation massive.

-   **`yay_manager.c`** : Une interface graphique dédiée à `yay` (Yet Another Yogurt). Elle permet de gérer spécifiquement les paquets provenant de l'**AUR (Arch User Repository)** : mise à jour, export de la liste des paquets installés et réimportation automatique.


### 2. Interface et Productivité

-   **`tasklist.c`** : Un module complexe pour Polybar qui agit comme une barre des tâches. Il utilise `wmctrl` pour lister les fenêtres ouvertes, détecte la fenêtre active, et attribue des icônes spécifiques à chaque application en fonction d'un fichier de configuration local (`icons.list`).

-   **`selectDate_libcb.c`** : La version C de votre script de calendrier. Il utilise `libclipboard` pour copier une date
    sélectionnée (via Zenity) directement dans le presse-papier, avec une gestion de la date du jour par défaut.

-   **`polybar_date.c`** : Un module d'affichage de la date et de l'heure en français. Il utilise des codes de formatage spécifiques à  Polybar (`%{T3}`, `%{O-60}`) pour superposer élégamment l'heure sur la date avec différentes tailles de police.


### 3. Configuration du Clavier et Entrée

-   **`kb-layout.c`** : Une application graphique organisée par zones géographiques (Europe, Amériques, Asie, etc.) pour changer rapidement la disposition du clavier (`setxkbmap`). Il inclut une fonction pour sauvegarder la disposition au démarrage.

-   **`kbd_status.c`** : Un utilitaire léger qui détecte et affiche l'état actuel du clavier : la langue utilisée (en majuscules) et l'état des touches Verrouillage Majuscule (**Caps Lock**) et Verrouillage Numérique (**Num Lock**).


### 4. Surveillance des Ressources et du Système

-   **`cpu-tool.c`** : Un programme en ligne de commande qui calcule l'utilisation du processeur en lisant `/proc/stat` à une seconde d'intervalle pour donner un pourcentage de charge précis.

-   **`ram-tool.c`** : Lit `/proc/meminfo` pour calculer la mémoire vive utilisée. Il affiche le résultat en Giga-octets (Go) avec un formatage fixe (ex: `04.2G`), idéal pour un affichage constant dans une barre de statut.

-   **`ram_monitor.c`** : Contrairement au simple `ram-tool`, ce module est une interface graphique GTK complète. Il analyse les processus actifs via `ps`, calcule leur consommation réelle et affiche un **rapport trié des processus les plus gourmands** en mémoire vive.

-   **`temp-tool.c`** : Un utilitaire léger qui interroge les capteurs thermiques du noyau (`/sys/class/thermal/`). Il extrait la température du processeur et la formate pour un affichage propre (ex: ` 45°C`), idéal pour une barre de statut.

-   **`updates.c`** : Un module de notification pour Polybar. Il exécute `checkupdates` en arrière-plan pour compter le nombre de mises à jour en attente et affiche une icône dynamique (󰚰 pour les mises à jour dispos, 󰄲 si le système est à jour).


#### 5. Diagnostic et Réseau

-   **`net-doctor.c`** :  Une interface graphique GTK permettant de diagnostiquer l'état du réseau. Il permet d'activer/désactiver le réseau global via nmcli et de réinitialiser le service NetworkManager en cas de problème.

-   **`net-tool.c`** : Analyse `/proc/net/dev` pour mesurer le débit de téléchargement (RX) en temps réel sur l'interface réseau active. Il fournit un affichage formaté avec une icône pour Polybar.

-   **`reseau.c`** : Un outil hybride utilisant **Rofi** pour le menu et **Zenity** pour l'affichage. Il permet de lancer des diagnostics de connexion ou de forcer la reconnexion de toutes les interfaces réseau actives via `nmcli`.

-   **`syslog_report.c`** : Une visionneuse de journaux système (logs) avec interface GTK. Sa particularité est d'appliquer une **coloration syntaxique intelligente** : il met en évidence en rouge et en gras les termes critiques comme "error", "segfault" ou "fail", facilitant grandement le débogage. Il permet l'enregistrement du rapport dans le dossier utilisateur et être communiquer à un support technique.


### 6. Utilitaires de Bureau et Monitoring

-   **`gdesktop.c`** : Un gestionnaire de bureau qui permet d'afficher des icônes d'applications sur le bureau GTK, gère leur positionnement par "glisser-déposer" et sauvegarde ces coordonnées dans un fichier de configuration (`.config/gdesktop/gdesktop_pos.conf`).


-   **`help_center.c`** : Un centre d'aide centralisé permettant de rechercher des pages de manuel (**man pages**) ou d'effectuer des recherches sur le web via différents moteurs (DuckDuckGo, Google, ArchWiki).


### 7. Lanceurs d'Applications Spécialisés

Ces modules utilisent `fork()` et `exec()` pour lancer des logiciels avec des paramètres prédéfinis :

-   **`launcher-qterminal.c`** : Lance le terminal QTerminal avec une taille de fenêtre fixe (900x700).

-   **`launcher_firefox.c`** & **`launcher_librewolf_flatpak.c`** : Ces deux lanceurs sont très avancés. Ils créent un **profil temporaire** dans `/tmp` en clonant un modèle existant.

    -   Ils vérifient l'espace disque disponible avant de se lancer.

    -   Ils lancent le navigateur en mode isolé (`--no-remote`).

    -   Une fois le navigateur fermé, le programme supprime automatiquement le profil temporaire de `/tmp`, garantissant qu'aucune trace de navigation ne reste sur le système.


### 8. Gestion Énergie

-   **`powermenu.c`** : Un menu de déconnexion graphique élégant (GTK + CSS). Il propose les options : Éteindre, redémarrer, Suspendre et Déconnexion. Il est intelligent car il détecte si `lxqt-leave` est présent, sinon il bascule sur les commandes standards `loginctl`.

------------------------------------------------------------------------

### Résumé Technique

Chaque module utilise des bibliothèques standards pour s'intégrer à votre système :

-   **GTK+ 3.0** pour toutes les interfaces graphiques.

-   **`sys/prctl.h`** pour nommer les processus dans le gestionnaire de
    tâches.

-   **Indépendance** : Chaque outil est un binaire léger et autonome.

-   **Sécurité** : Utilisation de `prctl(PR_SET_NAME, ...)` pour que chaque outil soit identifiable par son propre nom dans le gestionnaire de tâches (au lieu de s'appeler simplement "a.out").

-   **Hybride** : Un mélange d'outils CLI (ligne de commande) pour les
    statistiques et d'interfaces GTK pour l'administration

-  **L'extraction de données brutes** : Lecture directe dans `/proc` et `/sys` pour les performances.

-  **L'automatisation système** : Utilisation de `system()` et `popen()` pour piloter les outils standards (`pacman`, `nmcli`, `wmctrl`).

3.  **L'interface utilisateur cohérente** : Un mélange d'affichage texte pour Polybar et d'interfaces GTK3 pour les actions complexes.

------------------------------------------------------------------------

## 🛠️ Dépendances du projet

Pour compiler et utiliser l'ensemble des outils "polybar-aaddons", les composants suivants sont nécessaires.

### 1. Bibliothèques de développement (Build)

Ces paquets sont indispensables pour la **compilation** des sources via `gcc`.

-   **GTK+ 3.0** : Framework pour toutes les interfaces graphiques.

-   **libclipboard** : Gestion native du presse-papier pour le module de calendrier.

-   **pkg-config** : Aide le Makefile à localiser les drapeaux de compilation.

### 2. Outils Système (Runtime)

Ces binaires doivent être présents dans votre `$PATH` pour que les fonctions des modules soient opérationnelles.

------------------------------------------------------------------------

### 📥 Installation rapide (Arch Linux)

Vous pouvez installer la quasi-totalité des dépendances avec les deux commandes suivantes :

**Bibliothèques et outils officiels :**

Bash

    sudo pacman -S base-devel gtk3 networkmanager wmctrl rofi zenity polybar openbox pacman-contrib jgmenu procps-ng xorg-xset xorg-setxkbmap libclipboard

**Outils AUR (via yay) :**

Bash

    yay -S yay flatpak

### 📋 Fichiers de configuration requis

Certains modules s'attendent à trouver des fichiers de ressources spécifiques :

-   `~/.config/polybar/icons.list` : Pour le module `tasklist`.

-   `~/.mozilla/firefox/Modele` : Profil source pour le lanceur Firefox.

> **Note importante :** Un exemplaire du fichier de configuration polybar et de la liste des icônes à placer dans votre dossier de configuration se trouve le dossier documentation 

------------------------------------------------------------------------

### Vérification automatique

Le `Makefile` inclut une cible pour vérifier la présence des outils indispensables :

Bash

    make check-deps

------------------------------------------------------------------------

## 🚀 Installation

Suivez ces étapes pour récupérer, compiler et installer l'ensemble des outils sur votre système.

### 1. Cloner le projet

Bash

    git clone [https://github.com/albanmartel/polybar-aaddons](https://github.com/albanmartel/polybar-aaddons)
    cd votre-depot

### 2. Compilation

Avant de compiler, assurez-vous que les bibliothèques de développement (**GTK3** et **libclipboard**) sont installées.

Bash

    # Compilation de tous les modules
    make

### 3. Installation

Cette commande installe les binaires dans `/usr/local/bin` et génère les raccourcis `.desktop` pour votre menu d'applications.

Bash

    # Installation des binaires et des raccourcis menu
    sudo make install
    sudo make desktop-install

------------------------------------------------------------------------

## 💡 Utilisation

Le projet est conçu de manière hybride : certains modules sont des **applications autonomes** et d'autres sont des **scripts de monitoring** pour votre barre de statut.

### 🖥️ Applications graphiques

Les outils comme `arch_manager`, `pacman_manager` ou `powermenu` peuvent être lancés directement :

-   Depuis votre lanceur d'applications (Rofi, dmenu, ou menu Openbox).

-   En ligne de commande : `nom_du_programme` (ex: `yay_manager`).

### 📊 Intégration dans Polybar

Les modules de "tooling" (`cpu-tool`, `ram-tool`, `net-tool`, `temp-tool`, `updates`) sont optimisés pour être appelés par Polybar à intervalles réguliers.

**Exemple de configuration dans votre `config.ini` Polybar :**

Ini, TOML

    [module/cpu]
    type = custom/script
    exec = /usr/local/bin/cpu-tool
    interval = 2
    format-prefix = "CPU "
    format-prefix-foreground = ${colors.primary}

    [module/updates]
    type = custom/script
    exec = /usr/local/bin/updates
    interval = 600
    label = %output%
    click-left = /usr/local/bin/pacman_manager

------------------------------------------------------------------------

## ⚖️ Licence & Responsabilité

### Licence

Ce projet est sous licence **MIT**. Vous êtes libre de copier, modifier et distribuer ce logiciel, à condition d'inclure la mention de copyright d'origine.

### Déni de responsabilité (Disclaimer)

> **Note importante :** Ce logiciel est fourni "en l'état", sans aucune garantie d'aucune sorte. L'utilisation de ces outils (notamment la gestion des paquets via `pacman` ou `yay`) s'effectue sous votre entière responsabilité. L'auteur ne pourra être tenu pour responsable de toute perte de données ou dysfonctionnement du système résultant de l'exécution de ces modules.

------------------------------------------------------------------------

## 📫 Contact

Si vous avez des questions, des suggestions ou si vous souhaitez simplement échanger sur ce projet, n'hésitez pas à me contacter :

-   **LinkedIn** : [Alban MARTEL](https://www.google.com/search?q=https://www.linkedin.com/in/alban-martel-3a53a227a)

-   **Email** : [alban [dot] f [dot] j [dot] martel [at] gmail [dot] com](mailto:alban [dot] f [dot] j [dot] martel [at] gmail [dot] com)

------------------------------------------------------------------------
