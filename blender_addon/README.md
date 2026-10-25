# Add-on Blender — Dynamic Skinning (prototype)

Ajoute le dynamic skinning (oscillations et drag) à un maillage riggé dans Blender (5.0 et plus, testé avec 5.2). Le maillage oscille quand ses os bougent : quand on manipule un os en Pose Mode, et pendant la lecture de l'animation. C'est la version simple de la méthode, sans délai (dLBS) ni origines flexibles.

## Installation

```bash
blender --command extension build --source-dir blender_addon/dynamic_skinning --output-dir .
```

Puis, dans Blender : *Edit > Preferences > Get Extensions > ▾ > Install from Disk…* et choisir `dynamic_skinning-0.1.0.zip`.

## Fichiers d'exemple

Avec l'extension installée, ouvrir un fichier de `blender_addon/examples/` puis lancer la lecture (Espace) :

- **`cylinder.blend`** : le cylindre des applications ; l'os du bout tourne de 90°, pause, revient.
- **`spot.blend`** : Spot la vache (`assets/spot/`, texture incluse dans le fichier), avec l'animation des applications : la tête hoche, puis tourne de chaque côté et les oreilles oscillent.

Le dynamic skinning redémarre tout seul à l'ouverture du fichier. L'armature est en Pose Mode : on peut aussi tourner un os à la main (R). Sans l'extension, le maillage s'affiche sans dynamic skinning.

Les fichiers sont générés par `blender -b --factory-startup --python blender_addon/tests/make_examples.py`.

## Utilisation sur ses propres modèles

1. Sélectionner un maillage qui a un modificateur Armature (les groupes de sommets portent le nom des os).
2. *Enable*, dans *Properties > Object > Dynamic Skinning* (le maillage ou son armature sélectionné), ou dans *3D View > Sidebar (N) > Dyn. Skinning*. Les deux panneaux contiennent les mêmes réglages.
3. Passer l'armature en Pose Mode et tourner un os (R) : le maillage suit avec retard, puis oscille.

Paramètres (notations de l'article) : fréquence **f** (Hz), amortissement **τ** (s) et amplitude **A** du noyau κ(t) = A exp(−t/τ) sin(2π f t), drag **D**, et intensité du bend **γ**. Un groupe de sommets `DS_bend` (optionnel) donne le poids de bend par sommet, qu'on peut peindre ; sans ce groupe, il vaut 0.5 partout, comme dans les applications.

*Reset* recalcule les poids après une modification du rig ou du maillage, et arrête les oscillations.

## Fonctionnement

- **`core.py`** : la méthode, en numpy et sans dépendance à Blender (mêmes calculs que `dynamic_skinning_source/`) : poids hiérarchiques β_ij, vitesse angulaire de chaque os mesurée à partir de sa rotation locale, historique à pas fixe Δt = 1/60 s (indépendant du framerate), convolution avec κ, puis bend.
- **`blender_link.py`** : lecture du rig et de la pose, LBS des positions, écriture du décalage de chaque sommet dans l'attribut `ds_offset` du maillage. Un modificateur Geometry Nodes *Dynamic Skinning*, ajouté en fin de pile (après l'Armature), l'ajoute aux positions. Un timer à 60 Hz fait la mise à jour.
- **`__init__.py`** : propriétés, opérateurs et panneau.

L'angle du bend est calculé sur le modèle ramené dans [−1, 1] (demi-taille du maillage au repos) : le résultat ne dépend pas de l'échelle du modèle dans Blender, et les valeurs des paramètres sont celles des applications.

Coût d'une mise à jour (Python et numpy, Mac de développement, cylindre à 2 os, sans la réévaluation du modificateur par Blender) : 0.5 ms pour 1 440 sommets, 4 ms pour 25 600, 15 ms pour 102 400.

## Limites du prototype

- Le temps est le temps réel : le résultat d'une lecture d'animation dépend de la vitesse de lecture, et il n'y a pas encore de « bake » pour un rendu final reproductible.
- Le LBS est recalculé en numpy (pas de Dual Quaternion, pas de shape keys avant l'Armature).
- Pas de délai (dLBS), pas d'origines flexibles, pas de version GPU.

## Tests

```bash
# Test headless : cylindre riggé, os tourné de 90° en 0.7 s, rendus Workbench dans <dossier>
blender -b --factory-startup --python blender_addon/tests/test_cylinder.py -- <dossier>
# Démos avec l'interface, sans installer l'extension : cylindre ou Spot, prêts à manipuler en Pose Mode
blender --factory-startup --python blender_addon/tests/demo_cylinder.py
blender --factory-startup --python blender_addon/tests/demo_spot.py
# Ouverture d'un fichier d'exemple et lecture, avec captures du viewport dans <dossier>
DS_AUTOTEST=<dossier> blender --factory-startup --python blender_addon/tests/test_open_example.py -- blender_addon/examples/spot.blend
```

`make_spot.py` contient un petit importeur du format des assets des applications (`mesh.obj`, `skeleton_*`, `rig_joint`, `rig_weights`, `texture.png`), réutilisable pour les autres modèles.
