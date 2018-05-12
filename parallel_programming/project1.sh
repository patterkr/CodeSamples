#!/bin/bash

for thread in 1 2 4 8
do
	for node in 100 250 500 750 1000 2500 5000 7500 10000
	do
		g++ -DNUMT=$thread -DNUMNODES=$node project1.cpp -o project1 -lm -fopenmp
		./project1
	done
done
