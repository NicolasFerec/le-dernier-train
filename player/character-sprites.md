# Sprites du conducteur

Le conducteur utilise un atlas peint avec sept états et huit directions. Les sources sont conservées dans [assets/art-source/conductor](../assets/art-source/conductor/README.md).

| Élément | Format |
| --- | --- |
| Atlas | `conductor/poses.png`, PNG RGBA. Cellules de 128 × 128 px. |
| Colonnes | S, SW, W, NW, N, NE, E, SE, dans l’espace écran après projection. |
| Rangées | Quatre rangées réservées à chaque état dans l’ordre garde, course, esquive, frappe légère, frappe lourde, choc et chute. |
| Poses | Garde statique, quatre poses pour la course, l’esquive et les frappes, deux pour le choc et la chute. |
| Appui | Point (64, 95) de chaque cellule. La hauteur de référence du conducteur debout est de 64 px. |
| Prises | `conductor/grips.gd` contient les décalages de main relatifs à l’appui pour chaque pose armée et les angles de port par image de course. |
| Armes | PNG RGBA séparés. Poignée au point (32, 32), arme orientée vers la droite. |

La course boucle à 12 images par seconde. L’esquive et les frappes suivent la progression de l’action de jeu. La frappe reprend la direction mémorisée au départ du coup, et l’arme vise le bord du balayage utilisé pour les impacts. La prise change avec la pose du corps. Le sabre et le marteau partagent les animations. L’arme est masquée pendant l’esquive, le choc et la chute.

## Essai dans le wagon peint

L’arsenal est à portée d’interaction dès l’arrivée dans le wagon. Le sabre et le marteau sont disponibles sans dépenser de points pour cet essai. L’Éclaireur, lorsqu’il est débloqué, a également accès à ses deux armes. Rejoindre les mannequins à droite de la travée, puis courir, esquiver et essayer les deux frappes vers le haut à droite et vers le bas à gauche. Au contact, le mannequin clignote et le conducteur conserve ses points de vie.

Dans ces deux orientations, `workshop_sabre.png` et `workshop_marteau.png` dessinent l’arme dans chaque pose. Chaque atlas a quatre colonnes et huit rangées. Pour chaque orientation, les rangées sont la course, le dash, la frappe légère et la frappe lourde. La première pose de frappe légère sert aussi de garde. Les six autres orientations utilisent l’atlas précédent. Les [planches ImageGen et le script d’assemblage](../assets/art-source/conductor/workshop/README.md) permettent de refaire les atlas.

## Comparaison avec le rendu 3D

Depuis le dossier du projet, lancer `godot --path . -- --conductor-3d`, puis choisir Jouer. L’essai montre le conducteur local en 3D au repos et en course dans le wagon. Le même trajet avec `godot --path .` utilise les sprites peints. Les deux essais utilisent les commandes et l’armurerie habituelles.

Les modèles sont rangés dans [assets/models/conductor](../assets/models/conductor). Le modèle texturé du conducteur a été généré par Tripo v3.1 dans Layer à partir de la vue de face de la [planche du conducteur](../assets/art-source/conductor/reference.png), puis articulé avec Tripo Rig. Godot anime le squelette, attache l’arme à la main droite et affiche la scène 3D à travers un SubViewport transparent. Le sabre et le marteau sont des modèles GLB texturés générés par Tripo v3.1 à partir des illustrations des armes. Chaque modèle a son propre point de prise et sa propre échelle. La course est procédurale. Les attaques, l’esquive et la réaction aux dégâts utilisent encore les sprites peints.
