#!/bin/bash
make
./stop.sh
./start.sh
./test-lab-2-$1.pl ./yfs1
./stop.sh
