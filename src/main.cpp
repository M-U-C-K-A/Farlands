// -----------------------------------------------------------------------------
// Fichier : main.cpp
// Rôle : Point d'entrée de l'application Farlands.
// Instancie et lance la boucle principale de l'application, gère les exceptions globales.
// -----------------------------------------------------------------------------
#include "app.h"
#include "core/logger.h"
#include <iostream>

// Entry point of the application
// Instantiates the Application and catches any global exceptions thrown during runtime.
int main() {
  LOG_INFO("Starting Farlands Engine... (Minecraft 2.0)");
  Application app;
  try {
    app.run();
  } catch (const std::exception &e) {
    LOG_FATAL("A fatal error occurred: " << e.what());
    return EXIT_FAILURE;
  }
  LOG_INFO("Farlands Engine shutdown.");
  return EXIT_SUCCESS;
}
