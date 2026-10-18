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

## Phase 6 : Physique et Interaction (✅ Terminée)
- [x] Entité **Joueur** : gravité, saut, nage, sprint, collisions AABB par axe.
- [x] **Vol** créatif (double Espace ou F), zoom (C).
- [x] Raycast (DDA) pour casser / poser / choisir des blocs, barre d'inventaire à 9 cases, contour du bloc visé.
- [x] Sauvegarde des modifications du joueur, un dossier par monde (graine, type).

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

## Phase 9 : Structures et mondes (✅ Terminée)
- [x] Structures : villages (5 styles : plaines, taïga, cerisiers, neige, désert), temples du désert, tours en ruine, igloos, sanctuaires des cerisiers (torii), phares.
- [x] Biome **Amplified Peaks** (aiguilles, surplombs en bruit 3D) et type de monde amplifié.
- [x] Rochers, cactus, buissons morts, citrouilles ; 21 nouveaux blocs (textures procédurales).
- [x] Lumière des blocs (lanternes, torches, citrouilles-lanternes) propagée dans le maillage.
- [x] Préréglage **Nuit** : clair de lune, ombres de la lune, étoiles.
- [x] Onglet Monde : téléportation vers les structures et les biomes, nouveau monde depuis une graine.
- [x] Scripts de capture (`FARLANDS_SCRIPT`) pour les captures de la documentation.
- [x] **Grands villages** : rues et ruelles, place avec fontaine et étals, maisons de trois tailles, église (clocher, vitraux, bancs), forge, taverne, bibliothèque, remparts avec tours et portes, moulins, granges, champs le long des chemins, port avec pontons et barques.
- [x] Escaliers (4 orientations), dalles, vitres, vitraux, barrières, portes : sommets de 16 octets avec coordonnées de texture fractionnaires, collisions selon la forme, marche automatique.

## Phase 10 : Suite
- [ ] Greedy meshing (moins de sommets pour les ombres).
- [ ] Cascaded shadow maps.
- [ ] Grottes.
- [ ] Cycle jour / nuit animé.
- [ ] Villageois et animaux.
- [ ] Escaliers renversés, portes qui s'ouvrent ; structures visibles dans le terrain lointain.
