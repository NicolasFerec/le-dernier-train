# Conducteur — rig UE5 et animations, 12 novembre 2026

Le modèle existant du Conducteur est conservé. Son ancien rig a été retiré avant l’envoi à Meshy ; aucune nouvelle génération de personnage ni de décor n’a été achetée pour cette passe.

## Tâches et coût constaté

Le fichier `tasks.json` conserve les identifiants sans URL signée.

| Ressource | Tâche | Paramètres | Crédits consommés |
| --- | --- | --- | --- |
| `rigging` | `01a1cf4e-9874-775f-8b62-33306e1ae408` | `skeleton_template=ue5_mannequin`, `include_fingers=true`, hauteur 1,80 m | 5 |
| `animate` | `01a1cf4f-c6cc-7151-80f8-3845fe455d38` | actions 11, 219, 97, 242, 128, 158, 178, 8 sur ce nouveau rig | 24 |

Total : **29 crédits**, solde observé ensuite : **794**. La marche et la course étaient incluses dans la sortie du rig. Les fichiers téléchargés sont `Rig/rigged_character.glb`, `Rig/rigged_character.fbx`, `Rig/Walk.glb`, `Rig/Run.glb` et `Actions.glb`.

`Submitted/conductor-rig-input.glb` est exactement le fichier envoyé au service. Le script de préparation a ensuite été corrigé pour exclure les objets de visualisation des os de Blender du calcul des dimensions ; ce correctif local n’a déclenché aucune nouvelle tâche payante. `conductor-rig-input.glb` et `input-preview.png` à la racine de ce dossier ont été reconstruits avec cette correction pour un futur usage ; ils n’ont pas été renvoyés au service. Le rig retourné a été mesuré et normalisé à 180 cm pour Unreal.

## Squelette et limite des mains

Meshy a bien reçu le template **ue5_mannequin** et l’option doigts. Il a retourné **42 os**, auxquels la préparation ajoute une racine de sol : **43 os dans Unreal**. Les noms du corps suivent le mannequin (`pelvis`, `spine_01`, `hand_r`, etc.). Ce résultat n’est pas une copie intégrale du squelette Manny d’Epic.

Les mains de l’ancien modèle sont sculptées en poings. Le résultat possède une chaîne de majeur par main et quelques os auxiliaires, mais **pas cinq doigts indépendants**. La prise de l’arme est donc calibrée sur le gant et l’ancien repère anatomique ; il ne faut pas présenter cet asset comme un rig complet des doigts. Des mains ouvertes et séparées dans la géométrie source sont nécessaires pour reprendre ce point proprement.

## Préparation et intégration

1. `Scripts/prepare_conductor_ue5.py` traite les GLB avec Blender : échelle en centimètres, hauteur 180 cm, base au sol, racine supplémentaire et export FBX séparé pour chaque clip. Il normalise aussi les translations des clés d’animation, pas seulement la pose de référence.
2. `Scripts/import_conductor_ue5.py` importe dans `/Game/Characters/ConductorUE5`. Tous les clips utilisent le **même squelette que le modèle**, sans retarget depuis l’ancien rig Mixamo. Le matériau existant `/Game/Characters/Conductor/M_Conductor` est conservé.
3. Les axes du pack d’actions diffèrent de ceux de la locomotion livrée avec le rig. L’import recale le bassin et la racine sur la même orientation avant de bloquer la translation. Cela corrige la rotation latérale provoquée par le verrouillage de racine à l’exécution.
4. Le repos utilise `Idle_02`, les attaques deux coupes et une frappe chargée, le marteau une frappe lourde ; roulade, réaction et mort viennent aussi du nouveau pack. La frappe légère au marteau réutilise la coupe légère. La course reçoit une pose de bras droit adaptée au port de l’arme. Les clips ne sont pas tous de nouvelles chorégraphies : plusieurs actions de catalogue existaient déjà, mais sont maintenant générées et intégrées directement sur ce rig.
5. Les contacts des coups sont recalés sur les délais de dégâts existants. Character Movement et Gameplay Abilities conservent le déplacement et les actions. La cadence de course utilise une vitesse de pied planté mesurée à environ **483 cm/s**.

L’ancien dossier `/Game/Characters/Conductor` reste disponible pour ses textures et pour comparaison. Le chargement du personnage principal pointe désormais sur `ConductorUE5` ; l’Éclaireuse et les monstres ne sont pas remplacés par cette passe.

## Vérification reproductible

`Scripts/validate_conductor_ue5.py` vérifie les onze clips, le squelette commun, les unités, la racine, le sens du corps au repos/en locomotion et les appuis des attaques. Les mesures sont dans `validation.json` ; les chemins importés dans `import-report.json`.

Le mode de développement `-TrainConductorReview -ExecCmds=Train.ConductorReview`, lancé avec `Scripts/play-quai-blockout.sh`, capture réellement Unreal : repos, course, coupes, attaque lourde, changement vers le marteau et roulade. Il utilise un profil isolé et une simulation fixe à 30 Hz, sans modifier la sauvegarde du joueur. Cette capture sert à juger les animations ; ce n’est pas une mesure de performances. Les résultats et vidéos restent dans `Saved/ConductorUE5Review`.

Validation du 12 novembre : compilation locale réussie ; validation des onze clips et contrôle général des assets réussis. Les six tests natifs passent. Le test `WagonAndRun` a été adapté aux noms `hand_r`, `head`, `foot_r`, puis relancé avec succès ; les cinq autres tests passaient dès le premier lancement. Le test de coopération dans deux processus passe aussi, avec un écart de position maximal de 0,79 cm dû à la quantification réseau. Cela ne constitue pas un test Steam sur deux machines.

`Saved/ConductorUE5Review/conductor-review.mp4` est un recadrage de la capture en jeu validée ; `conductor-v2.mp4` conserve le cadre complet. Les extraits des journaux sont dans `validation-excerpts.txt`.

Une mesure séparée avec le rendu actif, sortie 2560 × 1440 et échelle de rendu 3D automatique, donne 37,41 FPS dans le wagon, 46,60 FPS dans la salle 6 (16 ennemis) et 41,78 FPS dans la salle 10 (22 ennemis). Pic mémoire du processus : 2 569 Mio. Ce n’est pas du 60 FPS stable. Résultats détaillés : `Saved/Benchmarks/conductor-ue5-native.csv`. La précédente mesure `native-auto.csv` utilisait l’Éclaireuse, dont le modèle est différent : ce n’est pas une comparaison avant/après isolant ce changement de rig.
