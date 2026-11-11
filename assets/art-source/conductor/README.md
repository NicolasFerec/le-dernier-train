# Conducteur — planches sources

Les PNG ont été générés avec ImageGen à partir du conducteur de référence. Ce dossier est ignoré par Godot. Le portrait livré dans `player/conductor_portrait.png` est importé séparément pour le HUD.

| Planche | Contenu et contrôle à effectuer |
| --- | --- |
| `reference.png` | Quatre vues du même costume sans arme. |
| `idle.png`, `idle-sw.png` | Garde. La planche principale contient deux trois-quarts avant vers la droite. La vue isolée fournit le trois-quarts gauche et demande un détourage du halo. |
| `run.png`, `dodge.png` | Quatre poses par direction, huit rangées. Vérifier la direction et poser les pivots au sol avant de construire les boucles. |
| `light.png`, `heavy.png` | Quatre poses de frappe à une main, sans arme. Certaines rangées orientées vers la gauche doivent être remplacées. |
| `attacks-west.png` | Corrections vers le bas-gauche, la gauche et le haut-gauche. Pour chaque direction, la frappe légère précède la lourde. Détourer le halo avant la découpe. |
| `hit-fall.png` | Deux poses de réaction et deux poses de chute par rangée. Vérifier les orientations avant import. |
| `weapons.png` | Trois variantes de sabre et trois variantes de hachette, avec la poignée à gauche. La hachette est une proposition visuelle. |
| `hammer.png` | Marteau à piston pour l’autre arme déjà jouable. |

## Consignes de génération

Référence d’identité constante, costume bordeaux et cuivre, gilet bleu-vert, épaulière droite, caméra orthographique élevée, même échelle, pieds alignés, mains vides, fond alpha, sans décor, ombre ni texte. Pour les planches de huit rangées, ordre demandé du haut vers le bas **S, SW, W, NW, N, NE, E, SE**, à confirmer image par image.

- Course : contact pied gauche, passage, contact pied droit, passage.
- Esquive : accroupissement, regroupement, roulade basse, récupération.
- Frappe légère : préparation, contact, accompagnement, récupération.
- Frappe lourde : préparation basse, poing levé, frappe descendante, récupération.
- Réaction : recul, récupération, genoux à terre, chute.
- Armes : acier et cuivre peints, poignée à gauche, silhouette lisible en petit.
- Portrait : buste carré, trois-quarts avant, visage et casquette lisibles à la taille du HUD.

## Atlas utilisé dans Godot

Les scripts [pack.py](pack.py) et [workshop/pack.py](workshop/pack.py) exportent les planches à deux pixels par unité de jeu avec Pillow. Les poses et armes détourées se trouvent dans [player/conductor](../../../player/conductor). Le [format de l’atlas](../../../player/character-sprites.md) décrit l’ordre des cellules, les points de prise et le test dans le wagon.
