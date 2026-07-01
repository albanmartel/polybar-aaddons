#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>

#include <algorithm>
#include <cassert>
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/prctl.h>
#include <vector>

// Pour la communication Inter-Processus (IPC Socket)
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define PROGRAMME_NAME "ram-monitor"
#define SOCKET_PATH "/tmp/ram-monitor.sock"

struct ProcessInfo {
  std::string name;
  double memory;
};

// Widgets globaux
Fl_Text_Buffer *text_buffer = nullptr;
Fl_Window *main_window =
    nullptr; // Rendu global pour y accéder depuis le Callback du Socket

// Comparateur pour le tri std::sort
bool compare_proc(const ProcessInfo &a, const ProcessInfo &b) {
  assert(a.memory >= 0.0 &&
         "Erreur : La mémoire du processus A est négative !");
  assert(b.memory >= 0.0 &&
         "Erreur : La mémoire du processus B est négative !");
  assert(!a.name.empty() && "Erreur : Le processus A n'a pas de nom !");
  assert(!b.name.empty() && "Erreur : Le processus B n'a pas de nom !");
  return a.memory > b.memory;
}

// Algorithme Léger : Lecture système directe de /proc/meminfo
double get_system_real_used_ram() {
  FILE *fp = fopen("/proc/meminfo", "r");
  if (!fp)
    return 0.0;

  unsigned long total = 0;
  unsigned long available = 0;
  char line[256];
  int found = 0;

  while (fgets(line, sizeof(line), fp)) {
    if (strncmp(line, "MemTotal:", 9) == 0) {
      sscanf(line, "MemTotal: %lu", &total);
      found++;
    } else if (strncmp(line, "MemAvailable:", 13) == 0) {
      sscanf(line, "MemAvailable: %lu", &available);
      found++;
    }
    if (found == 2)
      break;
  }
  fclose(fp);

  if (found < 2 || total == 0)
    return 0.0;
  return (double)(total - available) / 1024.0;
}

// Algorithme Léger : Parcours natif de /proc
std::string get_sorted_ram_report() {
  std::vector<ProcessInfo> procs;
  procs.reserve(256);

  double monitor_mem = 0.0;
  DIR *dir = opendir("/proc");
  if (!dir)
    return "Erreur : Impossible d'accéder à /proc";

  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (!isdigit(static_cast<unsigned char>(entry->d_name[0])))
      continue;

    char statm_path[300]; // Augmenté à 300 pour éviter le warning de troncature
    snprintf(statm_path, sizeof(statm_path), "/proc/%s/statm", entry->d_name);

    FILE *fstatm = fopen(statm_path, "r");
    if (!fstatm)
      continue;

    long size, resident, share;
    if (fscanf(fstatm, "%ld %ld %ld", &size, &resident, &share) != 3) {
      fclose(fstatm);
      continue;
    }
    fclose(fstatm);

    if (resident <= 0)
      continue;

    double rss_mo = (double)(resident * 4096) / (1024.0 * 1024.0);
    if (rss_mo < 0.5)
      continue;

    char comm_path[300]; // Augmenté à 300 pour éviter le warning de troncature
    snprintf(comm_path, sizeof(comm_path), "/proc/%s/comm", entry->d_name);
    FILE *fcomm = fopen(comm_path, "r");
    if (!fcomm)
      continue;

    char name_tmp[128];
    if (fgets(name_tmp, sizeof(name_tmp), fcomm)) {
      name_tmp[strcspn(name_tmp, "\n")] = 0;

      if (strcmp(name_tmp, PROGRAMME_NAME) == 0) {
        monitor_mem += rss_mo;
      } else {
        ProcessInfo p;
        p.name = name_tmp;
        p.memory = rss_mo;
        procs.push_back(p);
      }
    }
    fclose(fcomm);
  }
  closedir(dir);

  if (monitor_mem > 0) {
    ProcessInfo p = {PROGRAMME_NAME, monitor_mem};
    procs.push_back(p);
  }

  std::sort(procs.begin(), procs.end(), compare_proc);

  std::string report = "";
  char buffer[256];

  snprintf(buffer, sizeof(buffer), "  %-25s %s\n", "APPLICATION",
           "UTILISATION");
  report = buffer;
  report += "  ==========================================\n\n";

  for (const auto &proc : procs) {
    snprintf(buffer, sizeof(buffer), "  %-25s %8.2f Mo\n", proc.name.c_str(),
             proc.memory);
    report += buffer;
  }

  double real_system_total = get_system_real_used_ram();
  snprintf(buffer, sizeof(buffer),
           "\n  ==========================================\n"
           "  VRAI TOTAL RAM : %.2f Mo (%.2f Go)\n",
           real_system_total, real_system_total / 1024.0);
  report += buffer;

  return report;
}

// --- CALLBACKS FLTK ---

void on_refresh(Fl_Widget *, void *) {
  if (!text_buffer)
    return;

  std::string report = get_sorted_ram_report();
  if (report.empty()) {
    text_buffer->text("Erreur lors de la génération du rapport RAM.");
    return;
  }

  text_buffer->text("");
  text_buffer->text(report.c_str());
}

void on_save(Fl_Widget *, void *window_ptr) {
  if (!text_buffer)
    return;
  Fl_Window *win = static_cast<Fl_Window *>(window_ptr);
  if (!win)
    return;

  Fl_Native_File_Chooser fnfc;
  fnfc.title("Enregistrer le rapport");
  fnfc.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
  fnfc.filter("Fichiers texte\t*.txt");
  fnfc.preset_file("conso_ram.txt");

  if (fnfc.show() == 0) {
    const char *filepath = fnfc.filename();
    if (!filepath || strlen(filepath) == 0)
      return;

    FILE *fp = fopen(filepath, "w");
    if (fp) {
      fputs(text_buffer->text(), fp);
      fclose(fp);
    }
  }
}

// --- GESTION DE LA PREMIÈRE INSTANCE (SOCKET UNIX) ---

// Cette fonction est appelée par FLTK chaque fois que la 2ème instance envoie
// un signal au socket
void socket_callback(int fd, void *data) {
  int client_fd = accept(fd, nullptr, nullptr);
  if (client_fd >= 0) {
    char buffer[16];
    int bytes = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes > 0) {
      buffer[bytes] = '\0';
      if (strcmp(buffer, "ACTIVATE") == 0 && main_window) {
        // Force la fenêtre existante à réapparaître et à prendre le focus
        main_window->show();
        // Optionnel : On en profite pour rafraîchir les données de RAM
        on_refresh(nullptr, nullptr);
      }
    }
    close(client_fd);
  }
}

// --- INTERFACE PRINCIPALE ---

int main(int argc, char *argv[]) {
  prctl(PR_SET_NAME, PROGRAMME_NAME, 0, 0, 0);

  // 1. TENTATIVE DE CONNEXION AU SOCKET (Vérification si une instance tourne
  // déjà)
  int sock = socket(AF_UNIX, SOCK_STREAM, 0);
  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

  if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
    // La connexion a réussi -> Une instance tourne déjà !
    // On lui envoie le signal de s'activer
    if (write(sock, "ACTIVATE", 8) < 0) {
      // Échec de l'envoi silencieux, rien de grave car on quitte juste après
    }
    close(sock);
    return 0; // On quitte immédiatement la 2ème instance en silence
  }
  close(sock); // On ferme ce socket de test client

  // 2. CONFIGURATION DU SERVEUR UNIQUE (Pour la première instance)
  // On nettoie un éventuel vieux socket résiduel (si crash précédent)
  unlink(SOCKET_PATH);

  int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd >= 0) {
    // On rend le socket non-bloquant pour FLTK
    fcntl(server_fd, F_SETFL, O_NONBLOCK);
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
      listen(server_fd, 5);
      // On demande à l'event-loop de FLTK de surveiller ce socket en tâche de
      // fond
      Fl::add_fd(server_fd, FL_READ, socket_callback);
    }
  }

  // 3. CRÉATION DE L'INTERFACE GRAPHIQUE FLTK
  main_window = new Fl_Window(500, 650, "ArchMonitor RAM");

  text_buffer = new Fl_Text_Buffer();
  Fl_Text_Display *text_view = new Fl_Text_Display(15, 15, 470, 560);
  text_view->buffer(text_buffer);
  text_view->textfont(FL_COURIER);
  text_view->textsize(12);

  Fl_Button *btn_ref = new Fl_Button(240, 595, 110, 35, "🔄 Actualiser");
  btn_ref->callback(on_refresh);

  Fl_Button *btn_sav = new Fl_Button(360, 595, 125, 35, "💾 Enregistrer");
  btn_sav->callback(on_save, main_window);

  main_window->end();
  main_window->resizable(text_view);
  main_window->show(argc, argv);

  on_refresh(nullptr, nullptr);

  // Boucle principale
  int exit_code = Fl::run();

  // Nettoyage du fichier socket à la fermeture complète du programme
  if (server_fd >= 0) {
    Fl::remove_fd(server_fd);
    close(server_fd);
  }
  unlink(SOCKET_PATH);

  return exit_code;
}