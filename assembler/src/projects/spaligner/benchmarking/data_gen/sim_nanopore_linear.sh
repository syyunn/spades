#!/bin/bash

NANOSIMPATH=/home/tdvorkina/soft/NanoSim/src/
REF=ref/Caenorhabditis_elegans.WBcel235.dna.toplevel.fa
REALREADS=real_nanopore/real_nanopore.fasta

# Profiling stage, make sure to set the mode of read_analysis.py to -r-x or above
python2 $NANOSIMPATH/read_analysis.py -i $REALREADS -r $REF -o sim_nanopore

# Simulation stage, suppose the genome to be simulated is called test.fasta and make sure to provide the correct path to it
python2 $NANOSIMPATH/simulator.py linear --seed 115249 -n 40000 -r $REF  -c sim_nanopore # Note the -c option has to be the same as -o in read_analysis.py, or both use default parameter
