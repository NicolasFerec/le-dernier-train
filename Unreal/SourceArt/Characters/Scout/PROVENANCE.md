# L’Éclaireuse — 12 novembre 2026

Direction artistique : éclaireuse ferroviaire adulte, veste courte bleu pétrole, foulard ocre rentré, pantalon et bottes de cuir brun, épaulière de cuivre, carré auburn et lunettes sur le front. Silhouette distincte du Conducteur rouge, proportions stylisées, mains vides en A-pose pour le rig. Référence créée avec ImageGen, conservée dans `Reference.png`.

## Génération Meshy

- Image-to-3D : `01a1cef3-18dd-7105-a334-1d853e40d80f`, smart topology meshy-t2.5, cible 12 000 triangles, résultat 12 674 triangles. Texture meshy-7 2K avec PBR (base color, normal, metallic/roughness), 15 crédits.
- Rig final : `01a1cf01-0890-7434-b67c-88803386afc2`, **skeleton_template=ue5_mannequin**, **include_fingers=true**, hauteur 1,75 m, 5 crédits. `UE5Rig/` contient le GLB/FBX, la marche et la course livrées avec ce rig.
- Pack d’actions avec doigts : `01a1ceff-3321-74c7-a071-7a21871c3d07`, actions `0,219,97,240,158,8,174,91`, 24 crédits. Retarget natif Unreal IK de ce pack vers le rig UE5 ; il n’est pas directement assigné avec son ancien squelette.
- Essais précédents conservés localement dans `Saved/Checkpoints/ScoutGenerationTrials` : rig meshyrig sans doigts (5), premier pack (24), rig meshyrig avec doigts (5). Coût total de cette itération : **78 crédits**, solde Meshy observé après génération : 823.

## Intégration

`Scripts/prepare_scout.py` normalise la hauteur à 175 cm, pose la base au sol et ajoute une racine de sol au rig fourni (70 os source, 71 avec cette racine). Il extrait les textures du GLB, sans les repeindre. Le template Meshy porte les noms d’os Unreal ; ce n’est pas une copie intégrale du squelette Manny livré par Epic.

`Scripts/import_scout.py` importe le modèle tourné vers +X, crée le matériau PBR, convertit la normale glTF/OpenGL en convention DirectX, limite les textures à 2048, puis retargete les animations avec les chaînes IK natives, doigts compris. Les clips de combat sont recentrés sur la capsule et leur contact est aligné avec le moment des dégâts. La vitesse de course est ajustée sur une vitesse de pied planté mesurée à environ 476 cm/s.

Les armes sont attachées aux os `hand_r` et `hand_l`, avec orientation et placement calculés depuis les articulations des doigts. Le modèle, les animations et le portrait sont sélectionnés par le chargement d’équipement répliqué. Les essais graphiques et leurs profils restent dans `Saved/ScoutReview` et `Saved/TestSettings`.

## Correction du repos — 12 novembre, après retour en jeu

La boucle `Idle` importée plaçait le pied gauche environ 40 à 45 cm devant le droit. Le pied droit pivotait et descendait sur sa pointe pendant le balancement du buste. Le contrôle précédent, fondé sur la hauteur du **pied le plus bas**, ne pouvait pas détecter cette mauvaise posture de repos. La capture de l’utilisateur a été reproduite dans le wagon avant modification.

`Scripts/build_scout_idle.py` remplace uniquement cette boucle par une animation locale de quatre secondes sur le squelette déjà importé. Les chevilles et les avant-pieds restent à leur position de référence. Une résolution analytique des deux articulations de chaque jambe donne une flexion modérée des genoux vers l’avant, avec un bassin abaissé de 1 cm. Une respiration discrète anime le torse et la tête ; les mains conservent leur prise. Le résultat est enregistré dans les clés de l’animation, sans calcul de placement des pieds à l’exécution. Aucun changement du modèle ni nouvelle dépense Meshy.

`Scripts/import_scout.py` applique désormais cette étape après l’import pour que la correction survive à une reconstruction. L’ancien asset reste sauvegardé dans `Saved/Checkpoints/ScoutIdleRepair/Idle.uasset`.

`Scripts/validate_scout_idle.py`, également appelé par le contrôle général des assets de combat, vérifie 121 poses : les deux appuis et les avant-pieds, le sens et la flexion des genoux, le bassin au-dessus des appuis, la présence de respiration et la continuité de la boucle. Résultats dans `idle-validation.json` : décalage avant/arrière entre les pieds inférieur à 0,02 cm, flexion des genoux entre 17,4° et 19,6°, aucun glissement visible des appuis.

Le mode de développement `Train.ScoutReview`, activé uniquement avec `-TrainScoutReview`, enregistre le personnage dans le wagon avec un cadrage rapproché, dos puis face, suivi d’un déplacement, d’attaques et d’une esquive avec retours au repos. Il ne modifie pas la sauvegarde du joueur. Les captures avant/après et la vidéo sont dans `Saved/ScoutIdleReview`. Le mesh observé à l’exécution possède 72 os après import FBX (la préparation Blender ci-dessus en compte 71).

Compilation locale réussie, contrôle des assets réussi, six tests natifs réussis et coopération dans deux processus réussie après correction. Les retours au repos après course, attaque et esquive ont été capturés dans Unreal. Les extraits des journaux sont conservés dans `Saved/ScoutIdleReview/validation-excerpts.txt`. Les tests réseau sont locaux ; le test Steam réel sur deux machines reste distinct.
