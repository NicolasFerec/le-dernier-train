# Vision du jeu

Le jeu est un roguelite d’action en 3D stylisée avec une caméra orthographique à angle fixe, jouable seul ou en coop en ligne de deux à quatre joueurs. Il s’inspire de **SWORN**, **ANVIL: Vault Breaker** et **Chained Beasts**. SWORN sert de référence pour la préparation au salon et la progression de l’équipement. ANVIL inspire le choix du monde au départ et leur déblocage successif.

## 1. Préparer et jouer une partie

- **Au salon**, on se déplace entre des postes distincts pour choisir son personnage, s’équiper et dépenser sa monnaie. En coop, le salon est partagé et chaque joueur prépare son propre équipement.
- **Au départ**, on choisit un monde et un niveau de difficulté parmi ceux qui sont débloqués. En coop, l’hôte lance la partie pour le groupe.
- **Dans un monde**, on enchaîne les salles. On se déplace, on attaque et on esquive. La sortie s’ouvre quand tous les ennemis sont vaincus. En coop, le groupe passe ensemble dans la salle suivante.
- **En fin de partie**, on revient au salon et on retrouve ses choix et ses déblocages personnels, y compris après avoir joué chez un ami.

## 2. Mondes et difficultés

La cible initiale est de **trois mondes de dix salles chacun**. Une partie commence dans le monde choisi. Pour débloquer le deuxième, il faut terminer le premier au moins une fois. Pour débloquer le troisième, il faut terminer le deuxième.

Il y a **trois niveaux de difficulté**. Le premier est disponible dès le départ. Pour débloquer le suivant, il faut avoir terminé **les trois mondes** dans la difficulté précédente. Le monde et la difficulté se choisissent au même moment avant de lancer la partie.

À chaque expédition, l’agencement des obstacles et les rencontres varient. Les repères narratifs de chaque lieu, les entrées et les sorties restent identifiables ; la génération préserve les passages et l’espace de combat. Les ennemis apparaissent sur des zones praticables, à distance de l’arrivée du groupe.

La difficulté monte d’une salle à l’autre par le nombre, les rôles et la composition des adversaires. Un automate d’un type donné garde les mêmes points de vie, dégâts et capacités dans toutes les salles. La coopération ajoute des adversaires au budget des rencontres, sans augmenter leurs points de vie. Les boss restent des types distincts, avec leurs propres statistiques et comportements.

## 3. Progression permanente

La progression entre les parties doit changer sensiblement ce qu’un joueur peut faire et la façon dont il joue. Revenir au salon doit permettre de faire un choix qui compte pour la prochaine partie, qu’il s’agisse de changer de style de combat ou de développer un personnage favori.

Une **monnaie commune**, gagnée en jouant, permet de choisir entre trois usages.

- **Personnages.** Débloquer de nouveaux personnages, chacun avec son identité et son style de combat.
- **Armes.** Débloquer de nouvelles armes propres à chaque personnage. Chaque arme peut aussi être améliorée après son déblocage. Le choix d’une arme et de ses améliorations doit se ressentir en combat.
- **Passifs.** Investir dans l’arbre de compétences propre à chaque personnage, dans l’esprit d’un RPG. Les prérequis relient les compétences et permettent de développer le personnage dans plusieurs directions.

Les améliorations gagnées pendant une partie constituent un axe à définir séparément.

## 4. Solo et coop

Le solo et la coop suivent la même boucle. En coop, le groupe partage le salon, le monde choisi et le passage entre les salles. Chaque joueur garde sa monnaie, ses personnages, ses armes et ses compétences dans son propre profil. La session se termine si l’hôte quitte. Les sessions en ligne passent par Steam.

## 5. Univers et lore — Le Dernier Train

Un train blindé traverse un pays abandonné. Il abrite un équipage de survivants qui rouvre les voies, gare après gare. Chaque expédition permet de comprendre ce qui est arrivé au pays et de découvrir pourquoi quelqu’un attend encore ce train au bout de la ligne.

### Époque et équipement

Le Dernier Train se déroule dans un monde industriel d’inspiration steampunk, avec une part de fantastique. Le train blindé, les gares de pompage et les fonderies donnent à cet univers ses repères. L’époque exacte et la source d’énergie du train restent ouvertes.

Le corps à corps domine l’arsenal. Les bâtons, les épées et les haches côtoient quelques armes à feu. Dans le wagon-atelier, l’équipage peut adapter ses armes avec des mécanismes qui changent leur façon de combattre. Un bâton muni d’un module de propulsion pourrait par exemple déclencher une attaque circulaire rapide et puissante. Les armes gardent une silhouette reconnaissable et le fantastique permet des effets qui dépassent la mécanique ordinaire.

### Caméra de jeu

Le jeu adopte une **vue isométrique à angle fixe, en projection orthographique**, comme dans les [captures de gameplay de SWORN](https://store.steampowered.com/app/1763250/SWORN/). Les deux axes du sol apparaissent en diagonale. Le sol occupe l’essentiel de l’image et on voit assez du côté des personnages et des objets pour reconnaître leurs silhouettes.

L’inclinaison et l’orientation de la caméra restent fixes, dans le train comme dans toutes les salles. La caméra suit le personnage du joueur. En coop, chacun suit son propre personnage avec le même angle de vue. Le décor défile à mesure qu’on avance. Les éléments hauts, notamment les murs du wagon, laissent les chemins, les personnages et les postes interactifs visibles.

Le décor remplit toute la fenêtre, quel que soit son format. Sur un écran plus large que le 21:9, les éléments du HUD restent regroupés dans une zone centrale de largeur équivalente à du 21:9.

### Exploration et taille des cartes

Les cartes jouables s’étendent sur plusieurs écrans, aussi bien dans le train qu’en mission. Une salle peut être une grande zone à explorer avant d’en trouver la sortie. Dans le train, on peut traverser plusieurs wagons pour rejoindre les différents postes. Le cadre montre les alentours immédiats du personnage et révèle la suite du trajet au fil des déplacements.

Les essais graphiques de gameplay montrent une portion de carte à l’échelle du personnage. Des chemins, des rails ou des passages entre wagons se poursuivent hors cadre et donnent une direction à l’exploration. Pour comparer plusieurs modèles, on garde la même vue isométrique, le même cadre de référence et la même échelle de personnage.

### Ambiance et lisibilité

L’univers est sombre par son histoire et son atmosphère. Les gares désertées, les machines qui tournent encore et les traces des anciens voyageurs racontent ce qui a été perdu. À bord, les lampes chaudes et les signes de vie de l’équipage donnent au train un air de refuge.

Les salles doivent rester lisibles au premier regard, même de nuit ou dans la brume. On distingue le sol praticable, les obstacles et la sortie, ainsi que les silhouettes des joueurs et des ennemis. Les décors peuvent employer des couleurs franches et des contrastes marqués. En combat, les effets laissent les menaces et les trajectoires visibles, même avec quatre joueurs.

### Le train

Le train est le salon du jeu. Entre les sorties, l’équipage vit à bord. On choisit et améliore son arme dans le wagon-atelier, on développe ses compétences dans un wagon aménagé pour l’entraînement et on choisit sa destination sur le tableau des départs. Les pièces rapportées servent à recruter des survivants, à financer l’entraînement et à débloquer ou améliorer l’équipement.

Après une mission, le train récupère l’équipe à un point de rendez-vous. Si le groupe tombe, le conducteur le rappelle à bord grâce aux balises que portent les personnages. La mission est perdue, mais l’équipage peut repartir. Une fois une voie rouverte, sa destination reste accessible depuis le tableau des départs.

### Les trois destinations

1. **Les Faubourgs noyés.** L’eau a submergé les quartiers bas et coupé les rails. L’équipage remet en marche une gare de pompage pour dégager la voie vers la fonderie.
2. **La Fonderie des géants.** Une usine continue de fabriquer des machines de guerre pour une armée disparue. L’équipage détourne son énergie pour permettre au train de franchir les montagnes.
3. **La Gare du bout du monde.** Tous les trains disparus y arrivent encore. Quelqu’un y tient le registre des passagers et attend le retour du Dernier Train.

À la fin de la guerre, l’armée a ordonné aux automates d’ouvrir les barrages, de fermer les gares et d’empêcher quiconque de rouvrir la voie. Le train blindé a emporté les derniers survivants avant la fermeture de la ligne. Les automates défendent encore les installations condamnées. Au terminus, un automate de gare tient le registre des convois vides et attend l’arrivée du Dernier Train. Le passé des membres de l’équipage reste à définir.

Dans les Faubourgs noyés, l’équipage suit l’ancienne voie d’évacuation depuis le quai et la rue des vannes jusqu’à l’entrepôt, puis traverse le canal et affronte l’Éclusier. Il remonte ensuite le réseau d’eau à travers le marché, les collecteurs et la salle des chaudières, passe par le poste de répartition et affronte le gardien de la station. Une fois la station sécurisée, l’équipage remet les pompes en marche pour dégager la voie.
