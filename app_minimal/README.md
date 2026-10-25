# Application minimale — Dynamic Skinning sur CPU

Version la plus simple possible du dynamic skinning, sur trois modèles : le cylindre, la girafe et le clown. Il n'y a ni GPU, ni picking des joints, ni délai. On peut seulement tourner la caméra, choisir le modèle et régler les paramètres du noyau d'oscillation et le drag.

L'algorithme n'est pas dupliqué : comme l'application complète, elle utilise `dynamic_skinning_source/` (la méthode) et `lib_helper/` (personnage animé, loaders et animations).

## Compilation et exécution

```bash
cd app_minimal
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/minimal        # les shaders et assets sont trouvés à la racine du projet
```

Elle est aussi compilée par le `CMakeLists.txt` racine (`build/minimal`).

## Fichiers

- **`scene.cpp/.hpp`** : tout le dynamic skinning de l'application.
- **`environment.cpp/.hpp`** : caméra et lumière envoyées aux shaders, chemin du projet.
- **`main.cpp`** : la fenêtre, les callbacks et la boucle d'affichage génériques de cgp.

À chaque frame (`scene_structure::display_frame`), dans l'ordre :

1. La pose du squelette au temps de l'animation donne les repères des joints et leurs positions q_j.
2. Le LBS calcule les positions p_i^LBS.
3. Le dynamic skinning calcule p_i^DS = p_i^LBS + Σ_j β_ij ψ^bend(F^osc(Ω_j), p_i^LBS − q_j).
4. Le maillage déformé est affiché.

## Paramètres (notations de l'article)

Le noyau d'oscillation est κ(t) = A exp(−t/τ) sin(2π f t), avec :

| Variable | Rôle | Valeur par défaut |
|---|---|---|
| `f`   | fréquence de l'oscillation (Hz) | 1.5 |
| `tau` | amortissement τ (s) | 0.67 |
| `A`   | amplitude | 0.88 |

Et le poids du drag (retard proportionnel à la vitesse angulaire, indépendant des oscillations) :

| Variable | Rôle | Valeur par défaut |
|---|---|---|
| `D`   | drag | 0.5 |

Souris : clic gauche glissé = rotation, clic droit glissé ou molette = zoom, Ctrl + clic gauche = translation.
