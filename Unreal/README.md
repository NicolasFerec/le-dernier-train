# Le Dernier Train — prototype Unreal

Ouvrir `LeDernierTrain.uproject` dans Unreal Engine 5.8 et lancer **Play** depuis `Content/Maps/Menu.umap`, ou lancer `./Scripts/play.sh`. Le prototype est en 3D stylisée à caméra orthographique fixe. La caméra suit le personnage et conserve la même hauteur de champ en ultra-large.

Le wagon-atelier comporte cinq postes physiques : vestiaire, arsenal, passifs, télégraphe de l’équipage et table des départs. Approcher un poste pour afficher la vraie touche d’interaction. Les trois mannequins du dernier wagon affichent les dégâts des frappes sans infliger de blessures ni compter dans les statistiques.

L’expédition actuelle traverse **les dix salles des Faubourgs noyés**, du quai d’évacuation à la station de pompage. L’Éclusier garde la cinquième salle et le Gardien de la station la dixième. Vider chaque salle pour ouvrir la sortie ; le passage est au bout de la carte. Le bilan affiche les salles, éliminations et dégâts par joueur. Une salle sécurisée rapporte une pièce d’atelier, et terminer l’expédition en rapporte une de plus. Le profil local conserve le solde, les déblocages et l’équipement dans un SaveGame Unreal.

Le déplacement et les changements de direction sont rapides. Une frappe avance légèrement le personnage ; une commande saisie juste avant la fin de la frappe précédente est gardée pendant 0,20 seconde. Le sabre alterne deux coupes. L’esquive est une roulade au sol, possible après le contact du coup. Les automates ordinaires reculent et interrompent leur frappe lorsqu’ils sont touchés ; les boss gardent leurs attaques annoncées. Les coups ne traversent pas les obstacles.

Le conducteur a un sabre et peut débloquer le marteau à piston. L’Éclaireuse, plus rapide et moins robuste, possède son propre modèle Meshy (veste bleu pétrole, foulard ocre), un rig `ue5_mannequin` avec doigts, un matériau PBR et ses animations retargetées par Unreal. Elle utilise une rapière ou deux lames courtes. La provenance et les paramètres sont dans `SourceArt/Characters/Scout/PROVENANCE.md`. Les passifs Constitution (+20 PV) et Mobilité (+15 % de vitesse, après Constitution) se débloquent au poste dédié. Les portraits, la santé et l’esquive du groupe apparaissent dans le HUD ; les équipiers sont en plus petit au-dessus du joueur local.

Le Conducteur utilise maintenant le rig Meshy `ue5_mannequin` de `Content/Characters/ConductorUE5`, avec ses animations directement générées sur ce squelette : repos, locomotion, attaques, roulade et réactions. L’import corrige les axes du pack d’actions et adapte le bras droit au port de l’arme pendant la course. L’option doigts a été demandée, mais les mains sculptées en poings n’ont produit qu’une articulation partielle ; ce n’est pas le squelette Manny complet. Voir [la provenance et les limites du rig](SourceArt/Characters/Conductor/UE5Rework/PROVENANCE.md).

Le repos de l’Éclaireuse est reconstruit sur son squelette avec deux appuis fixes, une légère flexion des genoux et une respiration discrète. `Scripts/build_scout_idle.py` produit cette boucle et `Scripts/validate_scout_idle.py` vérifie les deux pieds, les genoux et la continuité de l’animation. La commande de développement `Train.ScoutReview` avec `-TrainScoutReview` permet de comparer les poses et les retours au repos dans le wagon.

- Déplacement : ZQSD (AZERTY par défaut) ou WASD (QWERTY), flèches ou stick gauche.
- Interaction : E ou A sur manette ; frappe légère : J, clic gauche ou X ; frappe lourde : K, clic droit ou Y ; esquive : Espace ou B.
- Pause : Échap ou bouton Menu. En Play-in-Editor, **Maj+Échap** arrête l’aperçu. Seul l’hôte met la simulation en pause.

## Salle pilote en blockout

Lancer `./Scripts/play-quai-blockout.sh` pour tester la nouvelle composition du Quai des évacués : débarquement, cour à deux contournements, passage de service et poste de signalisation. Cette carte indépendante utilise des volumes simples, trois patrouilleurs et les déplacements/combat du jeu. Après élimination des adversaires, la borne verte mène à la salle 2 existante. Le lancement utilise un profil de réglages séparé, avec la résolution native par défaut.

Le [parcours proposé des dix salles](Design/Faubourgs/PARCOURS.md), les deux références ImageGen et le plan coté servent à valider la composition avant les modèles de décor. `Scripts/build_quai_blockout.py` reconstruit uniquement `/Game/Maps/Blockout/QuaiBlockout`. Les dix cartes de l'expédition conservent leur génération actuelle.

## Éclairage

Les lanternes du wagon utilisent chacune une lumière ponctuelle mobile placée dans le luminaire, avec atténuation physique et ombres dynamiques. Le personnage, les armes, les meubles et les cloisons occultent ces sources. L’éclairage ambiant reste faible ; le bloom est réduit pour conserver les détails près des lampes. Les Faubourgs et la Fonderie utilisent les mêmes règles, avec une lumière extérieure froide et des sources locales chaudes.

`Scripts/lighting.py` centralise ces réglages. `Scripts/rebuild_lighting.py`, exécuté avec le commandlet Python d’Unreal, met à jour les douze cartes sans reconstruire leur géométrie ni leur navigation ; les cartes précédentes sont conservées dans `Saved/Checkpoints/Lighting`. Les scripts de construction réappliquent aussi ces réglages. Les intensités sont en lumens et les couleurs utilisent des arguments nommés : l’ordre positionnel du type Python `Color` est B, G, R, A.

Pour comparer les contributions réelles dans la vue de jeu : `./Scripts/play.sh /Game/Maps/Wagon -TrainLocal -TrainLightingPreview -ExecCmds=Train.LightingPreview`. Cet outil de développement capture une pose fixe avec l’éclairage complet, les lumières locales coupées, puis leurs ombres coupées, et ferme le jeu. Les images sont dans `Saved/LightingReview`. Les options `-TrainLightX=1550 -TrainLightY=30 -TrainLightLabel=entrainement` permettent de changer le point de contrôle. Les réglages graphiques d’ombres du joueur restent applicables.

## Salles et rencontres procédurales

Chaque nouvelle expédition reçoit une graine aléatoire. Le plugin **PCG d’Epic** place les modules de mobilier avec ses nœuds natifs Create Points et Spawn Actor ; les règles du jeu réservent un trajet entre l’entrée et la sortie, les abords de l’arrivée et les arènes des boss. Les quais, les ponts et les repères narratifs sont conservés. Caisses, étals, vannes et équipements sont choisis selon la salle, puis leurs positions et orientations changent à chaque partie.

La génération tourne sur l’hôte. Les modules sont des acteurs répliqués par Unreal, avec leurs collisions ; Recast recalcule les zones bloquées grâce aux modificateurs dynamiques. Les ennemis sont placés ensuite sur des positions accessibles, sans chevauchement, à plus de treize mètres de l’arrivée. Le groupe attend brièvement la fin de cette préparation avant de pouvoir se déplacer. L’hôte décide des types et positions ; les invités reçoivent les mêmes acteurs.

Le budget de menace augmente à chaque salle : **8, 11, 14, 17, 21, 25, 29, 33, 37, 43**. La composition varie à budget égal. Les coûts internes sont 1 pour un coureur, 2 pour une patrouille, 3 pour un tireur, 4 pour une sentinelle, 10 pour l’Éclusier et 16 pour le Gardien. Les sentinelles arrivent à partir de la salle 3, les tireurs à partir de la salle 4. En coop, chaque équipier supplémentaire ajoute `3 + salle / 3` points, avec division entière. L’effectif est fixé au départ du wagon pour toute l’expédition. Les points de vie et dégâts des types restent fixes, en solo comme en coop. Ce budget est une règle de composition ; l’équilibrage ressenti doit continuer à être testé en jouant.

Pour reproduire un tirage en développement : `./Scripts/play.sh /Game/Maps/Pompage -TrainLocal -TrainSeed=101`. Relancer avec une autre graine change le placement et les rencontres. Sans cet argument, chaque départ du wagon renouvelle la graine.

Les tests `LeDernierTrain.Generation.EncounterBudgets` et `LeDernierTrain.Generation.RoomSeeds` vérifient les budgets, les introductions de rôles, les boss, la reproductibilité, la variation entre graines, les statistiques constantes et la navigation après génération. `LeDernierTrain.Network.FourPlayers` compare aussi les modules et leurs collisions entre hôte et invité.

## Réglages

Accessibles depuis le menu principal et la pause, enregistrés dans `Saved/Config/LinuxEditor/GameUserSettings.ini` en développement (les versions distribuées utiliseront leur dossier utilisateur Unreal).

- Audio : volumes général, musique et effets. Les ambiances du dépôt et du wagon et le son de frappe sont importés depuis `SourceArt/Audio` ; les volumes pilotent des Sound Classes.
- Graphismes : résolution native automatique de l’écran choisi et plein écran sans bordure par défaut ; choix manuel conservé, résolutions du format de l’écran, VSync, compteur d’IPS, limite d’IPS par paliers (automatique, 30 à 240, sans limite), qualité globale et réglages des ombres, textures, effets, anticrénelage, post-traitement et distance d’affichage.
- Général : français/anglais et disposition AZERTY/QWERTY.
- Commandes : clavier/manette, réattribution des actions du prototype et du changement d’onglet, rejet des doublons dans un même contexte, touches réservées, réinitialisation, activation des vibrations.

La résolution du rendu 3D est indépendante de celle de l’écran : « Automatique (Unreal) » conserve la reconstruction temporelle du moteur, 100 % rend chaque pixel de la scène à la résolution choisie, et 83/75/67 % privilégient la fluidité. Le HUD reste à la résolution de sortie.

Les graphismes suivent l’approche de [Lyra](https://dev.epicgames.com/documentation/unreal-engine/scalability-and-device-profiles-in-lyra-sample-game-for-unreal-engine) : une sous-classe locale de `UGameUserSettings` et les groupes de qualité natifs d’Unreal. Lyra n’est pas une dépendance. Les menus sont limités à 30 IPS ; la limite choisie s’applique au jeu. « Automatique » utilise la fréquence native remontée par le RHI, avec repli à 60 ; cela ne détecte pas l’activation de FreeSync/G-Sync. La VSync peut limiter les IPS même avec « Sans limite ».

Les lancements de test avec `-TrainProfile=nom` isolent aussi `GameUserSettings.ini` dans `Saved/TestSettings/nom`, pour ne pas écraser la résolution et les commandes du joueur.

Sous Wayland, le compositeur gère le placement des fenêtres : le sélecteur d’écran est désactivé. Sur les plateformes permettant le placement, il déplace la fenêtre sur l’écran choisi. Pas de sélecteur de carte graphique.

Navigation : flèches, croix directionnelle ou stick ; Entrée / A pour valider. Par défaut, A/E ou LB/RB changent d’onglet ; Échap / B revient en arrière. Les indications affichent la touche réellement attribuée et suivent le dernier périphérique utilisé via Common Input d’Epic : touche clavier encadrée ou pictogramme Xbox/souris. Les images proviennent de [Kenney Input Prompts](https://kenney.nl/assets/input-prompts), sous licence CC0 ; les sources et leur licence sont conservées dans `SourceArt/InputPrompts`. La pause de l’hôte est répliquée ; Échap côté invité affiche une brève indication sans ouvrir de pause.

## Installer et mettre à jour

Les installateurs x86-64 embarquent le jeu. Sous Windows, ouvre `Le-Dernier-Train-Setup-Windows-x86_64.exe`. Le raccourci rejoint le dossier **Jeux** du menu Démarrer et le désinstallateur apparaît dans **Applications installées**. Sous Linux, ouvre un terminal dans le dossier de téléchargement et lance la commande suivante.

```sh
sh ./Le-Dernier-Train-Setup-Linux-x86_64.run
```

L’installateur Linux crée une entrée standard dans le menu des applications avec la catégorie **Jeux**. KDE la classe sous Jeux, et GNOME peut la retrouver dans sa grille ou sa recherche. Pour désinstaller le jeu, lance `${XDG_DATA_HOME:-$HOME/.local/share}/le-dernier-train-unreal/uninstall.sh`. Les deux installateurs travaillent dans les dossiers de l’utilisateur et la désinstallation conserve ses sauvegardes.

Le lanceur suit la dernière release stable de GitHub. Il affiche la recherche de mise à jour, le téléchargement et l’installation, puis démarre le jeu. Si GitHub est indisponible ou si le téléchargement échoue, il affiche un message bref et ouvre la version installée. Pour tester en local, utilise `./Scripts/play.sh` ou `./Scripts/local-coop.sh 2` depuis le projet Unreal.

Sous Linux, le lanceur peut aussi se mettre à jour. Il vérifie et teste le nouvel exécutable avant de le remplacer, puis redémarre depuis le même raccourci. Installe une fois une version qui prend en charge cette fonction pour activer les mises à jour suivantes du lanceur.

Le profil et `launcher.log` se trouvent sous `${XDG_STATE_HOME:-$HOME/.local/state}/le-dernier-train-unreal` sur Linux et `%LOCALAPPDATA%\LeDernierTrainUnreal` sur Windows.

### Construire les paquets

Sur chaque OS, utilise Unreal Engine 5.8.4 et Python 3.11 ou plus récent. Pour l’installateur Windows, ajoute `makensis` (NSIS) au `PATH`. Windows a également besoin de Visual Studio 2022, de son SDK Windows et des DLL redistribuables x64. Linux utilise le SDK Clang d’Epic. Le build local de l’installateur Linux doit se faire sur une distribution compatible avec les machines visées.

```sh
python Distribution/package_game.py --engine /chemin/vers/Unreal --version v0.2.0
python -m pip install -r Distribution/requirements.txt
python Distribution/package_installer.py --game Saved/Distribution/Packages/Le-Dernier-Train-Unreal-stable-v0.2.0-linux-x86_64.zip
```

Sur Windows, passe le chemin d’Unreal sous Windows et utilise l’archive dont le nom se termine par `-windows-x86_64.zip`. Les archives et installateurs sortent dans `Saved/Distribution/Packages`. Le build démarre l’exécutable Shipping avec `-nullrhi` et attend que le menu charge ses réglages. Les tests de mise à jour se lancent avec `python -m unittest discover -s Distribution -p 'test_*.py'`.

### Publier les paquets

Construis localement depuis le commit de `main` à distribuer. Prépare une release stable avec le tag `vX.Y.Z`, les notes GitHub générées depuis la stable précédente, le ZIP et l’installateur de chaque plateforme construite. Vérifie les fichiers joints avant la publication.

Sous Linux, joins aussi l’exécutable autonome `Le-Dernier-Train-Launcher-vX.Y.Z-linux-x86_64` généré avec l’installateur. Le script `Distribution/publish.py` attend ces trois fichiers pour Linux. Incrémente `Distribution/launcher/version.txt` à chaque changement du lanceur, indépendamment de la version du jeu.

Utilise `--repository propriétaire/dépôt` lors de la construction de l’installateur pour sélectionner le dépôt GitHub qui héberge ces releases.

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

Si le personnage de l’hôte tombe, l’hôte peut autoriser le passage dès qu’un équipier vivant atteint la sortie d’une salle vidée. L’indication de commande apparaît alors dans son HUD. Le rassemblement concerne les joueurs encore debout.

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
- `Scripts/rebuild_faubourgs.py` reconstruit les dix cartes et leur navigation depuis les modèles GLB de `SourceArt/Props/Faubourgs/`. `Scripts/import_faubourgs_enemies.py` importe les squelettes Meshy et retargete les animations d’automate. Lancer ces scripts avec `UnrealEditor-Cmd -run=pythonscript -script=...`.
- `Scripts/import_characters.py` prépare les personnages de base avec IK Rig et IK Retargeter. Pour les animations actuelles, exécuter ensuite `Scripts/import_combat_motion.py` : course, marche, deux coupes, attaques lourdes, roulade et réactions Meshy. Le script recale les contacts sur les délais du gameplay et fixe les appuis sans déplacer la capsule ; Character Movement et GAS pilotent le déplacement. Avant de réimporter les gardiens, lancer `Scripts/prepare_faubourgs_rigs.py` avec Blender puis importer avec `-TrainReimport` pour normaliser l’échelle et actualiser la pose de référence. Chaque poignée est alignée sur la main à partir de son propre point de prise.
- Pour le **Conducteur actuel**, utiliser `Scripts/prepare_conductor_ue5.py` dans Blender puis `Scripts/import_conductor_ue5.py` dans Unreal. Ce chemin importe directement les clips sur le nouveau squelette ; le script historique ci-dessus ne met à jour que l’ancien Conducteur et les ennemis. Vérifier avec `Scripts/validate_conductor_ue5.py` et `Scripts/validate_combat_assets.py`. La commande de développement `Train.ConductorReview`, avec le drapeau `-TrainConductorReview`, enregistre une séquence de contrôle visuel dans `Saved/ConductorUE5Review`.
- `Scripts/art_direction.py` partage les matériaux et l’éclairage entre le wagon et les Faubourgs. `Scripts/rebuild_world.py`, puis `Scripts/rebuild_faubourgs.py`, reconstruisent ces décors. L’établi Meshy est importé automatiquement depuis son GLB. Pour recalculer les matériaux existants, ajouter `-TrainRefreshMaterials`. `Scripts/build_combat_effects.py` crée les meshes de traînées et d’impacts dans Blender ; `Scripts/import_combat_effects.py` les importe avec les matériaux d’effets et les annonces de frappe.
- `Scripts/rebuild_prototype.py` applique les matériaux, reconstruit les cartes éditables et importe l’audio. Il conserve les anciennes cartes dans `Saved/Checkpoints/Maps`. Les sols répétés sont instanciés et les zones de navigation sont calculées par Recast.
- `GameplayCharacterLOD` fournit au conducteur et à l’automate un LOD à environ 15 000 triangles pour la caméra de jeu. La texture `/Game/Art/Steel` est redimensionnée à 1024 × 1024 lors de la préparation des données de rendu, avec des mipmaps et la compression standard pour les textures de décor.

`LeDernierTrain.Gameplay.WagonAndRun` vérifie les postes, les achats et passifs, les prises d’arme, les frappes sur mannequin, la poursuite, le parcours des dix salles et les deux boss, les statistiques, la santé conservée entre salles et la récompense. Le profil utilisé par ce test est restauré à la fin. `TRAIN_COOP_SMOKE=1 ./Scripts/local-coop.sh 2` lance deux processus en développement, vérifie la connexion et les actions, puis le passage dans une salle procédurale avec comparaison de la graine, des modules et des ennemis entre hôte et invité. Il capture leurs HUD puis quitte. Les rapports se trouvent dans `Saved/LocalCoopSmoke/`.

Pour capturer plusieurs poses de course, de frappe légère et lourde avec chaque arme, ainsi que l’esquive et les dégâts en jeu, avec un profil isolé :

```sh
./Scripts/play.sh /Game/Maps/Wagon -nosteam -TrainLocal -TrainProfile=visual-qa -TrainVisualSmoke -ExecCmds=Train.VisualSmoke
```

Les images sont enregistrées dans `Saved/PrototypeCaptures/`.

`Scripts/validate_combat_assets.py`, lancé dans le commandlet Python, vérifie aussi les assets importés : pieds au sol dans les poses de référence et les frappes, contact dirigé vers la cible, normal map et texture metallic/roughness reliées au matériau de l’établi. Ce contrôle complète les tests de jeu ; il ne remplace pas la revue visuelle des déformations et des transitions.

## Mesures locales de performance

`./Scripts/play.sh /Game/Maps/Wagon -TrainLocal -TrainProfile=benchmark -TrainBenchmark -TrainBenchmarkScout -TrainBenchmarkLabel=review -ExecCmds=Train.Benchmark` exécute trois vues réelles : wagon, salle 6, salle 10. Chaque vue chauffe six secondes puis mesure douze secondes, sans limite d’IPS ni VSync. Les combats continuent ; seul le joueur de test est invulnérable. Les salles utilisent un budget de rencontre pour quatre joueurs dans un seul processus ; ce n’est pas une mesure de quatre clients rendus simultanément.

Le CSV est dans `Saved/Benchmarks`. `-TrainNativeRendering` impose un rendu 3D à 100 % pour cette mesure, `-TrainProfileGPU` ajoute le profilage GPU natif, et `-windowed -ResX=1600 -ResY=900` permet de reproduire la résolution de référence. Sans ces dimensions, le lancement conserve la résolution native automatique. Les profils de test isolent les réglages du joueur.
