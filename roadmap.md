# 🗺️ Roadmap : Farlands (Moteur Voxel / Minecraft-like)

Cette roadmap détaille les prochaines étapes de développement du projet Farlands, de la base actuelle vers un moteur voxel complet et optimisé.

## Phase 1 : Fondations Graphiques & Données (✅ Terminée)
- [x] Initialisation de Vulkan (Instance, Device, Swapchain, RenderPass).
- [x] Pipelines graphiques et Shaders de base.
- [x] Affichage géométrique simple (Cube).
- [x] Structure de données de `Chunk` (Grille 16x256x16).
- [x] Algorithme de Maillage basique avec **Face Culling** (élimination des faces adjacentes).
- [x] Base de données de Blocs (`BlockType`, `BlockData`).

## Phase 2 : Visuels Avancés & Textures (⏳ À faire)
- [ ] **Texture Atlas** : Charger une grande image contenant toutes les textures de blocs.
- [ ] **Mapping UV** : Assigner correctement les coordonnées UV lors de la génération du Chunk Mesh selon le type de bloc et l'orientation de la face (Haut, Bas, Côtés).
- [ ] Mettre à jour les shaders pour utiliser l'échantillonnage de texture au lieu d'une couleur unie.

## Phase 3 : Immersion et Contrôles
- [ ] **Caméra Fly (Freecam)** : Contrôle total de la caméra avec la souris et le clavier (Z, Q, S, D / W, A, S, D).
- [ ] Gestion du délai inter-frame (Delta Time) pour un mouvement fluide indépendant du framerate.

## Phase 4 : Génération de Monde
- [ ] Intégration d'une librairie de génération de bruit (ex: *FastNoiseLite*).
- [ ] Génération de terrain procédural (collines, grottes) dans le `Chunk`.
- [ ] Transitions entre différents biomes ou gradients de hauteur.

## Phase 5 : Le Monde Infini (Chunk Manager)
- [ ] **Génération multi-chunks** : Structurer le stockage des chunks chargés autour du joueur.
- [ ] **Multithreading** : Déplacer la génération du maillage (`generateMesh`) et du terrain vers des threads asynchrones pour éviter les latences de rendu.
- [ ] Chargement et déchargement dynamique des chunks à mesure que la caméra se déplace.

## Phase 6 : Physique et Interaction
- [ ] Remplacement de la Fly Camera par une entité **Joueur** avec point de vue à hauteur des yeux.
- [ ] Application de la gravité et gestion des sauts.
- [ ] **Collisions AABB** : Empêcher la caméra de traverser les blocs solides (Détection des collisions sur les axes X, Y, Z).
- [ ] Interaction basique : Raycasting pour identifier le bloc regardé, le casser ou en poser un nouveau.

## Phase 7 : Éclairage et Polissage (Voxel Lighting)
- [ ] Éclairage statique basique (ambiant + soleil).
- [ ] Ambient Occlusion aux sommets de base de chaque voxel (Fake SSAO/Smooth Lighting).
- [ ] Améliorations de performance (ex: Frustum Culling des chunks, *Greedy Meshing*).
