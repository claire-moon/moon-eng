@ECHO OFF
C:
cd \zeus
set DJGPP=C:\djgpp\djgpp.env

:Menu
CLS
ECHO =======================
ECHO   M O O N   B U I L D
ECHO =======================
ECHO.
ECHO   1.  COMPILE ALL
ECHO   2.  COMPILE ZEUS
ECHO   3.  COMPILE MDPED
ECHO   4.  COMPILE SOUND TEST
ECHO   5.  CLEAN DIRECTORY
ECHO   6.  EXIT TO DOS
ECHO.

CHOICE /C:123456 /N "PRESS 1-6:"

IF ERRORLEVEL 6 GOTO End
IF ERRORLEVEL 5 GOTO Clean
IF ERRORLEVEL 4 GOTO BuildSound
IF ERRORLEVEL 3 GOTO BuildMdped
IF ERRORLEVEL 2 GOTO BuildZeus
IF ERRORLEVEL 1 GOTO BuildAll

:Clean
C:\djgpp\bin\make.exe clean
GOTO Menu

:BuildZeus
C:\djgpp\bin\make.exe zeus.exe
PAUSE
GOTO Menu

:BuildMdped
C:\djgpp\bin\make.exe mdped.exe
PAUSE
GOTO Menu

:BuildSound
C:\djgpp\bin\make.exe sound.exe
PAUSE
GOTO Menu

:BuildAll
C:\djgpp\bin\make.exe clean
C:\djgpp\bin\make.exe
PAUSE
GOTO Menu

:End
