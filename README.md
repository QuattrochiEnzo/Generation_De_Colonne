# Projet p-median – Integer Programming


## Arborescence

- `instances/`
  - toutes les instances `.inst`
- `src/`
  - `solver.cpp` : formulation compacte avec Gurobi
  - `solver_colgen.cpp` : génération de colonnes
  - `stabilization_colgen.cpp` : colgen avec stabilisation
  - `dynamic.cpp` : pricing par programmation dynamique
  - `visualisation.cpp` : sortie SVG pour voir les solutions
  - `Instance.hpp` : lecture instance + verif solution
  - `Solutions.hpp` : lecture / ecriture des solutions
  - `analyse_bench.py` : analyse des résultats
- `resultats/`
  - `bench.csv` : résultats bruts des runs
  - `analyse/` : stats
  - `svg/` : sortie visualisations
  - `tmp/` : détail des solutions par instance
- `Rapport_IP.pdf`
- `CMakeLists.txt`
- `FindGUROBI.cmake`

## Compilation

Depuis la racine :

```bash
mkdir build
cd build
cmake ..
make
````

Les executables sont dans `build/`.

## Lancer les programmes

Exemples :

```bash
./solver.exe sample.inst
./solver_colgen.exe sample.inst
./stabilization_colgen.exe sample.inst
./dynamic.exe sample.inst
```

Pour la visu :

```bash
./visualisation.exe sample.inst sample.sol sortie.svg
```

## Scripts d'analyse

Ils lisent `resultats/bench.csv` et sortent des csv + png dans `resultats/analyse/`.


## Notes

* le temps limite est fixé a 60s pour tous les runs
* si une methode depasse, on note `inf`



