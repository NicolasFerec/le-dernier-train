# Sprites du combattant

Le combattant de départ à l’épée utilise un AnimatedSprite2D et une ressource SpriteFrames. Le prototype charge le pack **2D Pixel Prototype: Top-Down Characters**, version Godot 4, variante couleur 64 px. Art par [Mythryl Studio](https://mythryl.com/assets/pixel-prototype-characters-topdown).

## Installation locale

Extraire le dossier `64px` de l’archive Godot dans `assets/mythryl-local/`. Laisser `godot/colour.tres` à côté du dossier `colour/` dans `64px` pour que ses chemins relatifs restent valides. Ouvrir le projet dans Godot pour importer les PNG. Le pack est ignoré par Git. Sans ce dossier, le combattant utilise son dessin procédural.

## Format de remplacement

| Élément | Format du prototype |
| --- | --- |
| Texture | PNG RGBA, une planche par mouvement, colonnes dans l’ordre de lecture |
| Cellule | 128 × 128 px, personnage d’environ 64 px |
| Ancrage au sol | (64, 95) dans chaque cellule. Sprite centré avec un décalage de (0, −31). |
| Lignes, de haut en bas | S, SW, W, NW, N, NE, E, SE. Directions à l’écran après projection isométrique. |
| Garde | `sword-ready-<direction>`, en boucle à 8 i/s |
| Course | `run-<direction>`, en boucle à 15 i/s |
| Esquive | `roll-<direction>`, jouée une fois sur les 160 ms de l’esquive |
| Attaque | `sword-slash-<direction>`, jouée une fois sur les 160 ms de frappe |

L’épée est incluse dans les images de garde et de frappe du prototype. Garder le point d’appui stable et laisser de la marge pour la lame. Le nombre de colonnes peut varier selon le mouvement. Dans l’éditeur, associer les cellules à une ressource SpriteFrames qui expose les mêmes noms. La garde et la course utilisent la cadence de la ressource. L’esquive et la frappe échantillonnent la progression de l’action du jeu sur toute la séquence. Le jeu applique les dégâts à 0 ms. Pour une future planche, placer la pose de contact en première image tant que ce timing est en vigueur. À cette cadence, une séquence trop longue saute des poses à l’écran.

Dans le wagon, équiper le combattant avec l’épée de départ. Vérifier la garde à l’arrêt, la course, l’esquive et la frappe dans les huit directions. Contrôler visuellement que l’appui reste au sol et que le contact sur un mannequin correspond à la frappe.

Pour une commande à un outil IA, transmettre la spécification et un gabarit vierge créé pour le projet. La licence Mythryl exclut l’usage de ses visuels comme référence pour les outils de génération.
