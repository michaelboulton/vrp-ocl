# Description

Follow on of some coursework I did to solve the Capacitated Vehicle routing Problem using a genetic algorithm whcih I did in OpenCL. It should be able to run on a CPU or GPU, it seems to have some problems running on a Xeon Phi though.

### Parameters

The different parameters such as mutation rate can be controlled through command line flags, do ./vrp-ocl -h to see what they do.

* Crossover - Different types of crossover can be specified - Order 1 (O1), Cyclic (CX), or PMX. A good explanation of them is [here](http://www.rubicite.com/Tutorials/GeneticAlgorithms/CrossoverOperators/Order1CrossoverOperator.aspx).

* Mutation - Different types of mutations include:
 * SWAP - choose a random range in a chromosome and swap it with another range of the same size in the chromosome
 * REVERSE - reverse a random range in a chromosome
 * SLIDE - take a random range in the chromosome and slide it to the left in the chromosome

* Mutation rate - chromosomes are mutated with a mutation\_rate/100 chance.

* Arena size - selection for chromosomes is done by specifying the size of the 'arena' first - the best chromosome in a population is chosen with 1/arena\_size chance, the second is chosen with 100/2\*arena\_size chance, etc.

* Population/generation size - The generation size is the number of total chromosomes, and the population size is the number of chromosomes in one population (total size should be a multiple of population size). This allows you to run 50 populations if size 32 at the same time, for example.

* Sorting strategy - Either choose the best out of all chromosomes after crossover or just choose the children and discard the old parents.

There are other parameters but these ones make the most difference

### Things to do

* Fix verbose output
* Stop it getting stuck in a local minimum
* Add rotation mutation
* Add edge recombination crossover
* Optimisation

