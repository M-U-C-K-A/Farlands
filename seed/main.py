# objectif faire une seed similaire a celle de minecraft, la logique etc... sortir avec
# mathplotlib 5 "chart" reprensentant les valeurs en grayscale de chaque perlin noise
# 1 perlin noise pour la continentalite
# 1 perlin noise pour l'erosion
# 1 perlin noise pour peak & valleys
# 1 perlin noise pour la temperature
# 1 perlin noise pour l'Humidity


import numpy as np
import matplotlib.pyplot as plt

# --- Fonctions Perlin (Inchangées et Corrigées) ---
def fade(t):
    return 6 * t**5 - 15 * t**4 + 10 * t**3

def lerp(a, b, x):
    return a + x * (b - a)

def gradient(h, x, y):
    vectors = np.array([[0, 1], [0, -1], [1, 0], [-1, 0]])
    g = vectors[h % 4]
    return g[:, :, 0] * x + g[:, :, 1] * y

def perlin(x, y, seed=0):
    np.random.seed(seed)
    p = np.arange(256, dtype=int)
    np.random.shuffle(p)
    p = np.stack([p, p]).flatten()

    xi = x.astype(int)
    yi = y.astype(int)
    xf = x - xi
    yf = y - yi

    u = fade(xf)
    v = fade(yf)

    # Gradients aux 4 coins
    n00 = gradient(p[p[xi] + yi], xf, yf)
    n01 = gradient(p[p[xi] + yi + 1], xf, yf - 1)
    n11 = gradient(p[p[xi + 1] + yi + 1], xf - 1, yf - 1)
    n10 = gradient(p[p[xi + 1] + yi], xf - 1, yf)

    # Interpolation linéaire (LERP)
    x1 = lerp(n00, n10, u)
    x2 = lerp(n01, n11, u) # <--- Utilisation correcte de n01 et n11
    return lerp(x1, x2, v)

# --- Préparation des données (Résolution augmentée pour la grille) ---
resolution = 256 # Grille de 256x256
lin = np.linspace(0, 5, resolution, endpoint=False)
y, x = np.meshgrid(lin, lin)

# Configuration des 5 couches de bruit de Minecraft
noise_layers = [
    {"name": "1. Continentalité", "seed": 111, "scale": 2.0},  # Très grandes formes
    {"name": "2. Érosion",         "seed": 222, "scale": 4.0},  # Détails côtiers
    {"name": "3. Peaks & Valleys", "seed": 333, "scale": 8.0},  # Relief accidenté
    {"name": "4. Température",     "seed": 444, "scale": 1.5},  # Zones climatiques
    {"name": "5. Humidité",        "seed": 555, "scale": 2.5}   # Variations de biomes
]

# --- Création de la figure globale (Grille 2x3) ---
# figsize définit la taille de l'image globale en pouces.
fig, axes = plt.subplots(nrows=2, ncols=3, figsize=(15, 10))

# flatten() permet de parcourir les 6 cases de la grille facilement
axes_flat = axes.flatten()

# Titre général de l'image
fig.suptitle("Farland Generation perlin noise", fontsize=20, y=0.98)

# Boucle pour générer et afficher chaque carte
for i, layer in enumerate(noise_layers):
    ax = axes_flat[i] # Sélection de la case actuelle

    # Calcul du bruit
    res = perlin(x * layer["scale"], y * layer["scale"], seed=layer["seed"])

    # Affichage en Grayscale
    ax.imshow(res, cmap='gray', origin='lower')

    # Configuration du sous-titre et suppression des axes
    ax.set_title(layer["name"], fontsize=14)
    ax.axis('off') # Cache les graduations (0, 50, 100, etc.)

# --- Gestion de la 6ème case (vide) ---
# Nous désactivons l'affichage de la dernière case (index 5) car elle est vide.
axes_flat[5].axis('off')

# tight_layout() ajuste automatiquement l'espacement pour éviter les chevauchements.
plt.tight_layout(rect=[0, 0, 1, 0.96]) # Ajustement pour le titre général

# Affichage final
plt.show()
