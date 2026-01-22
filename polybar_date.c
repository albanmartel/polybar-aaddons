#include <stdio.h>
#include <time.h>
#include <locale.h>
#include <string.h>

int main() {
    // Définit la langue en français pour avoir "mardi" au lieu de "Tuesday"
    setlocale(LC_TIME, "fr_FR.UTF-8");

    time_t now;
    struct tm *tm_info;
    char date_buf[64];
    char time_buf[16];

    time(&now);
    tm_info = localtime(&now);

    // Formatage : mardi 22 janvier 2026
    strftime(date_buf, sizeof(date_buf), "%a %d %h", tm_info);
    // Formatage : 09:51
    strftime(time_buf, sizeof(time_buf), "%H : %M", tm_info);
    
    // Mesure approximative de la longueur pour le recul (ajustez selon besoin)
    // On écrit la date en T3 (très petit)
    // On utilise %%{O-X} pour reculer de X pixels
    // On écrit l'heure en T4 ( normale mais décallée vers le bas)
    printf("%%{T3}%s%%{T4}%%{O-60}%s\n", date_buf, time_buf);

    return 0;
}