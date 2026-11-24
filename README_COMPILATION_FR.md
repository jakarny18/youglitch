# YouGlitch SMP 1024 — dossier propre pour compilation

Ce dossier contient les sources du plugin JUCE, le fichier CMake et le workflow GitHub Actions Windows.

## Structure obligatoire

- `CMakeLists.txt` : configuration CMake à la racine.
- `Source/` : sources C++ du plugin et du module de streaming.
- `.github/workflows/windows-build.yml` : compilation automatique Windows.
- `BUILD_WINDOWS.bat` : compilation locale depuis une invite de commandes Windows avec Visual Studio et CMake installés.
- `Reference/` : copie de référence du projet HTML original (non nécessaire à CMake, conservée pour référence).

## Compilation avec GitHub

1. Décompresser cette archive.
2. Dans le dépôt GitHub, envoyer le CONTENU de ce dossier à la racine du dépôt (pas le dossier parent et pas le ZIP).
3. Vérifier que GitHub affiche `.github/workflows/windows-build.yml` et `CMakeLists.txt` à la racine.
4. Ouvrir l'onglet Actions. Le workflow se lance sur un push vers `main` ou `master`, ou manuellement avec `Run workflow`.
5. En cas d'échec, ouvrir le job `build-windows` et récupérer la première erreur rouge complète.

Le workflow télécharge JUCE 8.0.6 depuis GitHub pendant la configuration CMake. Le runner GitHub doit donc avoir accès au réseau.

## Important

Cette archive est une remise en ordre des fichiers et de la configuration de build. Elle n'est pas présentée comme compilée ou validée : l'environnement de préparation n'a pas pu télécharger JUCE depuis GitHub (résolution DNS indisponible). Il faut confirmer le résultat avec le journal GitHub Actions. En cas d'erreur C++, la première erreur du journal sera nécessaire pour corriger le code précisément.
