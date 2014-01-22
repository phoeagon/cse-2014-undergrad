#!/bin/bash

mkdir myfs >/dev/null 2>&1

./start.sh

# run test 1
./test-lab-2-a.pl myfs | grep -q "Passed all"
if [ $? -ne 0 ];
then
        echo "Failed test-A"
        exit
fi
echo "Passed A"
./test-lab-2-b.pl myfs | grep -q "Passed all"
if [ $? -ne 0 ];
then
        echo "Failed test-B"
        exit
fi
echo "Passed B"
./test-lab-2-c.pl myfs | grep -q "Passed all"
if [ $? -ne 0 ];
then
        echo "Failed test-c"
        exit
fi
echo "Passed C"
./test-lab-2-d.sh myfs | grep -q "Passed SYMLINK"
if [ $? -ne 0 ];
then
        echo "Failed test-d"
        exit
fi
echo "Passed D"
./test-lab-2-e.sh myfs | grep -q "Passed BLOB"
if [ $? -ne 0 ];
then
        echo "Failed test-e"
        exit
fi
echo "Passed E"

# finally reaches here!
echo "Passed all tests!"

./stop.sh
