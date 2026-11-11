# Assets du Dernier Train

| Dossier | Contenu |
| --- | --- |
| [art-source/characters](art-source/characters) | Illustration du conducteur et de l’automate, avec leurs deux versions détourées. |
| [art-source/conductor](art-source/conductor/README.md) | Planches sources 2D du conducteur, des animations et des armes. |
| [art-source/environments](art-source/environments) | Concepts du wagon isométrique, du wagon-atelier et de la station de pompage. |
| [art-source/menu](art-source/menu) | Fonds originaux du dépôt et illustration de la locomotive. |
| [art-source/materials](art-source/materials) | Textures sources du plancher et de la tôle. |
| [art-source/models](art-source/models) | Conducteur GLB avant articulation et images exactes utilisées pour générer les trois modèles. |
| [models/conductor](models/conductor) | GLB du conducteur articulé, du sabre et du marteau, avec leurs textures et métadonnées Godot. |
| [menu](menu) | Fonds utilisés par le menu principal. |
| [../player/conductor](../player/conductor) | Atlas, prises en main et textures utilisées par la version 2D. |

Le dossier `art-source` est ignoré par l’importateur Godot. Les fichiers restent accessibles pour préparer de nouveaux exports. Pour un essai dans un autre moteur, partir des GLB dans `models/conductor`. Les réglages de caméra, de prise et de course propres à Godot sont décrits dans [player/character-sprites.md](../player/character-sprites.md).

Les illustrations ont été générées dans ImageGen. Les modèles texturés ont été générés dans Layer avec Tripo v3.1, et le conducteur a été articulé avec Tripo Rig. Les sessions Layer permettent de retrouver les générations du [conducteur](https://layer.ai/ferecium-studio/sessions/213784c1-8437-4e98-845c-2c4bad7ac55e), du [sabre](https://layer.ai/ferecium-studio/sessions/4749bbd8-c97a-40ce-ab3b-712d6f72f841) et du [marteau](https://layer.ai/ferecium-studio/sessions/862cc3bd-04c8-42b5-b891-eef34eedc0ec).
