# --- Configuration ---
CC = gcc
CFLAGS = -Wall -Wextra -O2
DEBUG_FLAGS = -g -O0 # Nécessaire pour des rapports Valgrind précis
DESTDIR = /usr/local/bin
DESKTOP_DIR = /usr/share/applications
BACKUP_NAME = backup_outils_$(shell date +%Y-%m-%d).tar.gz
LOG_FILE = $(HOME)/log_systeme.txt

# --- Dépendances ---
DEPENDENCIES = zenity xclip wl-copy valgrind

# --- INDIQUE A MAKE DE COMPILER PAR DÉFAUT ---
.DEFAULT_GOAL := all

# Couleurs pour le terminal
BLUE  = \033[1;34m
GREEN = \033[1;32m
CYAN  = \033[1;36m
RESET = \033[0m

# Détection des drapeaux GTK
GTK_FLAGS = $(shell pkg-config --cflags --libs gtk+-3.0 gio-2.0)

# --- Drapeaux spécifiques pour VTE (GTK + Terminal Embarqué) ---
VTE_FLAGS = $(shell pkg-config --cflags --libs gtk+-3.0 vte-2.91 gio-2.0)

# --- DRAPEAU DE DÉTECTION GLIB SEUL ---
GLIB_FLAGS = $(shell pkg-config --cflags --libs glib-2.0)
# --- DRAPEAU DE DECTION LIBCLIPBOARD ---
CLIP_FLAGS = -lclipboard
# --- DRAPEAU DE DECTION ALSA ---
ALSA_FLAGS = -lasound
# --- DRAPEAU DE DETECTION X11 ---
X11_FLAGS = -lX11

# --- Configuration de Valgrind ---
VALGRIND_CMD = valgrind --leak-check=full \
                       --show-leak-kinds=all \
                       --track-origins=yes \
                       --errors-for-leak-kinds=all

# --- Détection Automatique ---
SRCS = $(wildcard src/*.c)

# CORRECTION : On extrait uniquement le nom du binaire sans le préfixe "src/"
# 1. Applications VTE
SRCS_VTE  = $(shell grep -l "#include <vte/vte.h>" $(SRCS) 2>/dev/null)
PROGS_VTE = $(subst src/,,$(SRCS_VTE:.c=))

# 2. Applications GTK Standards
SRCS_GTK_ALL = $(shell grep -l "#include <gtk/gtk.h>" $(SRCS) 2>/dev/null)
PROGS_GTK     = $(filter-out $(PROGS_VTE), $(subst src/,,$(SRCS_GTK_ALL:.c=)))

# 3. Applications X11
SRCS_X11_ALL = $(shell grep -l "#include <X11/Xlib.h>" $(SRCS) 2>/dev/null)
PROGS_X11     = $(filter-out $(PROGS_VTE) $(PROGS_GTK), $(subst src/,,$(SRCS_X11_ALL:.c=)))

# 4. Applications ALSA
SRCS_ALSA_ALL = $(shell grep -l "#include <alsa/asoundlib.h>" $(SRCS) 2>/dev/null)
PROGS_ALSA     = $(filter-out $(PROGS_VTE) $(PROGS_GTK) $(PROGS_X11), $(subst src/,,$(SRCS_ALSA_ALL:.c=)))

# 5. Applications Libclipboard
SRCS_CLIP_ALL = $(shell grep -l "#include <libclipboard.h>" $(SRCS) 2>/dev/null)
PROGS_CLIP     = $(filter-out $(PROGS_VTE) $(PROGS_GTK) $(PROGS_X11) $(PROGS_ALSA), $(subst src/,,$(SRCS_CLIP_ALL:.c=)))

# 6. Applications GLib Seules
SRCS_GLIB_ALL = $(shell grep -l "#include <glib.h>" $(SRCS) 2>/dev/null)
PROGS_GLIB    = $(filter-out $(PROGS_VTE) $(PROGS_GTK) $(PROGS_X11) $(PROGS_ALSA) $(PROGS_CLIP), $(subst src/,,$(SRCS_GLIB_ALL:.c=)))

# 7. Fichiers simples (Tout ce qui reste)
ALL_SPECIAL_PROGS = $(PROGS_VTE) $(PROGS_GTK) $(PROGS_X11) $(PROGS_ALSA) $(PROGS_CLIP) $(PROGS_GLIB)
ALL_TOTAL_PROGS   = $(subst src/,,$(SRCS:.c=))
PROGS_SIMPLE      = $(filter-out $(ALL_SPECIAL_PROGS), $(ALL_TOTAL_PROGS))

# Liste finale propre, triée et garantie sans aucun doublon
ALL_PROGS = $(sort $(PROGS_VTE) $(PROGS_GTK) $(PROGS_GLIB) $(PROGS_ALSA) $(PROGS_CLIP) $(PROGS_X11) $(PROGS_SIMPLE))

# --- Aide ---
help:
	@echo -e "$(BLUE)🛠️  Makefile pour outils Système Alban$(RESET)"
	@echo ""
	@echo -e "$(GREEN)Commandes de gestion :$(RESET)"
	@echo "  make                 : Compile tous les programmes localement"
	@echo "  make rebuild         : Nettoie et recompile tout de zéro"
	@echo "  make test            : Vérifie si tous les binaires sont prêts"
	@echo "  make valgrind        : Lance un test de fuite de mémoire interactif"
	@echo "  make install         : Installe les binaires dans $(DESTDIR)"
	@echo "  make clean           : Supprime les binaires locaux et le dossier shortcuts"
	@echo "  make uninstall       : Désinstalle binaires et raccourcis du système"
	@echo ""
	@echo -e "$(GREEN)Gestion des Raccourcis Desktop :$(RESET)"
	@echo "  make desktop         : Crée les .desktop localement (./shortcuts)"
	@echo "  make desktop-install : Installe les raccourcis dans le système (SUDO)"
	@echo "  make desktop-clean   : Supprime le dossier local ./shortcuts"
	@echo ""
	@echo -e "$(GREEN)Commandes utilitaires :$(RESET)"
	@echo "  make backup          : Archive le code source (.tar.gz)"
	@echo "  make logs            : Affiche les 10 dernières lignes du journal"
	@echo ""
	@echo -e "$(CYAN)Programmes détectés :$(RESET) [$(ALL_PROGS)]"

# --- Vérification des dépendances ---
check-deps:
	@echo -e "$(CYAN)🔍 Vérification des dépendances...$(RESET)"
	@for cmd in $(DEPENDENCIES); do \
		if ! command -v $$cmd &> /dev/null; then \
			echo -e "$(RED)❌ Erreur : $$cmd n'est pas installé.$(RESET)"; \
			exit 1; \
		fi; \
	done
	@if [ ! -f /usr/include/libclipboard.h ] && [ ! -f /usr/local/include/libclipboard.h ]; then \
		echo -e "$(RED)❌ Erreur : libclipboard.h non trouvé. Installez libclipboard-git (AUR).$(RESET)"; \
		exit 1; \
	fi
	@echo -e "$(GREEN)✅ Toutes les dépendances sont présentes.$(RESET)"
	

check-libclip:
	@ldconfig -p | grep libclipboard > /dev/null || (echo -e "$(RED)❌ Erreur: libclipboard n'est pas installée. Utilisez 'yay -S libclipboard-git'$(RESET)"; exit 1)

# --- Rebuild (Forcer la recompilation) ---
rebuild: clean all

# --- Sauvegarde ---
backup: clean
	@echo "📦 Création de l'archive $(BACKUP_NAME)..."
	tar -czf $(BACKUP_NAME) src/ Makefile
	@echo "✅ Sauvegarde terminée."

# --- Journalisation ---
logs:
	@echo -e "$(CYAN)📜 Dernières entrées du journal ($(LOG_FILE)) :$(RESET)"
	@if [ -f $(LOG_FILE) ]; then tail -n 10 $(LOG_FILE); else echo "⚠️ Aucun log trouvé."; fi

# --- Règles de Compilation ---
all: check-libclip check-deps $(ALL_PROGS)

# CORRECTION : La règle dit "Prend la cible '%' (ex: net-doctor) et cherche sa source dans 'src/%.c' (ex: src/net-doctor.c)"
$(PROGS_SIMPLE): %: src/%.c
	@echo -e "$(CYAN)🛠️  Compilation simple :$(RESET) $<"
	$(CC) $(CFLAGS) $< -o $@

$(PROGS_VTE): %: src/%.c
	@echo -e "$(BLUE)🖥️  Compilation GTK+ avec Terminal VTE :$(RESET) $<"
	$(CC) $(CFLAGS) $< -o $@ $(VTE_FLAGS)

$(PROGS_GTK): %: src/%.c
	@echo -e "$(BLUE)🎨 Compilation GTK+ :$(RESET) $<"
	$(CC) $(CFLAGS) $< -o $@ $(GTK_FLAGS)

$(PROGS_CLIP): %: src/%.c
	@echo -e "$(GREEN)📋 Compilation avec libclipboard :$(RESET) $<"
	$(CC) $(CFLAGS) $< -o $@ $(CLIP_FLAGS)

$(PROGS_ALSA): %: src/%.c
	@echo -e "$(CYAN)🔊 Compilation ALSA :$(RESET) $<"
	$(CC) $(CFLAGS) $< -o $@ $(ALSA_FLAGS)

$(PROGS_X11): %: src/%.c
	@echo -e "$(CYAN)🖥️  Compilation X11 :$(RESET) $<"
	$(CC) $(CFLAGS) $< -o $@ $(X11_FLAGS)

$(PROGS_GLIB): %: src/%.c
	@echo -e "$(BLUE)⚙️  Compilation GLib seule :$(RESET) $<"
	$(CC) $(CFLAGS) $< -o $@ $(GLIB_FLAGS)

# --- Commande de Test Standard ---
test: all
	@echo -e "$(GREEN)🧪 Vérification de l'intégrité...$(RESET)"
	@for prog in $(ALL_PROGS); do \
		if [ -x ./$$prog ]; then \
			echo -e "✅ [READY] ./$$prog"; \
		else \
			echo -e "❌ [ERROR] ./$$prog manquant"; exit 1; \
		fi; \
	done

# --- Analyse interactive de fuite de mémoire (Valgrind) ---
valgrind: check-deps
	@echo -e "$(BLUE)🕵️  Menu de test Valgrind$(RESET)"
	@echo -e "$(CYAN)Programmes disponibles :$(RESET) $(ALL_PROGS)"
	@echo -n "Entrez le nom du programme à analyser : " && read prog; \
	if [ -z "$$prog" ] || [ ! -f "src/$$prog.c" ]; then \
		echo -e "$(RED)❌ Erreur : Le programme '$$prog' n'existe pas dans le dossier src/.$(RESET)"; \
		exit 1; \
	fi; \
	echo -e "$(CYAN)🔄 Recompilation temporaire de $$prog avec les drapeaux de débogage...$(RESET)"; \
	rm -f ./$$prog; \
	$(MAKE) DEBUG="$(DEBUG_FLAGS)" ./$$prog || exit 1; \
	echo -e "$(GREEN)🚀 Lancement de Valgrind sur ./$$prog...$(RESET)"; \
	echo -e "$(BLUE)📝 Le rapport sera écrit dans : valgrind_$$prog.log$(RESET)"; \
	$(VALGRIND_CMD) --log-file=valgrind_$$prog.log ./$$prog; \
	echo -e "$(GREEN)🏁 Test terminé. Restauration de la compilation classique...$(RESET)"; \
	rm -f ./$$prog; \
	$(MAKE) ./$$prog

# --- Gestion des Raccourcis Desktop ---

desktop:
	@mkdir -p shortcuts
	@for prog in $(PROGS_GTK); do \
		echo "[Desktop Entry]" > shortcuts/$$prog.desktop; \
		echo "Name=$$prog" >> shortcuts/$$prog.desktop; \
		echo "Exec=$(DESTDIR)/$$prog" >> shortcuts/$$prog.desktop; \
		echo "Icon=utilities-terminal" >> shortcuts/$$prog.desktop; \
		echo "Type=Application" >> shortcuts/$$prog.desktop; \
		echo "Categories=System;Utility;" >> shortcuts/$$prog.desktop; \
	done

desktop-install: desktop
	@echo -e "$(BLUE)🚀 Installation système des raccourcis...$(RESET)"
	sudo cp shortcuts/*.desktop $(DESKTOP_DIR)/
	sudo chmod 644 $(DESKTOP_DIR)/*.desktop
	@$(MAKE) desktop-clean
	@echo "✅ Installation terminée."

desktop-clean:
	@rm -rf shortcuts

desktop-uninstall:
	@for prog in $(PROGS_GTK); do \
		sudo rm -f $(DESKTOP_DIR)/$$prog.desktop; \
	done

# --- Installation et Désinstallation Globale ---

install: all
	@echo -e "$(GREEN)🚀 Installation des binaires dans $(DESTDIR)...$(RESET)"
	sudo install -m 755 $(ALL_PROGS) $(DESTDIR)
	@$(MAKE) clean

uninstall: desktop-uninstall
	@echo -e "$(BLUE)🗑️  Désinstallation des binaires...$(RESET)"
	@for prog in $(ALL_PROGS); do \
		sudo rm -f $(DESTDIR)/$$prog; \
	done
	@echo "✅ Système propre."

clean: desktop-clean
	@echo "🧹 Nettoyage des binaires locaux..."
	rm -f $(ALL_PROGS)

.PHONY: all install uninstall clean test desktop desktop-install desktop-clean desktop-uninstall help backup logs rebuild
