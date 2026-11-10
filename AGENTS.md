# Vision du projet

Lire [VISION.md](VISION.md) avant de concevoir ou de modifier le gameplay, la progression ou l’interface du jeu. Le [README](README.md) décrit comment lancer, tester et distribuer la version actuelle.

# Localisation du jeu

Le français est la langue par défaut. Pour chaque ajout ou modification de texte visible dans le jeu, renseigner le français et l’anglais dans `locale/game.csv` dans le même changement. Utiliser une clé stable dans les scènes et `tr()` pour les textes construits en GDScript. Garder les phrases complètes dans le catalogue pour que chaque langue puisse choisir son ordre et sa ponctuation.

# Développement et releases

- Tester les modifications avec la version de développement en local sur cet ordinateur. Éviter les builds GitHub Actions et les releases pour les itérations de développement.
- Une fois les changements validés localement, ouvrir une PR et attendre la review.
- Après la review, préparer et publier une release uniquement sur demande explicite de l’utilisateur. Générer automatiquement le changelog à partir des changements GitHub depuis la release précédente.
