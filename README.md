# Le Dernier Train

Prototype d’action 2D en GDScript pour Godot 4.7.2 Standard.
Voir la [vision du jeu](VISION.md) pour la boucle cible, la progression et les priorités du prototype.

```sh
godot --editor --path .   # Ouvrir le projet dans l’éditeur
godot --path .            # Ouvrir le menu principal
```

Clique sur **Jouer** pour entrer dans le salon. Va au poste **Départ**, appuie sur **E**, puis clique sur **Lancer la partie** pour jouer seul. **Échap** ou le bouton **Menu** de la manette ouvre le menu de pause. En coop, seul l’hôte peut mettre la partie en pause pour le groupe. Pour tester la coop sur cette machine, démarre deux instances de développement.

```sh
godot --path . -- --local-multiplayer
godot --path . -- --local-multiplayer --profile=invite
```

1. Clique sur **Jouer** dans les deux fenêtres et ouvre le menu avec **Échap**. Clique sur **Héberger en local** dans la première et **Rejoindre en local** dans la seconde. La liste du menu indique les deux joueurs. Seul l’hôte lance la partie depuis le poste **Départ**. Si des joueurs sont hors du cercle, le compte à rebours s’affiche pour tous. Le train part dès que le groupe est réuni ou après 10 secondes.
2. Au clavier, déplace-toi avec **ZQSD**, **WASD** (touches physiques) ou les flèches. Une touche seule suit l’écran. **D** va horizontalement vers la droite. Avec deux touches, **S + D** ou **Bas + Droite** suit la longueur du wagon. Les autres diagonales clavier et croix directionnelle suivent aussi les axes du sol. **J** attaque et **Espace** esquive. Sur une manette Xbox, déplace-toi au **stick gauche** ou à la **croix**, attaque avec **X** et esquive avec **B**.
3. Élimine tous les ennemis pour ouvrir la sortie au bout de la salle. L’hôte s’approche de la porte et appuie sur **E** ou **A**. Si tous les joueurs debout sont dans le cercle, le groupe passe immédiatement. Sinon, la même commande lance un compte à rebours de 10 secondes. Le groupe passe dès que le dernier joueur debout entre dans le cercle ou à la fin du compte à rebours. La sortie de la deuxième salle ramène le groupe au train et accorde trois points par joueur. Si tout le groupe tombe, l’hôte peut retourner au salon.
4. Au salon, approche-toi du poste **Personnage**, **Arsenal** ou **Passifs** et appuie sur **E** ou **A** pour acheter ou t’équiper. Poursuis le long du wagon jusqu’au **Stand d’entraînement** pour essayer les attaques et l’esquive sans infliger de dégâts. Les achats et les choix sont sauvegardés dans le profil local Godot (`user://`).
5. Pour terminer la session coop, clique sur **Quitter la session** chez l’hôte. Le client revient à son salon solo.

Le mode de développement ENet écoute uniquement sur la boucle locale au port UDP **7000**, avec trois places client. Les connexions s’effectuent au salon, entre les parties. L’hôte simule les combats et décide des transitions de salle.

La coop utilise [netfox 1.35.3](https://github.com/foxssake/netfox/releases/tag/v1.35.3) pour prédire le déplacement et l’esquive et pour interpoler les positions. Le projet inclut l’addon et ses licences MIT dans `addons/`. Les deux correctifs locaux concernent la fermeture avant la synchronisation initiale et l’annulation d’une synchronisation lors d’une déconnexion.

Le salon et les missions partagent la caméra isométrique et les [personnages articulés en 2D](player/character_visual.gd). Les décors des missions sont dessinés dans [la scène de salle](arena/mission/mission_room.tscn).

## Réglages

Depuis le menu principal, **Réglages** permet de modifier les trois volumes, le plein écran, la résolution de rendu en plein écran, la VSync, le compteur d’IPS et la limite d’IPS en jeu. **Retour** enregistre ces réglages localement. En plein écran, la liste propose la résolution native et des résolutions courantes au même format. Le jeu adapte le rendu à l’écran sans changer le mode d’affichage du bureau. En mode fenêtré, redimensionne la fenêtre directement. Les menus sont plafonnés à 30 IPS. Le mode **Auto** utilise la fréquence de l’écran ou 60 IPS si elle est inconnue.

Les boucles du dépôt et du wagon combinent une piste musicale et des bruitages d’ambiance. Le curseur **Musique** règle la première, tandis que **Effets** règle les ambiances et les sons des actions. Les pistes et effets originaux sont dans `audio/`.

## Coop Steam

Installe [GodotSteam 4.24 GDExtension](https://codeberg.org/godotsteam/godotsteam/releases/tag/v4.24-gde) en extrayant le dossier `addons/godotsteam` de l’archive à la racine du projet. Lance l’éditeur une fois pour importer l’extension. Les binaires de l’extension sont installés localement et ignorés par Git.

Pour tester, ouvre le client Steam et connecte-toi sur deux machines avec deux comptes amis. Lance la même version du jeu sur chacune :

```sh
godot --path .
```

L’App ID **480** correspond à Spacewar, l’application de test publique de Steam. Il est configuré dans la section `[steam]` de `project.godot`. Remplace-le par l’App ID Steamworks du jeu quand il sera disponible.

1. Dans le salon, l’hôte se rend au poste **Communications**, l’ouvre avec **E** ou **A**, puis choisit **Inviter des amis Steam**.
2. L’invité accepte l’invitation Steam. Le bouton d’invitation copie aussi le code du salon, que l’invité peut saisir au poste **Communications**.
3. Les deux joueurs apparaissent dans le menu de session. L’hôte lance la partie depuis le poste **Départ**.

Le salon Steam est réservé aux amis et accepte les connexions entre les parties.

## Langues

Le jeu démarre en français. Le sélecteur dans **Réglages → Général** permet de passer à l’anglais et mémorise le choix. Les dialogues, l’interface et les messages du jeu utilisent le catalogue `locale/game.csv`. Renseigne les colonnes `fr` et `en` à chaque ajout ou modification de texte.

## Publication et mises à jour

Teste d’abord les changements en local, puis ouvre une PR pour review. Une fois les changements intégrés et la release demandée, pousse le tag correspondant à `application/config/version` dans `project.godot`. Lance ensuite le workflow manuellement sur ce tag, par exemple avec `gh workflow run release.yml --ref v0.1.6`. Le workflow construit les paquets Windows et Linux x86‑64, puis publie les deux lanceurs, le manifeste de mise à jour et le changelog généré par GitHub.

Télécharge le fichier `Le-Dernier-Train-Lanceur-…` de ta plateforme depuis les releases. Les ZIP `Arene-coop-…` restent disponibles pour la mise à jour automatique des anciens lanceurs. Sous Linux, rends le fichier exécutable avec `chmod +x`. Au premier démarrage, il télécharge le jeu. Aux suivants, il affiche l’avancement de la mise à jour, puis lance automatiquement le jeu. Si GitHub est indisponible, il affiche brièvement un message et démarre la version installée. Les versions du jeu et les sauvegardes sont stockées séparément.

Dans **Réglages → Général → Canal de mise à jour**, **Stable** est sélectionné par défaut et suit la dernière release officielle GitHub. **Dev** suit la préversion roulante `dev`, reconstruite pour Windows et Linux à chaque mise à jour de `main`. Le nouveau lanceur propose aussi ce choix au premier démarrage. Pour rouvrir son sélecteur depuis un terminal, passe-lui `-- --launcher-settings`. Le choix fait dans le jeu s’applique au prochain lancement. Les installations des deux canaux sont séparées. Si Dev est indisponible, le lanceur utilise la version Dev déjà installée ou se rabat sur Stable si Dev n’a jamais été installé. Il faut utiliser un lanceur qui prend en charge les canaux.

Sous Linux, au premier démarrage réussi avec ce lanceur, **Le Dernier Train** apparaît dans la catégorie **Jeux** du menu des applications de KDE. Cette entrée ouvre la copie installée du lanceur et conserve la vérification des mises à jour. Tu peux ensuite supprimer le fichier téléchargé.

Pour tester la coop Steam depuis le lanceur Windows ou Linux, ouvre Steam et connecte-toi avant de lancer le jeu.
