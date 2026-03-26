# 🌌 Farlands

**Farlands** est un moteur de jeu de type voxel (style Minecraft) développé de A à Z en **C++** avec l'API **Vulkan**. 

L'objectif du projet est de créer un environnement de jeu infini, visuellement époustouflant et ultra-optimisé, en éliminant les goulots d'étranglement traditionnels des moteurs basés sur Java ou C#.

---

## 🚀 Pourquoi Farlands ?

La plupart des clones de Minecraft souffrent de problèmes de performance liés à la gestion de la mémoire ou à des API graphiques vieillissantes. Farlands s'attaque à ces problèmes à la source :

- **Vulkan API** : Communication directe avec le GPU pour un overhead minimal.
- **C++ Natif** : Gestion manuelle de la mémoire pour éviter les lags de "Garbage Collection".
- **Greedy Meshing** : Algorithme d'optimisation réduisant drastiquement le nombre de polygones à afficher.
- **Multithreading** : Chargement des chunks et génération du monde sur des threads séparés pour une fluidité totale.

## ✨ Fonctionnalités (en cours)

- [ ] **Rendu 3D** : Pipeline de rendu Vulkan complet (Shaders GLSL).
- [ ] **Génération procédurale** : Mondes infinis basés sur le bruit de Perlin/Simplex.
- [ ] **Système de Chunks** : Gestion dynamique du chargement/déchargement.
- [ ] **Physique** : Détection de collision rapide pour les voxels.
- [ ] **Lighting** : Système de lumière dynamique et SSAO.

## 🛠️ Stack Technique

- **Langage** : C++20
- **Graphismes** : Vulkan SDK
- **Fenêtrage** : GLFW
- **Mathématiques** : GLM (OpenGL Mathematics)
- **Shaders** : GLSL (compilé en SPIR-V)

---

## 🛠️ Installation (Preview)

> *Note : Le projet est actuellement en phase de développement intensif (Pre-Alpha).*

```bash
# Cloner le dépôt
  git clone [https://github.com/M-U-C-K-A/farlands.git](https://github.com/M-U-C-K-A/farlands.git)

# Initialiser les sous-modules (Vulkan, GLFW, etc.)
git submodule update --init --recursive
