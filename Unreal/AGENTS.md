# Le Dernier Train — Unreal

Unreal est le chantier actif. Les règles et la direction artistique sont dans [`../VISION.md`](../VISION.md). Garder le français par défaut et fournir l’anglais pour tout texte visible.

## Réseau

Utiliser les systèmes fournis par Epic : Online Subsystem Steam pour les sessions et invitations, Steam Sockets pour le transport, Character Movement pour la prédiction/correction/lissage, Gameplay Abilities pour les actions prédites. Ne pas créer de transport, de protocole de mouvement, de synchroniseur ou d’interpolateur maison. Le code `network/` raccorde les règles du jeu à ces API.

Les ennemis, les dégâts, les morts, les statistiques et les changements de salle font autorité sur l’hôte. Quatre places au maximum. Aucun transfert d’hôte. Seul l’hôte met la simulation en pause et décide du départ ; si les autres sont loin, compter dix secondes, puis partir immédiatement s’ils arrivent avant l’échéance. Steam est le seul mode en ligne exposé aux joueurs. Le fallback IP est limité aux outils de développement locaux.

L’App ID de test 480 figure dans `Config/DefaultEngine.ini` et `steam_appid.txt`, pas dans une variable d’environnement. Ne pas considérer la présence de la configuration Steam comme un test de connexion Steam réussi : deux comptes Steam sur deux machines restent nécessaires pour le test réel.

## Validation et publication

Compiler et tester en local. Le test natif `LeDernierTrain.Network.FourPlayers` crée un hôte et trois clients dans l’éditeur ; vérifier aussi `Scripts/local-coop.sh 2` dans des processus distincts. Distinguer les vérifications locales, les tests simulés et le test Steam réel dans le compte rendu.

Garder les itérations locales sauf demande explicite de push/PR. Construire les paquets en local et joindre les fichiers aux releases GitHub. Publier une release stable uniquement sur demande explicite après review, depuis un tag de `main`. Générer les notes GitHub depuis la release stable précédente.
