# Sources du prototype Unreal

## Passe visuelle et combat du 12 novembre 2026

- `Animations/Meshy/Combat.fbx` : huit mouvements Meshy (219, 97, 242, 128, 158, 210, 214, 174), tâche `01a1cd33-00b0-765e-8ad2-ee477443d967`, 24 crédits. Le rig source est `01a1ccf8-7dcf-7547-bd21-dd8a557b6ca5` ; ses exports de marche et course sont conservés dans le même dossier. Les mouvements sont reciblés sur les personnages existants, puis leurs contacts et appuis sont recalés dans Unreal.
- `References/WorkshopBench.png` : référence ImageGen originale. `Props/WorkshopBench.glb` : conversion Meshy image vers 3D, tâche `01a1cd43-2248-762c-8f21-7b7ba275e9d2`, 15 crédits, textures PBR 2K. Le GLB contient une normal map et une texture metallic/roughness. Le matériau importé est conservé dans Unreal.
- `Environment/QuayFlagstone.png` : texture de dalles de quai ImageGen originale. `QuayFlagstoneNormal.png` : relief léger dérivé de cette image, utilisé avec les matériaux à coordonnées monde pour éviter les étirements selon la taille des quais.
- `Effects/` : géométries originales de traînée et d’impact, générées par `Scripts/build_combat_effects.py`.

- `Characters/Scout/` : nouvelle Éclaireuse ImageGen → Meshy, rig final `ue5_mannequin` avec doigts, textures PBR 2K et dix animations intégrées. Les identifiants, coûts et étapes sont détaillés dans `Characters/Scout/PROVENANCE.md`.
- `Characters/Conductor/UE5Rework/` : nouveau rig Meshy `ue5_mannequin` du Conducteur existant et huit actions générées sur ce rig, marche/course incluses. Coût 29 crédits. Les poings sculptés n’ont permis qu’une articulation partielle des doigts. Voir [la provenance, les paramètres et la chaîne d’import](Characters/Conductor/UE5Rework/PROVENANCE.md).

## Sources précédentes

- Conducteur, sabre et marteau : modèles Tripo v3.1 issus des essais Layer du prototype historique. [Conducteur](https://layer.ai/ferecium-studio/sessions/213784c1-8437-4e98-845c-2c4bad7ac55e), [sabre](https://layer.ai/ferecium-studio/sessions/4749bbd8-c97a-40ce-ab3b-712d6f72f841), [marteau](https://layer.ai/ferecium-studio/sessions/862cc3bd-04c8-42b5-b891-eef34eedc0ec).
- Automate : modèle Tripo et rig Uthana, créés à partir de l’illustration de référence. [Session Layer](https://layer.ai/ferecium-studio/sessions/47ff9214-9076-431d-b3fe-940e2f2233bf). Les GLB source et articulé sont conservés séparément.
- Animations : éditions Standard CC0 de [Universal Animation Library](https://quaternius.com/packs/universalanimationlibrary.html) et [Universal Animation Library 2](https://quaternius.itch.io/universal-animation-library-2) de Quaternius. Le dossier de chaque source contient sa licence. Les animations ont été retargetées par Unreal sur les deux squelettes.
- Mobilier, machinerie et mannequins : modèles originaux, éditables dans Blender. `Scripts/build_props.py` décrit les assemblages, matières et détails. Les FBX voisins sont les exports pour Unreal.
- Illustrations, portraits, textures de sol et sons : repris des assets du projet Godot. Pictogrammes de commandes : Kenney Input Prompts, CC0, voir `InputPrompts/LICENSE.txt`.
- Surfaces du wagon peint : `Environment/WagonBoardsPainted.png`, `Environment/WagonSteelPlatePainted.png` et `Environment/WagonValveBlueprint.png` proviennent de l’outil ImageGen intégré, à partir de l’illustration du wagon fournie comme référence de style. Les prompts demandent respectivement des planches de noyer gris-brun et leurs joints encrés, une plaque de sol bleu ardoise usée avec huit rivets et un plan cyanotype de soupape de locomotive dessiné à l’encre crème. Les trois textures sont vues à plat, sans perspective ni lumière directionnelle. Les normales tangent espace sont produites par `Scripts/bake_wagon_normals.py` à partir des joints, biseaux et rivets. `Scripts/style_wagon.py` importe les textures, crée les matériaux et applique la scène.
