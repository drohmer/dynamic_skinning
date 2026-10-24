# Dynamic Skinning — Velocity Skinning Interactive Demo

Application interactive de démonstration du *velocity skinning* : une technique de déformation secondaire en temps réel qui ajoute des effets dynamiques (oscillation, flexion) aux personnages animés en fonction de la vitesse angulaire de leurs joints.

## Principe général

Le *velocity skinning* enrichit le skinning classique (Linear Blend Skinning) avec des déformations secondaires basées sur la vélocité des joints du squelette :

1. **Linear Blend Skinning (LBS)** — déformation standard du maillage selon les poids de skinning et les transformations du squelette.
2. **Filtre oscillation** — pour chaque joint, la vélocité angulaire est filtrée par un noyau oscillant amorti `κ(t) = exp(-t/τ) · sin(ωt)`, créant un effet de rebond/oscillation.
3. **Bend deformer** — applique une déformation de flexion (formule de Rodrigues) à chaque sommet, pondérée par les poids hiérarchiques β_ij et modulée par le poids de flexion par sommet γ^bend.

L'application propose deux chemins de rendu :
- **CPU** : LBS puis dynamic skinning calculés sur CPU, résultats uploadés aux VBOs.
- **GPU** : LBS dans le vertex shader, filtre oscillation pré-calculé sur CPU et uploadé comme texture, bend deformer dans le vertex shader.


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

### Build Emscripten (WebGL)

```bash
mkdir build_emscripten && cd build_emscripten
emcmake cmake ..
emmake make
emrun index.html
```


## Interface utilisateur

### Contrôles souris

| Action | Effet |
|--------|-------|
| Clic gauche + drag | Rotation de la caméra |
| Clic droit + drag | Translation de la caméra |
| Molette | Zoom |
| Shift + clic gauche sur joint | Rotation interactive du joint (incrémentale) |
| Shift + clic droit sur joint | Twist du joint autour de son axe |
| Shift + clic gauche (mode translation) | Translation interactive du joint |

### Panneau GUI (ImGui)

**Model** — Sélection du modèle parmi : Cylinder, Lola, Cthulhu, Clown, Dragon, Girafe, Snail, Spot. Pour le Cthulhu : choix de la résolution (150K–750K sommets) et activation des poids de flexion par sommet (γ^bend) avec visualisation blanc→rouge.

**Animation** — Sélection de l'animation, contrôle du timer (Start/Stop), vitesse de lecture.

**Display** — Affichage du maillage, wireframe, texture, squelette (bones, joints, repères).

**Sk. Interaction** — Choix du mode d'interaction : Translation ou Rotation.

**Dynamic Skinning** — Paramètres du velocity skinning (voir section Variables).

**Timings** — Mesures de performance par phase (moyenne glissante sur 60 frames).


## Variables principales du Dynamic Skinning

### `dynamic_skinning_parameters`

| Variable | Défaut | Description |
|----------|--------|-------------|
| `active` | `true` | Active/désactive le dynamic skinning |
| `frequency` | `2π × 1.5` | Fréquence du noyau oscillant ω (rad/s) |
| `amplitude` | `0.3` | Amplitude A du filtre (scaling de la convolution) |
| `attenuation` | `1.5` | Facteur d'amortissement 1/τ du noyau exponentiel |
| `frequency_slope` | `0.0` | Variation linéaire de fréquence dans le temps (extension Appendix C) |
| `bend_magnitude` | `1.0` | Multiplicateur global de la déformation bend |
| `bend_max_angle` | `π` | Angle maximal de déformation (clamp) |

Le facteur de flexion effectif est `0.4 × bend_magnitude` (équivalent du « Drag power » de l'original) ; `amplitude` ne règle que le filtre oscillant (`A = amplitude / 10`).

### Poids par sommet

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
  - **`interaction/`** — Picking interactif des joints : rotation et translation incrémentales à la souris
  - **`loaders/`** — Chargement des modèles : cylindre procédural, format legacy (OBJ + squelette texte), format binaire haute résolution (Cthulhu)
- **`shaders/`** — Shaders GLSL
  - `mesh/` — Shaders standard (Phong)
  - `single_color/` — Shader couleur uniforme (squelette)
  - `skinning/` — Shaders GPU skinning (LBS + bend deformer dans le vertex shader, Phong dans le fragment shader)
- **`cgp/`** — Framework graphique (ne pas modifier)
- **`assets/`** — Données des modèles (Lola avec animations, Cthulhu en plusieurs résolutions, modèles legacy)
- Ancien code GPU de référence : déplacé hors du projet, dans `../sources-gpu/`
