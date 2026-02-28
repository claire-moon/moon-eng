#!/bin/bash

echo "** [WINBUILD] COMPILING... **"

make clean && make

"/mnt/c/Program Files (x86)/DOSBox-0.74-3/DOSBox.exe" -conf dosbox.conf &

if [ $? -eq 0 ]; then

	echo "** [SUCCESS] BUILD COMPLETE...! **"
	echo "** [WINBUILD] LAUNCHING DOSBOX... **"

else

	echo "** [FAILURE] BUILD FAILED...! **"
	echo "** [WINBUILD] DON'T WORRY...! **"
	echo "** [WINBUILD] TRY AGAIN...! **"

	exit 1

fi
