@ECHO OFF
:Menu

CLS
ECHO ============================
ECHO    M O O N  E N G   0 . 1
ECHO ============================
ECHO.
ECHO    1. PLAY ZEUS
ECHO    2. RUN MDPED
ECHO    3. RUN SOUNDTEST
ECHO    4. EXIT TO DOS
ECHO.

CHOICE /C:1234 /N "PRESS 1, 2, 3 OR 4:"

IF ERRORLEVEL 4 GOTO END
IF ERRORLEVEL 3 GOTO SOUND
IF ERRORLEVEL 2 GOTO EDIT
IF ERRORLEVEL 1 GOTO PLAY

:Play
CLS
zeus.exe
GOTO Menu

:Edit
CLS
mdped.exe
GOTO Menu

:Sound
CLS
sound.exe
GOTO Menu

:End
CLS
ECHO SEE YOU, SPACE COWBOY...
