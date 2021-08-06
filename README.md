# README #

gem5 code repositories for DRAIN: Deadlock Removal for Arbitrary Irregular Networks

### Design Description ###
Section-3 of the paper describes the design

Mayank Parasar, ‪Hossein Farrokhbakht‬, Natalie Enright Jerger, Paul V. Gratz, Joshua San Miguel, Tushar Krishna

 In the 26th IEEE International Symposium on High-Performance Computer Architecture, HPCA-26, 2020

Paper:
   * https://mayank-parasar.github.io/website/papers/drain_hpca2020.pdf

### How to build ###
Under gem5/
* scons -j 15 build/Garnet_standalone/gem5.opt

### How to run ###

* See gem5/run_script.py
* To run: python run_script.py
* A handy (approx.) script for saturation throughtput gem5/sat_thrpt.py
* To run: python sat_thrpt.py

### Developer ###

* Mayank Parasar (mayankparasar@gmail.com)
