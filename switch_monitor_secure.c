#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

// Fonction sécurisée pour envoyer un message d'état au Window Manager
void set_window_state(Display *display, Window win, long action,
                      const char *state1, const char *state2) {
  // 1. VÉRIFICATION DE 'display'
  // Si le pointeur vers le serveur X est NULL, toute fonction Xlib va provoquer
  // un SegFault.
  if (!display) {
    fprintf(stderr,
            "[Erreur Sécurité] set_window_state: 'display' est NULL.\n");
    return;
  }

  // 2. VÉRIFICATION DE 'win'
  // Sous X11, 'None' (0) représente une absence de fenêtre.
  // On évite d'envoyer un message global potentiellement perturbateur.
  if (win == None) {
    fprintf(
        stderr,
        "[Erreur] set_window_state: ID de fenêtre 'win' invalide (None).\n");
    return;
  }

  // 3. VÉRIFICATION DE 'action'
  // La spécification EWMH (_NET_WM_STATE) n'accepte QUE 0 (remove), 1 (add) ou
  // 2 (toggle). Toute autre valeur est un comportement indéfini ou une
  // tentative d'anomalie.
  if (action < 0 || action > 2) {
    fprintf(stderr,
            "[Erreur] set_window_state: 'action' (%ld) hors limites (doit être "
            "0, 1 ou 2).\n",
            action);
    return;
  }

  // 4. VÉRIFICATION DE 'state1'
  // C'est le paramètre critique : il DOIT exister, ne pas être vide, et avoir
  // une taille raisonnable pour éviter les surprises (un nom d'atome X11 fait
  // rarement plus de 100 caractères).
  if (!state1 || state1[0] == '\0') {
    fprintf(stderr,
            "[Erreur Sécurité] set_window_state: 'state1' est NULL ou vide.\n");
    return;
  }
  if (strnlen(state1, 256) >= 256) {
    fprintf(stderr, "[Erreur Sécurité] set_window_state: 'state1' est trop "
                    "long (Buffer Overflow suspect).\n");
    return;
  }

  // 5. VÉRIFICATION DE 'state2' (Optionnel, mais s'il existe, on le valide)
  if (state2) {
    if (state2[0] == '\0') {
      fprintf(stderr,
              "[Erreur Sécurité] set_window_state: 'state2' est vide.\n");
      return;
    }
    if (strnlen(state2, 256) >= 256) {
      fprintf(stderr,
              "[Erreur Sécurité] set_window_state: 'state2' est trop long.\n");
      return;
    }
  }

  // --- SÉCURITÉ OK : DÉBUT DU TRAITEMENT ---

  XEvent xev;
  Atom atoms[2];

  // XInternAtom peut techniquement retourner 'None' si l'atome n'existe pas,
  // mais avec 'False', il le créera s'il n'existe pas.

  atoms[0] = XInternAtom(display, state1, False);
  atoms[1] = state2 ? XInternAtom(display, state2, False) : None;

  Atom net_wm_state = XInternAtom(display, "_NET_WM_STATE", False);

  long mask = SubstructureRedirectMask | SubstructureNotifyMask;

  // Initialisation propre de la structure pour éviter les fuites de mémoire
  // résiduelle de la pile
  memset(&xev, 0, sizeof(xev));

  xev.type = ClientMessage;
  xev.xclient.window = win;
  xev.xclient.message_type = net_wm_state;
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = action; // 0 = remove, 1 = add
  xev.xclient.data.l[1] = atoms[0];
  xev.xclient.data.l[2] = atoms[1];
  xev.xclient.data.l[3] = 1; // Source: application légitime
  xev.xclient.data.l[4] = 0;

  // XSendEvent retourne 0 en cas d'échec d'envoi
  if (XSendEvent(display, DefaultRootWindow(display), False, mask, &xev) == 0) {
    fprintf(stderr,
            "[Erreur X11] Impossible d'envoyer l'événement XSendEvent.\n");
  } else {
    XFlush(display);
  }
}

int main(void) {
  // 1. SÉCURITÉ : Interdire l'exécution en Root / Setuid
  // Un utilitaire graphique n'a JAMAIS besoin d'être root.
  // Si un attaquant tente de lui donner des privilèges via setuid, le programme
  // coupe court.
  if (getuid() == 0 || geteuid() == 0) {
    fprintf(stderr, "Erreur de sécurité : Ce programme ne doit pas être "
                    "exécuté en tant que root.\n");
    return EXIT_FAILURE;
  }

  // 2. SÉCURITÉ : Vérifier la variable d'environnement DISPLAY
  char *display_env = getenv("DISPLAY");
  if (!display_env || display_env[0] == '\0') {
    fprintf(stderr, "Erreur : La variable DISPLAY n'est pas définie. "
                    "Environnement X11 requis.\n");
    return EXIT_FAILURE;
  }

  // Connexion au serveur X
  Display *display = XOpenDisplay(NULL);
  if (!display) {
    fprintf(stderr, "Erreur : Impossible d'ouvrir le display X11.\n");
    return EXIT_FAILURE;
  }

  // 3. Identifier la fenêtre active de manière robuste
  Atom active_atom = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
  Atom actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  unsigned char *prop = NULL;
  Window win_id = None;

  if (XGetWindowProperty(display, DefaultRootWindow(display), active_atom, 0, 1,
                         False, XA_WINDOW, &actual_type, &actual_format,
                         &nitems, &bytes_after, &prop) == Success &&
      prop) {
    win_id = *(Window *)prop;
    XFree(prop);
  }

  // Si aucune fenêtre n'est active ou si c'est la fenêtre racine (le bureau
  // lui-même)
  if (win_id == None || win_id == 0 || win_id == DefaultRootWindow(display)) {
    XCloseDisplay(display);
    return EXIT_SUCCESS; // Quitte proprement sans planter
  }

  // 4. CASSER LES LIENS (Démaximiser)
  set_window_state(display, win_id, 0, "_NET_WM_STATE_MAXIMIZED_VERT",
                   "_NET_WM_STATE_MAXIMIZED_HORZ");

  XResizeWindow(display, win_id, 500, 500);
  XFlush(display);
  usleep(100000); // 0.1s

  // 5. Récupérer la géométrie et traduire les coordonnées de manière sécurisée
  Window root_return;
  int x, y;
  unsigned int width, height, border_width, depth;

  // XGetGeometry peut échouer si la fenêtre est fermée brusquement pendant
  // l'exécution
  XWindowAttributes wattr;
  if (!XGetWindowAttributes(display, win_id, &wattr) ||
      wattr.map_state != IsViewable) {
    XCloseDisplay(display);
    return EXIT_FAILURE;
  }

  XGetGeometry(display, win_id, &root_return, &x, &y, &width, &height,
               &border_width, &depth);

  int root_x, root_y;
  Window child_return;
  XTranslateCoordinates(display, win_id, DefaultRootWindow(display), 0, 0,
                        &root_x, &root_y, &child_return);

  // 6. Logique de bascule (0 <-> 1920)
  int target_x = (root_x < 1920) ? 2020 : 100;

  // 7. DÉPLACEMENT ET MAXIMISATION
  XMoveWindow(display, win_id, target_x, 200);
  XFlush(display);
  usleep(100000); // 0.1s

  set_window_state(display, win_id, 1, "_NET_WM_STATE_MAXIMIZED_VERT",
                   "_NET_WM_STATE_MAXIMIZED_HORZ");

  // Redonner le focus
  XRaiseWindow(display, win_id);
  XSetInputFocus(display, win_id, RevertToParent, CurrentTime);
  XFlush(display);

  XCloseDisplay(display);
  return EXIT_SUCCESS;
}