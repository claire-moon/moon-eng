#!/bin/bash

echo "** [1 / 3] COMPILING ENGINE... **"

make clean && make

# CHECK FOR SUCCESS

if [ $? -eq 0 ]; then

	echo "** IT COMPILED...! **"
	echo "** [2 / 3] STAGING FILES... **"

	git add .

	#TIMESTAMP

	TIMESTAMP=$(date +"%Y-%m-%d %H:%M:%S")

	echo "** [3 / 3] SAVING BUILD SNAPSHOT: $TIMESTAMP **"

	git commit -m "BUILD SNAPSHOT: $TIMESTAMP"

	echo "** [COMPLETE] BUILD SUCCEEDED...! **"
	echo "** [COMPLETE] HAVE A NICE DAY ...! **"

else

	echo "** [ERROR] COMPILATION FAILED...! **"

	echo "** [ERROR] ABORTING... **"

	exit 1

fi
