#!/bin/bash

echo " [1 / 3] COMPILING ENGINE..."

make clean && make

# CHECK FOR SUCCESS

if [ $? -eq 0 ]; then

	echo " IT COMPILED...!"
	echo "[2 / 3] STAGING FILES..."

	git add .

	#TIMESTAMP

	TIMESTAMP=$(date +"%Y-%m-%d %H:%M:%S")

	git commit -m "BUILD SNAPSHOT: $TIMESTAMP"

	#PUSH IT

	echo "[3 / 3] PUSHING TO GITHUB..."

	git push origin main

	echo "BUILD SUCCEEDED...!"
	echo "**have a nice day...**"

else

	echo "COMPILATION FAILED...!"
	echo "ABORTING..."

	exit 1

fi
