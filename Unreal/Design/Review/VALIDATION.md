# Validation locale — Faubourgs, rendu et personnages

12 novembre 2026, Unreal Engine 5.8.4, version de développement Linux. Les captures et les vidéos de ce dossier proviennent du moteur, avec un cadrage rapproché pour examiner les animations. Les références ImageGen du dossier `../Faubourgs/References` sont des intentions artistiques, pas des captures du jeu.

## Résultats

- Compilation C++ Development réussie.
- Six tests natifs réussis : `Gameplay.WagonAndRun`, `Generation.EncounterBudgets`, `Generation.RoomSeeds`, `Input.ActionHints`, `Input.KeyboardLayouts` et `Network.FourPlayers`.
- Coopération hôte/invité dans deux processus distincts réussie : mêmes salles, obstacles et ennemis ; écart maximal des obstacles répliqués de 0,792 cm. Il s’agit du pilote réseau local d’Unreal, pas d’un test Steam entre deux machines.
- Contrôle général des assets de combat réussi : orientations des contacts, appuis, matériaux PBR et normal maps concernées.
- Conducteur : onze clips sur le squelette UE5 importé ; orientation commune de la racine, sens de locomotion et appuis contrôlés. Le paramètre Meshy `ue5_mannequin` a été utilisé, mais le modèle aux poings fermés n’a produit qu’une articulation partielle des doigts.
- Éclaireuse : nouvelle boucle de repos vérifiée sur 121 poses, deux appuis fixes, flexion des genoux entre 17,4° et 19,6°, continuité de la boucle et respiration. Les retours au repos après déplacement, attaque et esquive ont été capturés.
- Carte pilote : le [rapport du blockout](../Faubourgs/Review/VALIDATION.md) décrit les parcours vérifiés, les points de départ, les ennemis accessibles et la transition vers la salle suivante.

Les journaux réduits sont dans `scout-validations.txt` et `conducteur-validations.txt`. `scout-idle-avant-apres.png`, `scout-idle.mp4` et `conducteur-ue5.mp4` permettent la revue visuelle. La validation porte sur le jeu de développement local ; aucun paquet distribué n’a été revalidé pour cette PR. La configuration de cook inclut le nouveau dossier `ConductorUE5`.

## Performances et état artistique

`performances.csv` conserve la mesure avec le Conducteur, sortie 2560 × 1440 et échelle de rendu 3D automatique : 37,41 FPS dans le wagon, 46,60 FPS en salle 6 avec 16 adversaires et 41,78 FPS en salle 10 avec 22 adversaires. Le pic mémoire du processus est de 2 569 Mio. Ce n’est pas du 60 FPS stable.

La carte `QuaiBlockout` est une composition pilote faite de volumes simples. Le parcours des dix salles est documenté ; l’habillage définitif des bâtiments et du train reste à réaliser. L’équilibrage et la qualité des animations doivent encore être évalués en jeu par le joueur ; la réussite des tests ne constitue pas une validation artistique globale.
