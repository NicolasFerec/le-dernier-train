# Vision du projet

Lire [VISION.md](VISION.md) avant de concevoir ou de modifier le gameplay, la progression ou l’interface du jeu. Le [README](README.md) décrit comment lancer, tester et distribuer la version actuelle.

Le projet se trouve dans [`Unreal/`](Unreal/). Appliquer aussi [Unreal/AGENTS.md](Unreal/AGENTS.md).

# Développement et releases

- Tester les modifications avec la version de développement en local sur cet ordinateur. Éviter les builds GitHub Actions et les releases pour les itérations de développement.
- Une fois les changements validés localement, ouvrir une PR et attendre la review.
- Après la review, préparer et publier une release stable uniquement sur demande explicite de l’utilisateur. Générer automatiquement le changelog à partir des changements GitHub depuis la release précédente.
- Construire les paquets Unreal en local. Lancer les tests locaux depuis le projet Unreal.
