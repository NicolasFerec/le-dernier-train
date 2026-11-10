# Combattant · premier sprite d’attaque

Essai du combattant au manteau rouille et à l’épée de départ (personnage 0, arme 0). La planche fournit une vue de trois quarts utilisée à droite et symétrisée à gauche. Le prototype réutilise cette vue pour toutes les orientations. Le corps actuel mesure environ 52 unités à l’écran avant agrandissement de la fenêtre.

| Propriété | Livraison |
|---|---|
| Format | PNG RGBA, atlas 2 × 2 de 256 × 192 px |
| Cellule | 128 × 96 px, rendue à l’échelle 1:1 dans le viewport |
| Ancrage | (48, 72) dans chaque cellule, au centre de l’appui au sol |
| Ordre | Contact, prolongement, retour, garde |
| Horloge | 4 poses de 40 ms sur les 160 ms du balayage existant |
| Dégâts | Résolus au déclenchement de l’attaque, à 0 ms. La première pose montre donc le contact. |
| Récupération | Retour à la pose de garde à 160 ms. La prochaine attaque reste disponible à 360 ms. |

Le sprite contient le corps et l’épée. Le jeu dessine l’ombre, l’indicateur d’équipe et l’arc de portée. La garde sert aussi pendant le déplacement et l’esquive. Les autres armes et personnages gardent leur rendu procédural.
