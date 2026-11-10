# Arène coop

Prototype d’action 2D en GDScript pour Godot 4.7.2 Standard.

```sh
godot --editor --path .   # Ouvrir le projet dans l’éditeur
godot --path .            # Ouvrir le salon, prêt pour le solo
```

Va au poste **Départ**, appuie sur **E**, puis clique sur **Lancer la partie** pour jouer seul. En coop, lance une deuxième instance avec un profil séparé :

```sh
godot --path . -- --profile=invite
```

1. Clique sur **Héberger** dans la première fenêtre et sur **Rejoindre** avec `127.0.0.1` dans la seconde. Le salon affiche **2/4 joueurs**. L’hôte lance la partie depuis le poste **Départ**.
2. Déplace-toi avec **ZQSD**, **WASD** (touches physiques) ou les flèches. **J** attaque dans la direction du regard. **Espace** déclenche une esquive.
3. Élimine tous les ennemis. L’hôte clique sur **Salle suivante**, puis sur **Réclamer la récompense** à la fin de la deuxième salle. Chaque joueur reçoit trois points et revient au salon. Si tout le groupe tombe, l’hôte peut retourner au salon.
4. Au salon, approche-toi du poste **Personnage**, **Arsenal** ou **Passifs** et appuie sur **E** pour acheter ou t’équiper et lance une nouvelle partie pour essayer ton équipement. Les achats et les choix sont sauvegardés dans le profil local Godot (`user://`).
5. Pour terminer la session coop, clique sur **Quitter la session** chez l’hôte. Le client revient à son salon solo.

ENet utilise le port UDP **7000**, avec trois places client. Les connexions s’effectuent au salon, entre les parties. L’hôte simule les combats et décide des transitions de salle.

Les sprites du décor, des personnages et de l’ennemi viennent de [Tiny Dungeon par Kenney](https://kenney.nl/assets/tiny-dungeon), sous licence CC0. La licence accompagne les fichiers dans `assets/tiny_dungeon/`.

## Coop Steam

Installe [GodotSteam 4.24 GDExtension](https://codeberg.org/godotsteam/godotsteam/releases/tag/v4.24-gde) en extrayant le dossier `addons/godotsteam` de l’archive à la racine du projet. Lance l’éditeur une fois pour importer l’extension. Les binaires de l’extension sont installés localement et ignorés par Git.

Pour tester, ouvre le client Steam et connecte-toi sur deux machines avec deux comptes amis. Lance la même version du jeu sur chacune :

```sh
godot --path .
```

L’App ID **480** correspond à Spacewar, l’application de test publique de Steam. Il est configuré dans la section `[steam]` de `project.godot`. Remplace-le par l’App ID Steamworks du jeu quand il sera disponible.

1. L’hôte clique sur **Héberger Steam**.
2. **Inviter / copier le code** ouvre le sélecteur d’amis si l’overlay Steam est disponible et copie le code du salon. Accepte l’invitation avec le jeu ouvert. Si l’overlay est indisponible, transmets le code à ton ami pour qu’il le colle dans le champ d’adresse et clique sur **Rejoindre Steam**.
3. Vérifiez que le salon affiche **2/4 joueurs**. L’hôte lance la partie depuis le poste **Départ**.

Le salon est réservé aux amis et accepte les connexions entre les parties. Le solo et la coop par adresse restent accessibles sans Steam.

## Langues

Le jeu démarre en français. Le sélecteur en haut à droite permet de passer à l’anglais et mémorise le choix. Les dialogues, l’interface et les messages du jeu utilisent le catalogue `locale/game.csv`. Renseigne les colonnes `fr` et `en` à chaque ajout ou modification de texte.

## Publication et mises à jour

Pour publier une version stable, mets à jour `application/config/version` dans `project.godot`, puis pousse un tag correspondant, par exemple `v0.1.2`. GitHub Actions construit les paquets Windows et Linux x86‑64 et publie une GitHub Release avec les deux lanceurs et le manifeste de mise à jour.

Télécharge le lanceur de ta plateforme depuis les releases. Sous Linux, rends le fichier exécutable avec `chmod +x`. Au premier démarrage, il télécharge le jeu. Aux suivants, il affiche l’avancement de la mise à jour, puis lance automatiquement le jeu. Si GitHub est indisponible, il affiche brièvement un message et démarre la version installée. Les versions du jeu et les sauvegardes sont stockées séparément.

Pour tester la coop Steam depuis le lanceur Windows ou Linux, ouvre Steam et connecte-toi avant de lancer le jeu.
