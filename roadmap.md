# 🗺️ Roadmap : Farlands (Moteur Voxel / Minecraft-like)

Cette roadmap détaille les étapes de développement du projet Farlands, d'un moteur voxel de base vers un rendu réaliste et un monde complet.

## Phase 1 : Fondations Graphiques & Données (✅ Terminée)
- [x] Initialisation de Vulkan (Instance, Device, Swapchain, RenderPass), compatible MoltenVK.
- [x] Pipelines graphiques et shaders GLSL compilés en SPIR-V.
- [x] Structure de données de `Chunk` (Grille 16x256x16).
- [x] Maillage avec **Face Culling** et occlusion ambiante par sommet.
- [x] Base de données de blocs data-driven (`assets/data/blocks.json`).

## Phase 2 : Visuels Avancés & Textures (✅ Terminée)
- [x] **Tableau de textures** (une couche par face) avec mipmaps et filtrage anisotrope.
- [x] **Normal maps** déduites automatiquement de la luminance des textures.
- [x] Feuillages ajourés (découpe alpha) et plantes en croix.

## Phase 3 : Immersion et Contrôles (✅ Terminée)
- [x] Caméra première personne (souris + ZQSD / WASD), delta time.
- [x] Menu principal par-dessus le monde rendu en direct, fenêtre d'options, overlay F3.

## Phase 4 : Génération de Monde (✅ Terminée)
- [x] Bruit de Perlin fBm maison (continents, collines, lacs).
- [x] Montagnes en multifractale "ridged" avec domain warping, neige et falaises.
- [x] 10 biomes (dont Cherry Grove, Taiga, Snowy Peaks), arbres (chêne, bouleau, sapin, cerisier), herbes et fleurs.
- [x] Point d'apparition panoramique (lac devant, sommets en vue).

## Phase 5 : Le Monde Infini (✅ Terminée)
- [x] Chargement / déchargement dynamique des chunks autour du joueur.
- [x] **Multithreading** : génération et maillage sur un pool de threads.
- [x] Un buffer GPU par chunk, frustum culling, destruction différée.

## Phase 6 : Physique et Interaction (⏳ En partie)
- [x] Entité **Joueur** : gravité, saut, nage, collisions AABB par axe.
- [ ] Raycast pour casser / poser des blocs (le bloc visé est déjà calculé pour l'overlay).
- [ ] Sauvegarde des modifications du joueur (le format de chunk est prêt).

## Phase 7 : Rendu Réaliste (✅ Terminée)
- [x] Pipeline HDR, reverse-Z, rendu relatif à la caméra.
- [x] Ciel atmosphérique (Rayleigh + Mie), soleil, nuages volumétriques.
- [x] Shadow map du soleil (PCF, cache), ombres des nuages.
- [x] Eau : vagues, réflexions SSR, réfraction, absorption, caustiques, vue sous l'eau.
- [x] Bloom, god rays, tonemapping ACES.

## Phase 8 : Ambiances et horizon (✅ Terminée)
- [x] Brouillard volumétrique avec rayons de lumière (shadow map).
- [x] Préréglages de shaders : Réaliste, Golden Hour, Horreur (+ étalonnage des couleurs).
- [x] Terrain lointain (LOD) jusqu'à 6 km.
- [x] Feuilles et pétales qui tombent, transitions douces herbe / sable / neige.
- [x] Onglets Performance / Système, compteur de FPS, plein écran F11.

## Phase 9 : Suite
- [ ] Greedy meshing (moins de sommets pour les ombres).
- [ ] Cascaded shadow maps.
- [ ] Grottes.
- [ ] Cycle jour / nuit, lune et étoiles.
- [ ] Lumières ponctuelles (lanternes, torches).
