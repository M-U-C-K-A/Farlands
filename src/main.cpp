// -----------------------------------------------------------------------------
// Fichier : main.cpp
// Rôle : Point d'entrée de l'application Farlands.
// Instancie et lance la boucle principale de l'application, gère les exceptions globales.
// -----------------------------------------------------------------------------
#include "app.h"
#include <iostream>

// Entry point of the application
// Instantiates the Application and catches any global exceptions thrown during runtime.
int main() {
  Application app;
  try {
    app.run();
  } catch (const std::exception &e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
