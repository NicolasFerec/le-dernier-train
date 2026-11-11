# Wagon peint

Sources générées avec ImageGen. Les textures importées par Godot sont dans `arena/train/`.

- `surfaces.png` fournit quatre matières orthographiques de face. Plancher en noyer et voies vus de dessus, paroi bleue à fenêtres et bas de caisse riveté vus de face. Le jeu projette chaque surface avec les axes du wagon.
- `mobilier.png` demande huit objets détourés dans la même vue isométrique à 30°. Vestiaire, établi d’armurerie, pupitre de compétences, tableau des départs, télégraphe, mannequin sur ressort, caisse et lanterne. Les objets ont un dessus et deux faces visibles. Le mobilier est découpé en sprites indépendants, et certaines pièces sont retournées horizontalement pour les aligner sur le wagon.
- `entrainement.png` représente un tapis en toile bleu ardoise à bords de cuivre. La texture est peinte à plat, avec des traces d’usure et trois bandes, puis projetée sur le plancher par le jeu.
