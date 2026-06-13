#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>

// --- NOM PROGRAMME ---
#define PROGRAMME_NAME "powermenu"

/**
 * Exécute la commande appropriée selon l'environnement.
 * Sous Openbox/X11, lxqt-leave est souvent plus fiable pour communiquer avec le
 * session manager. Sous Wayland, loginctl est la norme.
 */
void execute_system_command(const char *action) {
  // 1. Vérification de sécurité immédiate
  if (action == NULL || strlen(action) == 0) {
    fprintf(stderr, "Erreur : action système non spécifiée.\n");
    return;
  }

  char cmd[128] = "";

  // 2. Mapping sécurisé des commandes
  if (strcmp(action, "shutdown") == 0) {
    strcpy(cmd, "systemctl poweroff");
  } else if (strcmp(action, "reboot") == 0) {
    strcpy(cmd, "systemctl reboot");
  } else if (strcmp(action, "suspend") == 0) {
    strcpy(cmd, "systemctl suspend");
  } else if (strcmp(action, "logout") == 0) {
    strcpy(cmd, "loginctl terminate-session self");
  } else {
    // L'action n'est pas reconnue
    fprintf(stderr, "Action inconnue : %s\n", action);
    return;
  }

  // 3. Exécution
  printf("Exécution de l'action : %s (Commande : %s)\n", action, cmd);

  if (system(cmd) == -1) {
    perror("Erreur lors de l'appel à system()");
    return;
  }

  // 4. Fermeture propre de l'interface uniquement si la commande est lancée
  while (gtk_events_pending())
    gtk_main_iteration();
  gtk_main_quit();
}

gboolean confirmer(GtkWindow *parent, const char *message) {
  // 1. Protection contre les pointeurs nuls
  const char *texte_final =
      (message != NULL) ? message : "effectuer cette action";

  // 2. Création avec flags de sécurité
  GtkWidget *dialog = gtk_message_dialog_new(
      parent, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
      GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "Confirmation");

  // Définition du texte secondaire pour un look plus pro
  gtk_message_dialog_format_secondary_text(
      GTK_MESSAGE_DIALOG(dialog), "Voulez-vous vraiment %s ?", texte_final);

  gtk_window_set_title(GTK_WINDOW(dialog), "Système");

  // Pour que la fenêtre soit toujours au-dessus
  gtk_window_set_keep_above(GTK_WINDOW(dialog), TRUE);

  // 3. Exécution et récupération du résultat
  gint result = gtk_dialog_run(GTK_DIALOG(dialog));

  // 4. Destruction systématique
  gtk_widget_destroy(dialog);

  // Retourne TRUE seulement si l'utilisateur a cliqué explicitement sur OUI
  return (result == GTK_RESPONSE_YES);
}

// Callbacks simplifiés
void on_power_off(GtkWidget *widget, gpointer data G_GNUC_UNUSED) {
  GtkWindow *parent = GTK_WINDOW(gtk_widget_get_toplevel(widget));
  if (confirmer(parent, "éteindre l'ordinateur"))
    execute_system_command("shutdown");
}

void on_reboot(GtkWidget *widget, gpointer data G_GNUC_UNUSED) {
  GtkWindow *parent = GTK_WINDOW(gtk_widget_get_toplevel(widget));
  if (confirmer(parent, "redémarrer le système"))
    execute_system_command("reboot");
}

// Correction ici : on ajoute les arguments pour correspondre à GCallback
void on_suspend(GtkWidget *widget G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED) {
  execute_system_command("suspend");
}

void on_logout(GtkWidget *widget, gpointer data G_GNUC_UNUSED) {
  GtkWindow *parent = GTK_WINDOW(gtk_widget_get_toplevel(widget));
  if (confirmer(parent, "quitter la session"))
    execute_system_command("logout");
}

gboolean on_key_press(GtkWidget *widget G_GNUC_UNUSED, GdkEventKey *event,
                      gpointer user_data G_GNUC_UNUSED) {
  // 1. Vérification du pointeur d'événement
  if (event == NULL) {
    return FALSE;
  }

  // 2. Vérification du type d'événement (optionnel mais robuste)
  // On s'assure que c'est bien un appui de touche
  if (event->type != GDK_KEY_PRESS) {
    return FALSE;
  }

  // 3. Logique métier
  if (event->keyval == GDK_KEY_Escape) {
    printf("Touche Échap détectée. Fermeture...\n");
    gtk_main_quit();
    return TRUE; // On indique que l'événement a été traité
  }

  return FALSE; // On laisse passer les autres touches
}

GtkWidget *create_menu_button(const char *label_text, const char *icon_name,
                              GCallback cb) {
  // 1. Vérification des entrées critiques
  if (cb == NULL) {
    g_warning("create_menu_button: Callback est NULL pour le bouton '%s'",
              label_text ? label_text : "inconnu");
    // On ne retourne pas NULL pour éviter de faire planter le parent,
    // mais on pourrait désactiver le bouton plus bas.
  }

  // 2. Création sécurisée des widgets
  GtkWidget *button = gtk_button_new();
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);

  // Si icon_name est NULL, on utilise une icône par défaut pour éviter un crash
  const char *final_icon = (icon_name != NULL) ? icon_name : "image-missing";
  GtkWidget *icon = gtk_image_new_from_icon_name(final_icon, GTK_ICON_SIZE_DND);

  // Si label_text est NULL, on met une chaîne vide
  GtkWidget *label = gtk_label_new(label_text ? label_text : "");

  // 3. Configuration et assemblage
  gtk_container_set_border_width(GTK_CONTAINER(button), 5);

  // Alignement du label à gauche pour plus de propreté
  gtk_label_set_xalign(GTK_LABEL(label), 0.0);

  gtk_box_pack_start(GTK_BOX(box), icon, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE,
                     5); // TRUE pour que le label prenne l'espace

  gtk_container_add(GTK_CONTAINER(button), box);

  // 4. Connexion du signal uniquement si le callback existe
  if (cb != NULL) {
    g_signal_connect(button, "clicked", cb, NULL);
  } else {
    gtk_widget_set_sensitive(button,
                             FALSE); // Désactive le bouton si pas de callback
  }

  return button;
}

int main(int argc, char *argv[]) {
  prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);
  gtk_init(&argc, &argv);

  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "Menu Système");
  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  gtk_container_set_border_width(GTK_CONTAINER(window), 20);
  gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

  // Style CSS
  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_data(provider,
                                  "button { min-width: 280px; padding: 12px; "
                                  "border-radius: 6px; font-size: 14px; }"
                                  "window { background-color: #2e3440; }",
                                  -1, NULL);
  gtk_style_context_add_provider_for_screen(
      gdk_screen_get_default(), GTK_STYLE_PROVIDER(provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press), NULL);
  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_add(GTK_CONTAINER(window), vbox);

  gtk_box_pack_start(GTK_BOX(vbox),
                     create_menu_button("Éteindre", "system-shutdown",
                                        G_CALLBACK(on_power_off)),
                     TRUE, TRUE, 0);
  gtk_box_pack_start(
      GTK_BOX(vbox),
      create_menu_button("Redémarrer", "system-reboot", G_CALLBACK(on_reboot)),
      TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox),
                     create_menu_button("Mise en veille", "system-suspend",
                                        G_CALLBACK(on_suspend)),
                     TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox),
                     create_menu_button("Déconnexion", "system-log-out",
                                        G_CALLBACK(on_logout)),
                     TRUE, TRUE, 0);

  gtk_widget_show_all(window);
  gtk_main();
  return 0;
}
