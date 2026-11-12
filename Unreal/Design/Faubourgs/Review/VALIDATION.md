# Revue locale — Quai en blockout

12 novembre 2026, Unreal Engine 5.8.4, développement local. La carte est un prototype de composition ; les références ImageGen sont une intention d'habillage, pas des captures du moteur.

## Contrôles

- Compilation C++ Development : réussie (`build.txt`).
- Six tests natifs `LeDernierTrain` : réussis (`tests.txt`), dont `Network.FourPlayers` dans l'éditeur.
- Deux processus locaux : `Scripts/local-coop.sh 2`, hôte/invité, réussis (`coop.txt`). Ce test de non-régression porte sur l'expédition existante et sa réplication PCG ; il ne constitue ni un test Steam réel ni une session à quatre joueurs humains dans le nouveau blockout.
- Carte pilote : quatre départs reliés à la zone de sortie par Recast ; trois ennemis accessibles et répliqués ; aucun générateur PCG dans cette carte de composition.
- Parcours réel par `CharacterMovement` : onze points atteints, contournements nord et sud, franchissement du passage de service, arrivée à la sortie. Tous les points sont atteints au sol. Aucun téléport entre ces étapes.
- Transition native de la borne vers `Faubourgs02` : vérifiée. Le test de trajet neutralise l'IA, puis supprime les ennemis avant de tester le départ ; ce n'est pas une validation de l'équilibrage du combat.
- Captures réelles à 1600 × 900, caméra du jeu pour les vues 02 à 06, caméra éloignée uniquement pour la vue d'ensemble. HUD masqué pour lire la composition. Pas de benchmark FPS nouveau dans cette passe.
- Reconstruction de la carte vérifiée sur plusieurs exécutions ; copie de la précédente dans `Saved/Checkpoints/Blockout/`.

Les captures isolent la composition avec les ennemis immobiles. Le lancement normal rétablit leur IA et les combats habituels. Les contrôles de cette passe ne valident pas les rigs et animations, qui sont encore ceux de la version précédente.

## À discuter en jouant

1. L'arrivée donne-t-elle assez de contexte et laisse-t-elle comprendre où aller ?
2. La cour permet-elle de contourner naturellement les ennemis, sans paraître trop vide ?
3. Le passage et le poste changent-ils suffisamment le rythme et le cadrage ?
4. Les distances fonctionnent-elles à la caméra du jeu, notamment avec quatre joueurs ?

Après accord sur ces points, remplacer les volumes par ordre de silhouette et d'utilité. Aucun crédit Meshy de décor consommé ici.
