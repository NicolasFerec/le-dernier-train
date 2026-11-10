# Combattant · premier sprite d’attaque

Essai du combattant au manteau rouille et à l’épée de départ (personnage 0, arme 0). Une seule orientation, vers le bas et la droite de l’écran, correspondant à Vector2.RIGHT sur le plan du jeu. Le corps actuel mesure environ 52 unités à l’écran avant agrandissement de la fenêtre.

| Propriété | Livraison |
|---|---|
| Format | PNG RGBA, atlas 2 × 2 de 256 × 192 px |
| Cellule | 128 × 96 px, rendue à l’échelle 1:1 dans le viewport |
| Ancrage | (48, 72) dans chaque cellule, au centre de l’appui au sol |
| Ordre | Contact, prolongement, retour, garde |
| Horloge | 4 poses de 40 ms sur les 160 ms du balayage existant |
| Dégâts | Résolus au déclenchement de l’attaque, à 0 ms. La première pose montre donc le contact. |
| Récupération | Retour au dessin habituel à 160 ms. La prochaine attaque reste disponible à 360 ms. |

Le sprite contient le corps et l’épée. Le jeu dessine l’ombre, l’indicateur d’équipe et l’arc de portée. Les autres orientations et armes gardent leur rendu procédural.
