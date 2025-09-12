#!/bin/bash

# Nombre del trabajo
#SBATCH --job-name=IMT2112
# Archivo de salida
#SBATCH --output=output_%j.txt
# Cola de trabajo
#SBATCH --partition=full
# Solicitud de cpus
#SBATCH --ntasks=8
#SBATCH --cpus-per-task=1

echo "start script"
date

module purge
module load gcc
module load openmpi

mpic++ tarea2.cpp
time mpirun -np 2 a.out

echo "end script"
date

