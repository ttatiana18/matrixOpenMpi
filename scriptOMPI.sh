#! /bin/bash
for j in {1..10}
do
for i in 100 300 500 700 1000 2000 3000 5000
do
./matrixOMPI $i >> openMpi.csv
done
done
