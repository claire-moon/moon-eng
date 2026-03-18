@ECHO OFF

ECHO *** COMPILING... ***

make all

IF ERRORLEVEL 1 GOTO FAIL

ECHO *** SUCCESS...! ***

GOTO END

:FAIL

ECHO  *** FAILED...! ***

:END