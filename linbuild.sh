#!/bin/bash

echo "** [LINBUILD] COMPILING... **"

make clean && make

if [ $? -eq 0 ]; then

	echo "** [SUCCESS] BUILD COMPLETE...! **"
	echo "** [LINBUILD] LAUNCHING WSL DOS... **"

	dosbox -conf dosbox.conf &

else

	echo "** [FAILURE] BUILD FAILED...! **"
	echo "** [LINBUILD] NO WORRIES...! **"
	echo "** [LINBUILD] TRY AGAIN...! **"

	exit 1

fi
