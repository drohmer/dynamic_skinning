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

# Exécution (depuis la racine)
build/complete    # application complète
build/minimal     # application minimale
```

Le `CMakeLists.txt` racine compile les deux applications ; chacune peut aussi être compilée seule depuis son répertoire (`cd app_minimal && cmake -B build && cmake --build build`).

Avec le `Makefile` : `make` compile les deux applications (`./complete` et `./minimal` à la racine), `make complete` ou `make minimal` une seule ; le code commun (`dynamic_skinning_source/`, `lib_helper/`, `cgp/`) n'est compilé qu'une fois.

Dépendances : GLFW (système), OpenGL 3.3+. Le framework cgp est inclus dans le répertoire `cgp/` (version de [cgp-2027](https://github.com/drohmer/cgp-2027), commit 34b0efa). Hors build Debug, les assertions de cgp sont désactivées (`CGP_NO_DEBUG`) : les vérifications d'indices sur les opérations `vec3` ralentissent fortement le skinning CPU.

### Captures d'écran automatiques

Le programme peut sauvegarder des images (sans la GUI) à des instants donnés de l'animation, puis quitter :

```bash
DS_CAPTURE_DIR=captures DS_CAPTURE_MODEL=Girafe DS_CAPTURE_TIMES="0,2.5,3.0" build/complete
```

Variables optionnelles : `DS_CAPTURE_DS=0|1` (désactive/active le dynamic skinning), `DS_CAPTURE_DELAY=0|1` (désactive/active le délai), `DS_CAPTURE_FPS=30` (impose une fréquence d'images, sans vsync), `DS_CAPTURE_EYE="x,y,z"` (position de caméra, visant le centre du preset), `DS_CAPTURE_GUI=1|2` (inclut la GUI dans les captures ; 2 ouvre aussi la fenêtre Settings).

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
- **Model** — choix du modèle : Cylinder, Girafe, Clown, Spot, Snail, Dragon, Cthulhu, Train, puis les autres modèles.
- **Play / Pause** — lecture de l'animation pré-conçue du modèle.
- **Dynamic skinning** — active / désactive les déformations liées à la vitesse.
- **Skeleton** — affiche / masque rapidement le squelette (os et joints).
- **Weights** (Cthulhu) — colore les poids de flexion par sommet (blanc : rigide, rouge doux : souple ; uniquement pour le Cthulhu, les autres modèles ayant des poids uniformes), activé par défaut pour bien voir les parties qui se déforment (tentacules, pointes, membranes).
- **Local centers** (Clown) — « flexible origins » de la section *Deformation origin control* du papier : quand un joint du corps tourne, les bras et le chapeau se déforment autour de leur propre centre, placé au début du membre (sphères jaunes `joint8`, `joint11`, `joint19`), et le corps et la tête restent rigides. Pour un joint j situé au-dessus d'une origine m, la contribution du sommet i devient β_im · ψ^bend(Ω_j, p_i − q_m) ; la partie restante (β_ij − β_im) est rigide. La transition entre la partie rigide et la partie souple reste lisse grâce aux poids du rig. L'option est activée par défaut, avec D = 1 (D revient à la valeur par défaut si on la décoche) ; la décocher redonne le comportement standard, où tout le clown plie autour du joint en rotation. Cette option a demandé de remplacer le rig du clown par celui de `ClownSkeletonHat` du code original (même maillage, mais 27 joints, avec les bras et le chapeau).
- **Delay** — *delayed LBS* (dLBS, section *Time Delay Filter* du papier) : chaque transformation locale L_k du squelette est retardée de (d_i − d_k)/c, soit p_i^dLBS = Σ_j α_ij Π_{k ∈ racine..j} L_k(t − (d_i − d_k)/c) B_j⁻¹ p_i⁰. Le dynamic skinning s'applique ensuite sur p^dLBS au lieu de p^LBS. d_i est la distance du sommet à la racine mesurée le long du squelette (distance au point le plus proche sur les os du joint de plus fort poids α_ij, comme dans le papier, puis longueurs d'os cumulées jusqu'à la racine), d_k la même distance pour le joint k ; elles sont calculées une fois au chargement. Le mouvement d'un joint part donc de ce joint et se propage vers les extrémités : la bosse de la locomotive (racine) va jusqu'au dernier wagon, et quand on tourne le cou de la girafe, la base du cou suit tout de suite et la tête avec retard. (Le code original utilisait une seule pose globale retardée de d_i/c, ce qui décalait aussi le départ du mouvement d'un joint éloigné de la racine.) Les matrices locales passées sont stockées toutes les 1/60 s sur 2 s (`dynamic_skinning_source/delayed_lbs.cpp`), et une texture d'historique les transmet au shader en mode GPU. Le coût est proportionnel à la profondeur du joint dans le squelette. Le slider **Delay speed c** règle la vitesse de propagation, en unités du modèle (qui tient dans [−1, 1]) par seconde. L'option est activée seulement au chargement du Train, sans le dynamic skinning (qui est réactivé au chargement des autres modèles), avec c = 3 (c = 2 pour les autres modèles). La première fois qu'on coche **Delay**, le dynamic skinning est décoché pour voir le délai seul (on peut le recocher ensuite).
- **Frequency f, Damping tau, Amplitude A, Drag D** (visibles quand le dynamic skinning est actif) — paramètres principaux avec les notations du papier : fréquence f en Hz (stockée comme ω = 2πf), amortissement τ en secondes (stocké comme `attenuation` = 1/τ), amplitude A des oscillations, et poids D du drag. La vitesse angulaire filtrée vaut `D·Ω + Σ_k κ(kΔt) (Ω((i−k)Δt) − Ω((i−k−1)Δt))`, forme numérique de l'équation du papier, avec le noyau κ(t) = A exp(−t/τ) sin(2πf t) (A est donc l'amplitude du noyau, comme dans le papier) : D règle le retard pendant le mouvement (velocity skinning, proportionnel à la vitesse), A les oscillations (déclenchées par les changements de vitesse) ; les deux sont indépendants.
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

Le pinch du trackpad n'est pas transmis par GLFW sur desktop : on y zoome avec le scroll à deux doigts. En version web, le pinch du trackpad arrive comme une molette, et les gestes tactiles à deux doigts sont gérés par `app_complete/interaction/touch_gestures.cpp`.

Raccourcis : Tab (changement de mode), Shift + P (affiche la caméra courante sous forme de ligne de preset — œil, centre, distance et vecteur « haut », pour conserver aussi l'inclinaison de la caméra — à coller dans `load_model`), Shift + F (plein écran).

### Fenêtre Settings

**Window** — Plein écran, échelle de la GUI, limitation des FPS, vsync.

**Cthulhu** (si chargé) — Choix de la résolution (150K–750K sommets) et activation des poids de flexion par sommet (γ^bend, **activés par défaut** : seules les parties souples se déforment — membranes des ailes, tentacules, pointes) avec visualisation blanc→rouge.

**Animation** — Sélection de l'animation, position du timer, vitesse de lecture.

**Dynamic Skinning** — Skinning GPU (activé par défaut, conservé d'un modèle à l'autre) et paramètres du velocity skinning (voir section Variables).

**Bone edit** — Choix de la déformation appliquée au joint : Translation ou Rotation.

**Display** — Affichage du maillage, wireframe, texture, squelette (bones, joints, repères), visualisation des poids.

**Timings** — Mesures de performance par phase (moyenne glissante sur 60 frames).

### Animations pré-conçues

« Bending » pour le cylindre (flexion de ±90°, transitions lissées (ease-in-out) de 0,7 s, γ^bend = 0.5 comme dans le code original), « Neck swing » pour la girafe (partant de la pose droite, le cou part vers la gauche, pause, tourne vers la droite, pause, puis revient droit ; transitions lissées (ease-in-out), comme dans le teaser du papier), « Swing » pour le clown (le corps pivote sur le ressort vers la gauche puis vers la droite, comme dans la figure du papier, puis twist de 50° autour du ressort d'un côté, retour, puis de l'autre côté, pour faire osciller les mains), « Head nod & twist » pour Spot (la tête oscille de haut en bas, pause, puis twist de chaque côté pour faire osciller les oreilles), « Head up & down » pour l'escargot (rotation du bone parent du joint 10 : la tête monte légèrement avec un peu de twist, pause, revient, puis descend avec un twist opposé, pour faire osciller les yeux sans qu'ils rentrent dans la carapace ; la queue (parent du joint 9) se lève avec la tête puis se balance sur le côté quand la tête descend), « Flight » pour le dragon (les ailes battent presque tout le temps, ±15° à la **même fréquence f que les oscillations** pour les faire entrer en résonance, avec un court plané en fin de boucle ; pendant ce temps, les bras se tendent vers l'avant l'un après l'autre puis ensemble, la queue se balance et se soulève, la tête tourne, hoche et ouvre la gueule, et les jambes donnent de petits coups vers l'avant, avec des mouvements rapides et des pauses pour faire osciller mains, queue, tête et pieds ; vue de côté, légèrement de face). L'animation du dragon est reconstruite automatiquement quand on change f (la durée de la boucle en dépend) ; Time scale ≠ 1 désynchronise les battements de la fréquence f. « Head shake » pour le Cthulhu (mouvements volontairement **petits**, car les grandes ailes et les tentacules s'auto-intersectent vite ; la tête, les ailes, les bras et les jambes bougent ensemble : ailes levées de 6°, bras vers l'avant (12°), jambes vers l'avant (7°, mouvements lents de 0,7 s pour ne pas trop étirer les pointes), hochement de tête (8°), puis la tête fait « non » (6°) pendant que l'articulation des tentacules du visage (Bone_003) se tourne de 12° d'un côté puis revient (mouvements de 0,4 s, pas plus rapides que l'oscillation des tentacules) ; la queue suit ; un battement d'ailes à la fréquence f a été écarté car la résonance déforme trop les membranes ; vue rapprochée par en dessous, et A = 0.47, D = 0.3 au chargement du Cthulhu, les autres modèles reprenant les valeurs par défaut). « Bumps » pour le train (figure du dLBS dans le papier, modèle et voie repris du code original) : la locomotive, à la racine du squelette, passe sur trois bosses de hauteurs différentes avec un léger roulis alterné. Avec **Delay**, chaque bosse se propage de la locomotive jusqu'au dernier wagon, et la voie (rails, traverses, piliers) défile pour donner l'impression que le train avance. Les instants des bosses sont fixes, pour que la boucle soit reproductible, alors que l'original les tirait au hasard. Toutes ces animations sont construites dans `lib_helper/loaders/procedural_animations.cpp`.


## Variables principales du Dynamic Skinning

### `dynamic_skinning_parameters`

| Variable | Défaut | Description |
|----------|--------|-------------|
| `active` | `true` | Active/désactive le dynamic skinning |
| `frequency` | `2π × 1.5` | Fréquence du noyau oscillant ω (rad/s) |
| `amplitude` | `0.88` | Amplitude A du noyau oscillant κ |
| `drag` | `0.5` | Poids D du terme de vitesse (drag du velocity skinning), indépendant de A |
| `attenuation` | `1.5` | Facteur d'amortissement 1/τ du noyau exponentiel |
| `frequency_slope` | `0.0` | Variation linéaire de fréquence dans le temps (extension Appendix C) |
| `bend_magnitude` | `1.0` | Multiplicateur global de la déformation bend |
| `bend_max_angle` | `π` | Angle maximal de déformation (clamp) |

Le facteur de flexion effectif est `0.4 × bend_magnitude` (équivalent du « Drag power » de l'original) ; `amplitude` ne règle que le filtre oscillant.

Indépendance au framerate : la vitesse angulaire de chaque joint est mesurée à chaque image avec la durée réelle de l'image, lissée avec un temps caractéristique (0,058 s, équivalent au coefficient 0,75 par image du code original à 60 fps), puis stockée dans un historique à pas fixe Δt = 1/60 s (`dynamic_skinning_source/angular_velocity_tracker.hpp`). La convolution lit donc toujours l'échantillon k au temps t − kΔt, et les oscillations (fréquence, amortissement, amplitude) sont les mêmes à 30, 60 ou 120 images/s. Le code original supposait une image toutes les 0,017 s ; ses valeurs d'amplitude correspondent à A ≈ 5,9 × amplitude, car sa convolution `Σ Ω κ'` omettait le facteur Δt et divisait par 10 (0.15 → 0.88, 0.08 → 0.47). Son premier terme utilisait aussi κ(−Δt), ce qui ajoutait un léger drag instantané, supprimé ici.

### Poids par sommet

- **γ^bend par défaut** — 0.5 pour les modèles legacy (comme dans le code original), 1.0 pour les autres.
- **β_ij** (hierarchical weights) — Calculés automatiquement à l'initialisation. Pour chaque sommet, accumule les poids de skinning α en remontant la hiérarchie du squelette (eq. 6 du papier).
- **γ^bend** (weight_bend) — Poids de flexion par sommet. Par défaut 1.0 (uniforme). Pour le Cthulhu, peut être chargé depuis les fichiers `floppy_weights` pour moduler l'intensité de déformation par zone.


## Architecture du projet

- **`dynamic_skinning_source/`** — La méthode seule (CPU), partagée par les deux applications : filtre d'oscillation (`oscillation_filter.hpp`), suivi de la vitesse angulaire par quaternion (`angular_velocity_tracker.hpp`), bend deformer et poids hiérarchiques (`dynamic_skinning.cpp`), delayed LBS (`delayed_lbs.cpp`)
- **`lib_helper/`** — Code d'appui, partagé par les deux applications
  - **`animated_character/`** — Structures de données pour l'animation : squelette (hiérarchie, matrices locales/globales), maillages riggés, animations par keyframes, poids de skinning, chargement d'assets
  - **`loaders/`** — Chargement des modèles : cylindre procédural, format legacy (OBJ + squelette texte), format binaire haute résolution (Cthulhu), animations pré-conçues
- **`app_complete/`** — Application complète (exécutable `complete`, aussi en version web)
  - `scene.cpp/.hpp` — Boucle principale, chargement des modèles, presets caméra
  - `scene_gui.cpp` — Contrôles ImGui
  - `main.cpp` — Point d'entrée
  - **`gpu_skinning/`** — Velocity skinning GPU : upload des textures de données, calcul du filtre sur CPU, rendu via le vertex shader
  - **`interaction/`** — Picking interactif des joints : rotation et translation incrémentales à la souris ; zoom par pinch à deux doigts sur écran tactile (version web)
  - **`capture/`** — Captures d'écran automatiques
- **`app_minimal/`** — Application minimale (exécutable `minimal`, CPU seulement, cylindre / girafe / clown, caméra et paramètres f, τ, A, D) ; voir son README
- **`blender_addon/`** — Prototype d'add-on Blender (5.0+), autonome avec ses propres modèles (`blender_addon/assets/`) : dynamic skinning en numpy, appliqué par un modificateur Geometry Nodes ; voir son README
- **`shaders/`** — Shaders GLSL
  - `mesh/` — Shaders standard (Phong)
  - `single_color/` — Shader couleur uniforme (squelette)
  - `skinning/` — Shaders GPU skinning (LBS + bend deformer dans le vertex shader, Phong dans le fragment shader)
- **`cgp/`** — Framework graphique cgp-2027 (ne pas modifier, à part la suppression de warnings « unused » quand `CGP_NO_DEBUG` est défini)
- **`assets/`** — Données des modèles (Cthulhu en plusieurs résolutions, modèles legacy)
- Ancien code GPU de référence : déplacé hors du projet, dans `../archive/sources-gpu/`
