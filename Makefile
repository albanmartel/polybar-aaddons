# --- Configuration ---
CC = gcc
CFLAGS = -Wall -Wextra -O2
DESTDIR = /usr/local/bin
DESKTOP_DIR = /usr/share/applications
BACKUP_NAME = backup_outils_$(shell date +%Y-%m-%d).tar.gz
LOG_FILE = $(HOME)/log_systeme.txt

# --- Dépendances ---
DEPENDENCIES = zenity xclip wl-copy

# --- INDIQUE A MAKE DE COMPILER PAR DÉFAUT ---
.DEFAULT_GOAL := all

# Couleurs pour le terminal
BLUE  = \033[1;34m
GREEN = \033[1;32m
CYAN  = \033[1;36m
RESET = \033[0m

# Détection des drapeaux GTK
GTK_FLAGS = $(shell pkg-config --cflags --libs gtk+-3.0)
# --- DRAPEAU DE DECTION LIBCLIPBOARD ---
CLIP_FLAGS = -lclipboard

# --- Détection Automatique ---
SRCS = $(wildcard *.c)

# 1. On identifie ceux qui utilisent GTK (qu'ils aient libclipboard ou non)
SRCS_GTK = $(shell grep -l "#include <gtk/gtk.h>" $(SRCS))
PROGS_GTK = $(SRCS_GTK:.c=)

# 2. On identifie ceux qui utilisent libclipboard MAIS qui ne sont pas déjà dans GTK
SRCS_CLIP_ALL = $(shell grep -l "#include <libclipboard.h>" $(SRCS))
PROGS_CLIP = $(filter-out $(PROGS_GTK), $(SRCS_CLIP_ALL:.c=))

# 3. On identifie les fichiers simples (ni GTK, ni CLIP)
SRCS_SIMPLE = $(shell grep -L "#include <gtk/gtk.h>\|#include <libclipboard.h>" $(SRCS))
PROGS_SIMPLE = $(SRCS_SIMPLE:.c=)

# Mettre à jour la liste globale (plus besoin de filter-out ici)
ALL_PROGS = $(PROGS_GTK) $(PROGS_CLIP) $(PROGS_SIMPLE)

# --- Aide ---
help:
	@echo -e "$(BLUE)🛠️  Makefile pour outils Système Alban$(RESET)"
	@echo ""
	@echo -e "$(GREEN)Commandes de gestion :$(RESET)"
	@echo "  make               : Compile tous les programmes localement"
	@echo "  make rebuild       : Nettoie et recompile tout de zéro"
	@echo "  make test          : Vérifie si tous les binaires sont prêts"
	@echo "  make install       : Installe les binaires dans $(DESTDIR)"
	@echo "  make clean         : Supprime les binaires locaux et le dossier shortcuts"
	@echo "  make uninstall     : Désinstalle binaires et raccourcis du système"
	@echo ""
	@echo -e "$(GREEN)Gestion des Raccourcis Desktop :$(RESET)"
	@echo "  make desktop       : Crée les .desktop localement (./shortcuts)"
	@echo "  make desktop-install : Installe les raccourcis dans le système (SUDO)"
	@echo "  make desktop-clean : Supprime le dossier local ./shortcuts"
	@echo ""
	@echo -e "$(GREEN)Commandes utilitaires :$(RESET)"
	@echo "  make backup        : Archive le code source (.tar.gz)"
	@echo "  make logs          : Affiche les 10 dernières lignes du journal"
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
	tar -czf $(BACKUP_NAME) *.c Makefile
	@echo "✅ Sauvegarde terminée."

# --- Journalisation ---
logs:
	@echo -e "$(CYAN)📜 Dernières entrées du journal ($(LOG_FILE)) :$(RESET)"
	@if [ -f $(LOG_FILE) ]; then tail -n 10 $(LOG_FILE); else echo "⚠️ Aucun log trouvé."; fi

# --- Règles de Compilation ---
all: check-libclip check-deps $(ALL_PROGS)

$(PROGS_SIMPLE): %: %.c
	@echo -e "$(CYAN)🛠️  Compilation simple :$(RESET) $<"
	$(CC) $(CFLAGS) $< -o $@

$(PROGS_GTK): %: %.c
	@echo -e "$(BLUE)🎨 Compilation GTK+ :$(RESET) $<"
	$(CC) $(CFLAGS) $< -o $@ $(GTK_FLAGS)

$(PROGS_CLIP): %: %.c
	@echo -e "$(GREEN)📋 Compilation avec libclipboard :$(RESET) $<"
	$(CC) $(CFLAGS) $< -o $@ $(CLIP_FLAGS)

# --- Commande de Test ---
test: all
	@echo -e "$(GREEN)🧪 Vérification de l'intégrité...$(RESET)"
	@for prog in $(ALL_PROGS); do \
		if [ -x ./$$prog ]; then \
			echo -e "✅ [READY] ./$$prog"; \
		else \
			echo -e "❌ [ERROR] ./$$prog manquant"; exit 1; \
		fi \
	done

# --- Gestion des Raccourcis Desktop ---

# 1. Création locale (sans sudo)
desktop:
	@echo -e "$(CYAN)📂 Création locale des fichiers .desktop...$(RESET)"
	@mkdir -p shortcuts
	@for prog in $(PROGS_GTK); do \
		echo "[Desktop Entry]" > shortcuts/$$prog.desktop; \
		echo "Name=$$prog" >> shortcuts/$$prog.desktop; \
		echo "Exec=$(DESTDIR)/$$prog" >> shortcuts/$$prog.desktop; \
		echo "Icon=utilities-terminal" >> shortcuts/$$prog.desktop; \
		echo "Type=Application" >> shortcuts/$$prog.desktop; \
		echo "Categories=System;Utility;" >> shortcuts/$$prog.desktop; \
		echo "✅ Généré : shortcuts/$$prog.desktop"; \
	done

# 2. Installation système (un seul sudo)
desktop-install: desktop
	@echo -e "$(BLUE)🚀 Installation système des raccourcis...$(RESET)"
	sudo cp shortcuts/*.desktop $(DESKTOP_DIR)/
	sudo chmod 644 $(DESKTOP_DIR)/*.desktop
	@$(MAKE) desktop-clean
	@echo "✅ Installation terminée."

desktop-clean:
	@echo "🧹 Nettoyage du dossier local ./shortcuts..."
	@rm -rf shortcuts

desktop-uninstall:
	@echo -e "$(BLUE)🗑️  Suppression des raccourcis système...$(RESET)"
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
