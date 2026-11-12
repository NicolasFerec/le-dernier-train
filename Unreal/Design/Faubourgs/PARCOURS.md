# Faubourgs noyés — parcours et salle pilote

Version de travail du 12 novembre 2026. Périmètre choisi avec Nicolas : parcours global des dix salles, puis **une salle complète à éprouver**, avant l'habillage. Les formes ci-dessous sont une proposition de level design, pas dix nouvelles cartes déjà réalisées.

## Le fil du voyage

Le train s'arrête devant les voies noyées. L'équipe descend au quai, remonte le canal jusqu'à l'écluse, puis suit les conduites vers la station de pompage. L'armée avait ordonné aux automates d'inonder les quartiers et d'empêcher la remise en service du réseau : ils gardent donc précisément les passages et les commandes dont les survivants ont besoin. À l'écluse, l'Éclusier bloque l'accès à la rive industrielle ; à la station, le Gardien protège les pompes. Réactiver celles-ci libère les rails vers la Fonderie.

L'architecture raconte cette mission sans ajouter dix objets à collecter : suivre l'eau, puis les canalisations, puis l'alimentation des pompes. Les traces de l'évacuation se raréfient ; les installations industrielles prennent la place des habitations. On voit la station avant d'y arriver.

## Dix lieux, dix compositions

Les budgets indiqués sont ceux du système actuel en solo, conservés comme garde-fou pour la future intégration. Ils ne sont pas des nombres d'ennemis : patrouilleur 2, coureur 1, sentinelle 4, tireur 3, Éclusier 10, Gardien 16. Les statistiques d'un type restent fixes. La difficulté vient du budget croissant et de la combinaison des rôles, avec des temps de respiration internes aux salles.

| Salle | Forme et progression spatiale proposées | Ce que le lieu raconte | Combat et budget solo |
|---|---|---|---|
| 1. Quai des évacués | Débarquement latéral protégé → cour ouverte autour d'un îlot → passage de service → poste de signalisation. Deux contournements larges. | Bagages abandonnés ; les machines gardent le départ des évacués. La rue des vannes est la seule sortie sèche. | Apprendre à contourner et lire un coup. Budget cible 8 ; pilote volontairement limité à 3 patrouilleurs pour juger l'espace. |
| 2. Rue des vannes | Rue en coude, petite placette puis bifurcation autour d'un immeuble condamné ; branches qui se rejoignent. | Vannes fermées, repères du niveau d'eau sur les façades ; accès public coupé. | Coureurs qui débordent les patrouilleurs. Budget 11. |
| 3. Entrepôt des messageries | Halle traversante en diagonale ; allée centrale large, deux travées latérales, sortie par le quai de chargement opposé. | Fret resté sur place lors de l'évacuation ; rails logistiques dirigés vers l'écluse. | Première sentinelle ; contourner son front sans se coincer entre les caisses. Budget 14. |
| 4. Passerelles du canal | Deux rives longitudinales reliées par deux traversées larges, avec un bassin au centre ; boucle en huit lisible, aucun cul-de-sac étroit. | Le canal conduit à l'écluse visible au fond. | Premiers tireurs, changement de rive pour casser leur ligne de tir. Budget 17. |
| 5. Écluse du faubourg | Grand tablier en U autour de la chambre d'écluse ; large aire centrale sèche, deux refuges latéraux. | L'Éclusier maintient la vanne de transit condamnée. Sa défaite ouvre la route de la rive industrielle. | Premier boss : cônes frontaux, contournement, renforts contenus. Budget 21, dont 10 pour le boss. |
| 6. Marché submergé | Place en éventail ; arcades périphériques, un cœur dégagé, étals regroupés en îlots asymétriques. | Dernière poche habitée ; l'eau entre par les rues basses. Les conduites émergent derrière le marché. | Pression de plusieurs directions, espace de repli sur le pourtour. Budget 25. |
| 7. Collecteurs | Deux grandes chambres reliées par une galerie large et une dérivation ; vue sur une troisième chambre noyée inaccessible. | L'eau converge sous la ville ; suivre la canalisation principale mène à la chaufferie. | Séparer poursuite et tir, puis réunir les menaces dans la deuxième chambre. Budget 29. |
| 8. Salle des chaudières | Deux nefs décalées, trois chaudières en bordure et un passage transversal ; une grande diagonale libre. | Source d'énergie de la station ; dispositifs encore entretenus par les automates. | Sentinelles qui tiennent les intersections, coureurs sur les côtés. Budget 33. |
| 9. Poste de répartition | Vestibule comprimé → salle de commande en éventail → large seuil donnant vue sur les pompes. | Câbles et conduites alimentent la même station ; dernières commandes condamnées. | Combinaison de rôles déjà connus ; éviter de concentrer tous les tireurs sur l'entrée. Budget 37. |
| 10. Station de pompage | Grande salle longitudinale ; bassin et pompes en fond, piste centrale large, deux boucles latérales ; console finale au-delà du boss. | Le Gardien applique l'ordre d'arrêt. Après sa défaite, les pompes évacuent l'eau et le train peut repartir. | Boss final avec place pour attaques circulaires et renforts latéraux. Budget 43, dont 16 pour le boss. |

Les ouvertures de vannes et la remise en route finale décrivent la mise en scène souhaitée. Elles ne constituent pas de nouvelles interactions déjà programmées dans cette passe.

## Ce qui doit rester fixe, ce qui pourra varier

Fixe : silhouette de chaque lieu, repères visuels, arrivée protégée, sortie, sens de circulation de l'eau, grandes zones de combat et voies de contournement. Variable ensuite : positions dans des poches de rencontre, quelques amas de fret aux endroits autorisés, substitutions d'ennemis à coût égal. Aucun déplacement aléatoire des murs principaux, du pont ou des bâtiments. Le pilote reste déterministe afin de comparer les versions de composition.

## Quai des évacués : critères de validation

- Une arrivée où quatre personnages peuvent se regrouper sans agression immédiate.
- Une cour qui se comprend en arrivant ; l'îlot masque une ligne directe sans devenir un labyrinthe.
- Deux chemins réellement praticables autour des bagages ; une esquive ne doit pas finir dans une collision décorative.
- Passage de service de 6 m, avec assez de place pour croiser et combattre ; pas une passerelle d'une personne.
- Cabine et signal comme repère de sortie, bordures basses au premier plan, aucun toit au-dessus de la zone de combat.
- Trois séquences perceptibles depuis la caméra du jeu, pas seulement depuis une vue d'ensemble éloignée.
- Ennemis accessibles, navigation continue, bordures qui empêchent la chute dans l'eau ; pas de sol invisible traversant le drain.
- Sortie avec la règle de regroupement native, après élimination des adversaires ; mène à la salle 2 existante.

La couleur des volumes indique leur fonction : ocre pour le débarquement, gris clair pour le combat, bleu-gris pour le passage technique, brun pour le fret, vert pour la sortie. Les sources lumineuses servent déjà à tester la lecture et les ombres ; les surfaces restent volontairement simples.

## Séquence de production après cette revue

1. Tester cette salle à la caméra réelle et ajuster dimensions, repères et rythme avec Nicolas.
2. Valider la composition ; seulement ensuite établir la liste d'assets à partir des blocs conservés.
3. Remplacer d'abord les grandes masses : halle, train, cabine, bords de canal. Les sols, murs et bordures resteront modulaires.
4. Réserver Meshy aux formes spécifiques qui apportent une silhouette utile ; réutiliser les assets existants quand ils conviennent. Vérifier dimensions, collisions, UV et normales à chaque remplacement.
5. Décliner les neuf autres compositions avant leur habillage ; reconnecter ensuite la variation PCG dans des zones explicitement autorisées.

En parallèle, le chantier animation reste à traiter : Conducteur et humanoïdes en template UE5 avec doigts ; audit source/retarget puis idle sobre de l'Éclaireuse ; attaques validées en vidéo avec appuis, arme en main, contact et récupération. Cette passe de blockout ne prétend pas corriger ces animations.

## Fichiers et lancement

- Références ImageGen : `References/quai-overview-v1.png`, `References/quai-gameplay-v1.png` ; prompts dans `References/PROMPTS.md`.
- Plan coté : `quai-plan.svg` ; parcours : `parcours.svg`.
- Carte indépendante : `/Game/Maps/Blockout/QuaiBlockout`.
- Reconstruction déterministe dans l'éditeur : `Scripts/build_quai_blockout.py` (primitives Unreal uniquement).
- Depuis `Unreal/` : `./Scripts/play-quai-blockout.sh`. Profil de test séparé ; résolution native par défaut.
- Vérification instrumentée : ajouter `-TrainBlockoutReview -ExecCmds=Train.BlockoutReview` ; captures dans `Saved/BlockoutReview/`.

Le wagon et les dix cartes existantes restent accessibles. Aucun crédit Meshy n'est dépensé pour ce blockout.
