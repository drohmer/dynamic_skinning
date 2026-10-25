# Dynamic Skinning — Velocity Skinning Interactive Demo

Application interactive de démonstration du *velocity skinning* : une technique de déformation secondaire en temps réel qui ajoute des effets dynamiques (oscillation, flexion) aux personnages animés en fonction de la vitesse angulaire de leurs joints.

## Principe général

Le *velocity skinning* enrichit le skinning classique (Linear Blend Skinning) avec des déformations secondaires basées sur la vélocité des joints du squelette :

1. **Linear Blend Skinning (LBS)** — déformation standard du maillage selon les poids de skinning et les transformations du squelette.
2. **Filtre oscillation** — pour chaque joint, la vélocité angulaire est filtrée par un noyau oscillant amorti `κ(t) = exp(-t/τ) · sin(ωt)`, créant un effet de rebond/oscillation.
3. **Bend deformer** — applique une déformation de flexion (formule de Rodrigues) à chaque sommet, pondérée par les poids hiérarchiques β_ij et modulée par le poids de flexion par sommet γ^bend.

L'application propose deux chemins de rendu :
- **CPU** : LBS puis dynamic skinning calculés sur CPU, résultats uploadés aux VBOs.
- **GPU** (activé par défaut, case « GPU Skinning » ; le choix est conservé d'un modèle à l'autre) : LBS dans le vertex shader, filtre oscillation pré-calculé sur CPU et uploadé comme texture, bend deformer dans le vertex shader.


## Compilation et exécution

```bash
# Configuration
cmake -S . -B build/

# Compilation
cmake --build build/

# Exécution
build/project
```

Dépendances : GLFW (système), OpenGL 3.3+. Le framework cgp est inclus dans le répertoire `cgp/`.

### Captures d'écran automatiques

Le programme peut sauvegarder des images (sans la GUI) à des instants donnés de l'animation, puis quitter :

```bash
DS_CAPTURE_DIR=captures DS_CAPTURE_MODEL=Girafe DS_CAPTURE_TIMES="0,2.5,3.0" build/project
```

Variables optionnelles : `DS_CAPTURE_DS=0|1` (désactive/active le dynamic skinning), `DS_CAPTURE_EYE="x,y,z"` (position de caméra, visant le centre du preset), `DS_CAPTURE_GUI=1|2` (inclut la GUI dans les captures ; 2 ouvre aussi la fenêtre Settings).

### Build Emscripten (WebGL)

```bash
mkdir build_emscripten && cd build_emscripten
emcmake cmake ..
emmake make
emrun index.html
```


## Interface utilisateur

L'écran principal ne montre qu'une petite barre d'outils ; tous les réglages sont dans une fenêtre séparée « Settings », cachée par défaut.

### Barre d'outils

- **Bone edit / Camera** — mode d'interaction (aussi avec la touche Tab), voir ci-dessous.
- **Model** — choix du modèle : Cylinder, Girafe, Clown, Spot, Snail, Dragon, Lola, Cthulhu, puis les autres modèles.
- **Play / Pause** — lecture de l'animation pré-conçue du modèle.
- **Dynamic skinning** — active / désactive les déformations liées à la vitesse.
- **Frequency f, Damping tau, Amplitude A, Drag D** (visibles quand le dynamic skinning est actif) — paramètres principaux avec les notations du papier : fréquence f en Hz (stockée comme ω = 2πf), amortissement τ en secondes (stocké comme `attenuation` = 1/τ), amplitude A des oscillations, et poids D du drag. La vitesse angulaire filtrée vaut `D·Ω + A·(Ω ∗ κ')` : D règle le retard pendant le mouvement (velocity skinning, proportionnel à la vitesse), A les oscillations (déclenchées par les changements de vitesse) ; les deux sont indépendants.
- **Settings** — ouvre / ferme la fenêtre des réglages avancés.

### Contrôles souris, trackpad et tactile

| Action | Mode Bone edit | Mode Camera |
|--------|----------------|-------------|
| Clic gauche + drag (1 doigt sur tablette) | Sélection d'un joint et rotation (translation si choisie dans Settings) | Rotation de la caméra |
| Clic droit + drag | Twist du joint autour de son axe | Zoom |
| Ctrl + clic gauche + drag | — | Translation de la caméra |
| Molette / scroll à deux doigts (trackpad) | Zoom | Zoom |
| Pinch à deux doigts (trackpad en version web, tablette) | Zoom | Zoom |
| Shift + clic gauche / droit + drag | Rotation / zoom de la caméra | — |

Le pinch du trackpad n'est pas transmis par GLFW sur desktop : on y zoome avec le scroll à deux doigts. En version web, le pinch du trackpad arrive comme une molette, et les gestes tactiles à deux doigts sont gérés par `src/interaction/touch_gestures.cpp`.

Raccourcis : Tab (changement de mode), Shift + P (affiche la caméra courante sous forme de ligne de preset, à coller dans `load_model`), Shift + F (plein écran).

### Fenêtre Settings

**Window** — Plein écran, échelle de la GUI, limitation des FPS, vsync.

**Cthulhu** (si chargé) — Choix de la résolution (150K–750K sommets) et activation des poids de flexion par sommet (γ^bend) avec visualisation blanc→rouge.

**Animation** — Sélection de l'animation, position du timer, vitesse de lecture.

**Dynamic Skinning** — Skinning GPU (activé par défaut, conservé d'un modèle à l'autre) et paramètres du velocity skinning (voir section Variables).

**Bone edit** — Choix de la déformation appliquée au joint : Translation ou Rotation.

**Display** — Affichage du maillage, wireframe, texture, squelette (bones, joints, repères), visualisation des poids.

**Timings** — Mesures de performance par phase (moyenne glissante sur 60 frames).

### Animations pré-conçues

« Bending » pour le cylindre (flexion de ±90°, transitions lissées (ease-in-out) de 0,7 s, γ^bend = 0.5 comme dans le code original), « Neck swing » pour la girafe (partant de la pose droite, le cou part vers la gauche, pause, tourne vers la droite, pause, puis revient droit ; transitions lissées (ease-in-out), comme dans le teaser du papier), « Swing » pour le clown (le corps pivote sur le ressort vers la gauche puis vers la droite, comme dans la figure du papier, puis twist de 50° autour du ressort d'un côté, retour, puis de l'autre côté, pour faire osciller les mains), « Head nod & twist » pour Spot (la tête oscille de haut en bas, pause, puis twist de chaque côté pour faire osciller les oreilles), « Head up & down » pour l'escargot (rotation du bone parent du joint 10 : la tête monte légèrement avec un peu de twist, pause, revient, puis descend avec un twist opposé, pour faire osciller les yeux sans qu'ils rentrent dans la carapace ; la queue (parent du joint 9) se lève avec la tête puis se balance sur le côté quand la tête descend), « Flight » pour le dragon (4 battements d'ailes de ±15° à la **même fréquence f que les oscillations**, pour les faire entrer en résonance, puis plané (pause) ; les bras se tendent vers l'avant puis reviennent, pour faire osciller les mains ; enfin la queue se balance d'un côté puis de l'autre). L'animation du dragon est reconstruite automatiquement quand on change f (la durée de la boucle en dépend) ; Time scale ≠ 1 désynchronise les battements de la fréquence f. Toutes ces animations sont construites dans `src/loaders/procedural_animations.cpp`.


## Variables principales du Dynamic Skinning

### `dynamic_skinning_parameters`

| Variable | Défaut | Description |
|----------|--------|-------------|
| `active` | `true` | Active/désactive le dynamic skinning |
| `frequency` | `2π × 1.5` | Fréquence du noyau oscillant ω (rad/s) |
| `amplitude` | `0.15` | Amplitude A du filtre (scaling de la convolution) |
| `drag` | `0.5` | Poids D du terme de vitesse (drag du velocity skinning), indépendant de A |
| `attenuation` | `1.5` | Facteur d'amortissement 1/τ du noyau exponentiel |
| `frequency_slope` | `0.0` | Variation linéaire de fréquence dans le temps (extension Appendix C) |
| `bend_magnitude` | `1.0` | Multiplicateur global de la déformation bend |
| `bend_max_angle` | `π` | Angle maximal de déformation (clamp) |

Le facteur de flexion effectif est `0.4 × bend_magnitude` (équivalent du « Drag power » de l'original) ; `amplitude` ne règle que le filtre oscillant (`A = amplitude / 10`).

### Poids par sommet

- **γ^bend par défaut** — 0.5 pour les modèles legacy (comme dans le code original), 1.0 pour les autres.
- **β_ij** (hierarchical weights) — Calculés automatiquement à l'initialisation. Pour chaque sommet, accumule les poids de skinning α en remontant la hiérarchie du squelette (eq. 6 du papier).
- **γ^bend** (weight_bend) — Poids de flexion par sommet. Par défaut 1.0 (uniforme). Pour le Cthulhu, peut être chargé depuis les fichiers `floppy_weights` pour moduler l'intensité de déformation par zone.


## Architecture du projet

- **`src/`** — Code principal de l'application
  - `scene.cpp/.hpp` — Boucle principale, chargement des modèles, presets caméra
  - `scene_gui.cpp` — Contrôles ImGui
  - `main.cpp` — Point d'entrée
  - **`animated_character/`** — Structures de données pour l'animation : squelette (hiérarchie, matrices locales/globales), maillages riggés, animations par keyframes, poids de skinning, chargement d'assets
  - **`dynamic_skinning/`** — Velocity skinning CPU : filtre oscillation, bend deformer, tracking de vélocité angulaire par quaternion
  - **`gpu_skinning/`** — Velocity skinning GPU : upload des textures de données, calcul du filtre sur CPU, rendu via le vertex shader
  - **`interaction/`** — Picking interactif des joints : rotation et translation incrémentales à la souris ; zoom par pinch à deux doigts sur écran tactile (version web)
  - **`loaders/`** — Chargement des modèles : cylindre procédural, format legacy (OBJ + squelette texte), format binaire haute résolution (Cthulhu)
- **`shaders/`** — Shaders GLSL
  - `mesh/` — Shaders standard (Phong)
  - `single_color/` — Shader couleur uniforme (squelette)
  - `skinning/` — Shaders GPU skinning (LBS + bend deformer dans le vertex shader, Phong dans le fragment shader)
- **`cgp/`** — Framework graphique (ne pas modifier)
- **`assets/`** — Données des modèles (Lola avec animations, Cthulhu en plusieurs résolutions, modèles legacy)
- Ancien code GPU de référence : déplacé hors du projet, dans `../sources-gpu/`
