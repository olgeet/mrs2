#!/bin/bash
#File: RunMappedUniformMDESimulations.sh

#rm *.txt #be careful not to remove txt files that you want to keep

NUM_SIMS=3; #How many simulations

DIM=1 #dimensions
HOLDOUTPERCENT=0.33 #how many percent of the data to hold out
MAXLEAVESEST=100 #maximum number of leaves in the function estimator
CRITLEAVES=100 #split until this number of leaves in the PQ
NUM_CHECKS=10 #collate num_checks histogram
NUM_ITERS=5 #number of iterations for "zooming-in" 

for N in 150
do
   #mkdir ${MYDIR}/n${N}critleaves${CRITLEAVES}
	for DATASEED in `seq 1 ${NUM_SIMS}`
		do 
		echo Simulation ${NUM_SIMS} for n = $N and CRITLEAVES = ${CRITLEAVES}
		./UniformMDE ${DATASEED} ${DIM} $N ${HOLDOUTPERCENT} ${MAXLEAVESEST} ${CRITLEAVES} ${NUM_CHECKS} ${NUM_ITERS}  
		#mv *.txt ${MYDIR}/n${I}L${L}
		#append the results for each loop to an overall file #make sure in the correct folder depending on n and L
		cat iaes_leaves${DATASEED}.txt >> n${N}critleaves${CRITLEAVES}all.txt
		rm iaes_leaves${DATASEED}.txt
	done
done