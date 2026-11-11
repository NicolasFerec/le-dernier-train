# Le Dernier Train — prototype Unreal

Ouvrir `LeDernierTrain.uproject` dans Unreal Engine 5.8 et lancer **Play** depuis `Content/Maps/Menu.umap`, ou lancer `./Scripts/play.sh`. Le prototype est en 3D stylisée à caméra orthographique fixe. La caméra suit le personnage et conserve la même hauteur de champ en ultra-large.

Le wagon-atelier comporte cinq postes physiques : vestiaire, arsenal, passifs, télégraphe de l’équipage et table des départs. Approcher un poste pour afficher la vraie touche d’interaction. Les trois mannequins du dernier wagon affichent les dégâts des frappes sans infliger de blessures ni compter dans les statistiques.

L’expédition actuelle enchaîne **la station de pompage et la halle des turbines**. Vider chaque salle pour ouvrir la sortie ; le passage est au bout de la carte. Le bilan affiche les salles, éliminations et dégâts par joueur. Une salle sécurisée rapporte une pièce d’atelier, et terminer l’expédition en rapporte une de plus. Le profil local conserve le solde, les déblocages et l’équipement dans un SaveGame Unreal.

Le conducteur a un sabre et peut débloquer le marteau à piston. L’Éclaireur, plus rapide et moins robuste, utilise une variante teintée du même modèle avec une rapière ou deux lames courtes. Les passifs Constitution (+20 PV) et Mobilité (+15 % de vitesse, après Constitution) se débloquent au poste dédié. Les portraits, la santé et l’esquive du groupe apparaissent dans le HUD ; les équipiers sont en plus petit au-dessus du joueur local.

- Déplacement : ZQSD (AZERTY par défaut) ou WASD (QWERTY), flèches ou stick gauche.
- Interaction : E ou A sur manette ; frappe légère : J, clic gauche ou X ; frappe lourde : K, clic droit ou Y ; esquive : Espace ou B.
- Pause : Échap ou bouton Menu. En Play-in-Editor, **Maj+Échap** arrête l’aperçu. Seul l’hôte met la simulation en pause.

## Réglages

Accessibles depuis le menu principal et la pause, enregistrés dans `Saved/Config/LinuxEditor/GameUserSettings.ini` en développement (les versions distribuées utiliseront leur dossier utilisateur Unreal).

- Audio : volumes général, musique et effets. Les ambiances du dépôt et du wagon et le son de frappe sont importés depuis le projet Godot ; les volumes pilotent des Sound Classes.
- Graphismes : plein écran, résolutions du format de l’écran, VSync, compteur d’IPS, limite d’IPS par paliers (automatique, 30 à 240, sans limite), qualité globale et réglages des ombres, textures, effets, anticrénelage, post-traitement et distance d’affichage.
- Général : français/anglais et disposition AZERTY/QWERTY.
- Commandes : clavier/manette, réattribution des actions du prototype et du changement d’onglet, rejet des doublons dans un même contexte, touches réservées, réinitialisation, activation des vibrations.

Les graphismes suivent l’approche de [Lyra](https://dev.epicgames.com/documentation/unreal-engine/scalability-and-device-profiles-in-lyra-sample-game-for-unreal-engine) : une sous-classe locale de `UGameUserSettings` et les groupes de qualité natifs d’Unreal. Lyra n’est pas une dépendance. Les menus sont limités à 30 IPS ; la limite choisie s’applique au jeu. « Automatique » utilise la fréquence native remontée par le RHI, avec repli à 60 ; cela ne détecte pas l’activation de FreeSync/G-Sync. La VSync peut limiter les IPS même avec « Sans limite ».

Sous Wayland, le compositeur gère le placement des fenêtres : le sélecteur d’écran est désactivé. Sur les plateformes permettant le placement, il déplace la fenêtre sur l’écran choisi. Pas de sélecteur de carte graphique.

Navigation : flèches, croix directionnelle ou stick ; Entrée / A pour valider. Par défaut, A/E ou LB/RB changent d’onglet ; Échap / B revient en arrière. Les indications affichent la touche réellement attribuée et suivent le dernier périphérique utilisé via Common Input d’Epic : touche clavier encadrée ou pictogramme Xbox/souris. Les images proviennent de [Kenney Input Prompts](https://kenney.nl/assets/input-prompts), sous licence CC0 ; les sources et leur licence sont conservées dans `SourceArt/InputPrompts`. La pause de l’hôte est répliquée ; Échap côté invité affiche une brève indication sans ouvrir de pause.

## Installer et mettre à jour

Les installateurs x86-64 embarquent le jeu. Sous Windows, ouvre `Le-Dernier-Train-Setup-Windows-x86_64.exe`. Le raccourci rejoint le dossier **Jeux** du menu Démarrer et le désinstallateur apparaît dans **Applications installées**. Sous Linux, ouvre un terminal dans le dossier de téléchargement et lance la commande suivante.

```sh
sh ./Le-Dernier-Train-Setup-Linux-x86_64.run
```

L’installateur Linux crée une entrée standard dans le menu des applications avec la catégorie **Jeux**. KDE la classe sous Jeux, et GNOME peut la retrouver dans sa grille ou sa recherche. Pour désinstaller le jeu, lance `${XDG_DATA_HOME:-$HOME/.local/share}/le-dernier-train-unreal/uninstall.sh`. Les deux installateurs travaillent dans les dossiers de l’utilisateur et la désinstallation conserve ses sauvegardes.

Le lanceur affiche la recherche de mise à jour, le téléchargement et l’installation, puis démarre le jeu. Si GitHub est indisponible ou si le téléchargement échoue, il affiche un message bref et ouvre la version installée. Stable suit la dernière release GitHub. Dev suit la préversion `unreal-dev`. Choisis le canal dans **Réglages → Général**. Le choix s’applique au prochain lancement. Le raccourci Linux propose aussi une action **Canal de mise à jour**, disponible sur les deux plateformes avec `LeDernierTrainLauncher --settings`.

Les deux canaux ont chacun leur installation et partagent le profil de jeu. Le profil et `launcher.log` se trouvent sous `${XDG_STATE_HOME:-$HOME/.local/state}/le-dernier-train-unreal` sur Linux et `%LOCALAPPDATA%\LeDernierTrainUnreal` sur Windows.

### Construire les paquets

Sur chaque OS, utilise Unreal Engine 5.8.4 et Python 3.11 ou plus récent. Pour l’installateur Windows, ajoute `makensis` (NSIS) au `PATH`. Windows a également besoin de Visual Studio 2022, de son SDK Windows et des DLL redistribuables x64. Linux utilise le SDK Clang d’Epic. Le build local de l’installateur Linux doit se faire sur une distribution compatible avec les machines visées. Le workflow utilise Ubuntu 24.04 pour le lanceur.

```sh
python Distribution/package_game.py --engine /chemin/vers/Unreal --channel stable --version v0.2.0
python -m pip install -r Distribution/requirements.txt
python Distribution/package_installer.py --game Saved/Distribution/Packages/Le-Dernier-Train-Unreal-stable-v0.2.0-linux-x86_64.zip
```

Sur Windows, passe le chemin d’Unreal sous Windows et utilise l’archive dont le nom se termine par `-windows-x86_64.zip`. Les archives et installateurs sortent dans `Saved/Distribution/Packages`. Le build démarre l’exécutable Shipping avec `-nullrhi` et attend que le menu charge ses réglages. Les tests de mise à jour se lancent avec `python -m unittest discover -s Distribution -p 'test_*.py'`.

### Activer GitHub Actions

Le workflow construit le projet dans `Unreal/` depuis le dépôt public GitHub de Le Dernier Train. Le lanceur généré par la CI utilise ce dépôt comme source. Le tag `unreal-dev` permet aux deux générations du jeu de cohabiter. Pour les builds locaux, l’option `--repository propriétaire/dépôt` de `package_installer.py` sélectionne la même source.

Enregistre deux runners dédiés au dépôt, Linux x86-64 et Windows x64, avec le label `unreal-5.8.4`. Installe les SDK, Python, Git, Bash et `gh`, puis configure `UNREAL_LINUX_ROOT` et `UNREAL_WINDOWS_ROOT` dans les variables Actions. Le script Windows utilise les redistribuables d’Unreal ou de Visual Studio. `UNREAL_WINDOWS_APPLOCAL_DIR` permet de fournir une arborescence Unreal alternative, par exemple `Win64/x64/Microsoft.VC143.CRT`.

Les PR qui modifient la distribution exécutent les tests du lanceur sur GitHub. Chaque mise à jour de `main` prépare les deux jeux et les deux installateurs, puis actualise la préversion `unreal-dev`. Les paquets transitent par un brouillon de release que le workflow supprime après publication. Le lanceur attend que les deux archives soient disponibles pour proposer un nouveau build. Une stable se lance manuellement sur un tag `vX.Y.Z` issu de `main`, par exemple avec `gh workflow run distribution.yml --ref v0.2.0`. GitHub génère les notes de version. Le dépôt doit autoriser la mise à jour de la préversion et de son tag `unreal-dev`.

Tant que `UNREAL_WINDOWS_ROOT` est vide, le workflow Dev construit Linux et conserve l’installateur dans le brouillon de release pour test. La publication des canaux Dev et Stable reprend une fois les deux plateformes configurées.

## Développement

Compiler : `Engine/Build/BatchFiles/Linux/Build.sh LeDernierTrainEditor Linux Development /chemin/vers/LeDernierTrain.uproject` depuis l’installation du moteur. Le MCP officiel démarre avec l’éditeur sur `http://127.0.0.1:8003/mcp`.

Le déplacement utilise Enhanced Input et Character Movement. Les attaques et l’esquive utilisent Gameplay Abilities avec prédiction locale ; les dégâts et morts restent décidés par l’hôte. Les automates poursuivent les joueurs avec Recast et AIController, annoncent leur frappe par une animation, puis appliquent le coup si le joueur est toujours à portée. Les nombres de dégâts infligés sont clairs au-dessus de la cible, les dégâts reçus sont rouges sous le personnage. Le test `LeDernierTrain.Input.ActionHints` vérifie les indications et l’activation réelle des interactions après réattribution clavier/manette, le changement de périphérique (y compris les axes), les pictogrammes et les collisions de raccourcis. Le test automatisé `LeDernierTrain.Input.KeyboardLayouts` charge le wagon et vérifie les deux dispositions, les règles de réattribution et la pause/reprise du monde. `Scripts/import_audio.py` réimporte les sources WAV de `SourceArt/Audio` et recrée les bus. Les captures de vérification se trouvent dans `Saved/PrototypeCaptures`.

Le client de viewport désactive la saisie de texte SDL dans la fenêtre du jeu tant qu’aucun champ éditable n’a le focus, pour éviter le sélecteur d’accents KDE à l’appui prolongé. Aucun réglage KDE n’est modifié.


## Coop : solo, Steam et test local

La création et la connexion sont regroupées dans `Source/LeDernierTrain/network/`. Le réseau repose sur les plugins Epic **Online Subsystem Steam**, **Steam Sockets** et **Gameplay Abilities**, ainsi que sur **Character Movement**, **GameState** et **PlayerState**. Aucun transport ni synchroniseur de mouvement personnalisé.

Dans le wagon, rejoindre le **télégraphe de l’Équipage** et utiliser la touche d’interaction affichée à proximité. Le menu indique le mode de session, les joueurs, leurs noms et leur ping. Sans Steam actif, les boutons Steam restent visibles et leur indisponibilité est expliquée. La table des départs se trouve à l’autre bout du wagon, après les mannequins.

Pour le test Steam, lancer **Steam connecté**, puis le jeu hors PIE :

```sh
./Scripts/play.sh
```

Ouvrir le salon depuis le pôle Équipage, puis utiliser « Inviter des amis Steam ». L’invitation utilise l’overlay natif Steam et son acceptation rejoint le salon. On peut aussi rechercher les salons de ce jeu depuis ce pôle. L’App ID de développement **480** se trouve dans les fichiers du projet. Les salons d’autres jeux utilisant cet App ID sont filtrés. Les versions incompatibles sont refusées avec un message lisible. Le jeu s’ouvre toujours en solo si Steam est absent ; il faut relancer le jeu après avoir ouvert Steam. Le PIE ne charge pas Steam Sockets : utiliser deux machines, deux comptes Steam et des versions identiques pour valider la vraie connexion Steam.

Test local sans Steam, dans deux processus (accepte aussi 3 ou 4) :

```sh
./Scripts/local-coop.sh 2
```

Le script utilise uniquement `127.0.0.1`, `-nosteam`, `-TrainLocal` et le net driver IP natif en fallback. Il garde les journaux dans `Saved/LocalCoop/`. Fermer le script termine ses processus. Dans les lancements de développement avec `-TrainLocal`, le pôle Équipage propose également les boutons Héberger/Rejoindre en local. Ces boutons ne sont pas disponibles dans une version Shipping.

L’hôte décide des dégâts, morts et statistiques. Les frappes légères sont sur J/clic gauche/X, les lourdes sur K/clic droit/Y. B esquive ; A interagit. Les mannequins donnent un retour de frappe sans modifier la vie, les éliminations ou les statistiques d’expédition. Les autres joueurs et leurs barres de vie sont affichés dans le HUD. Le passage n’est accessible qu’après élimination des automates. Seul l’hôte peut lancer la partie ou le changement de salle ; si le groupe n’est pas réuni, il lance dix secondes de compte à rebours. L’arrivée du dernier joueur déclenche immédiatement le passage. Les niveaux changent avec le seamless travel natif et conservent le groupe. À la victoire ou si tout le monde tombe, le bilan présente les salles, éliminations et dégâts de chaque joueur ; l’hôte ramène le groupe au wagon.

Quitter l’hôte termine la session. Les invités reviennent au menu avec une explication ; aucun transfert d’hôte n’est implémenté.

Test automatisé dans Session Frontend ou via le MCP : `LeDernierTrain.Network.FourPlayers`. Il prépare ses paramètres PIE, crée quatre mondes réseau et contrôle les déplacements, l’esquive GAS, les dégâts autoritaires, les morts répliquées, les deux comportements du compte à rebours et la pause de l’hôte. Il restaure les paramètres de test à la fin. Le projet active `net.AllowPIESeamlessTravel` pour que le test suive le même passage de niveau que le jeu hors éditeur.

### Vérification Steam avec un seul compte

Le test de développement suivant appelle les véritables fonctions du jeu et les services Steam. Steam doit être ouvert et connecté ; lancer hors PIE :

```sh
./Scripts/play.sh /Game/Maps/Wagon -ExecCmds=Train.SteamSmoke -TrainSteamSmokeExit
```

Il vérifie l’App ID 480, l’identité et le pseudo en jeu, la recherche, la création d’un salon à quatre places, l’identifiant de build natif et le net driver Steam Sockets. Il demande l’ouverture de l’interface d’invitation, **sans envoyer d’invitation**, puis ferme le salon et quitte cette instance. Le résultat est enregistré dans `Saved/SteamSmoke.json` ; `overlay_open_event` distingue un affichage confirmé d’une simple requête acceptée par l’API. Ce test ne valide pas la réception d’une invitation ni la connexion d’un autre joueur, qui demandent un deuxième compte sur une autre machine. La commande est exclue des exécutables de jeu distribués et des builds Shipping.

## Sources et validation de la scène

`SourceArt/` conserve les GLB Layer, les scènes Blender éditables, les FBX, les textures, les portraits et les licences. Les modèles et les animations d’Unreal sont dans `Content/Characters/`, les armes dans `Content/Weapons/`, et les meubles du wagon et les machines dans `Content/Props/`. Voir [la provenance des assets](SourceArt/README.md).

- `Scripts/prepare_models.py` et `Scripts/build_props.py` s’exécutent en arrière-plan dans Blender. Les meshes articulés sont normalisés en centimètres, avec un os racine au sol ; les modèles des armes sont exportés avec l’origine sur la poignée.
- Dans `UnrealEditor-Cmd`, `-run=pythonscript -script=Scripts/import_characters.py` importe les FBX et retargete les clips avec IK Rig et IK Retargeter. Les transitions et les montages sont évalués par le système d’animation d’Unreal. Les armes suivent les os de la main et leur prise est alignée sur les articulations des doigts.
- `Scripts/rebuild_prototype.py` applique les matériaux, reconstruit les cartes éditables et importe l’audio. Il conserve les anciennes cartes dans `Saved/Checkpoints/Maps`. Les sols répétés sont instanciés et les zones de navigation sont calculées par Recast.

`LeDernierTrain.Gameplay.WagonAndRun` vérifie les postes, les achats et passifs, les prises d’arme, les frappes sur mannequin, la poursuite, les deux salles, les statistiques, la santé conservée entre salles et la récompense. Le profil utilisé par ce test est restauré à la fin. `TRAIN_COOP_SMOKE=1 ./Scripts/local-coop.sh 2` lance deux processus en développement, vérifie la connexion et les actions, capture leurs HUD puis quitte. Les rapports se trouvent dans `Saved/LocalCoopSmoke/`.

Pour recapturer les deux armes, l’esquive et les dégâts en jeu avec un profil isolé :

```sh
./Scripts/play.sh /Game/Maps/Wagon -nosteam -TrainLocal -TrainProfile=visual-qa -TrainVisualSmoke -ExecCmds=Train.VisualSmoke
```

Les images sont enregistrées dans `Saved/PrototypeCaptures/`.
