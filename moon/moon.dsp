# Microsoft Developer Studio Project File - Name="moon" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=moon - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "moon.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "moon.mak" CFG="moon - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "moon - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "moon - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "moon - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f moon.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "moon.exe"
# PROP BASE Bsc_Name "moon.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "NMAKE /f moon.mak"
# PROP Rebuild_Opt "/a"
# PROP Target_File "moon.exe"
# PROP Bsc_Name "moon.bsc"
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "moon - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Cmd_Line "NMAKE /f moon.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "moon.exe"
# PROP BASE Bsc_Name "moon.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Cmd_Line "C:\zeus\build.bat"
# PROP Rebuild_Opt "C:\zeus\build.bat"
# PROP Target_File "C:\zeus\moon.bat"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "moon - Win32 Release"
# Name "moon - Win32 Debug"

!IF  "$(CFG)" == "moon - Win32 Release"

!ELSEIF  "$(CFG)" == "moon - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "MOONENG"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\console.c
# End Source File
# Begin Source File

SOURCE=..\hud.c
# End Source File
# Begin Source File

SOURCE=..\input.c
# End Source File
# Begin Source File

SOURCE=..\main.c
# End Source File
# Begin Source File

SOURCE=..\map.c
# End Source File
# Begin Source File

SOURCE=.\moon.c
# End Source File
# Begin Source File

SOURCE=..\physics.c
# End Source File
# Begin Source File

SOURCE=..\player.c
# End Source File
# Begin Source File

SOURCE=..\render.c
# End Source File
# Begin Source File

SOURCE=..\skybox.c
# End Source File
# Begin Source File

SOURCE=..\sprite.c
# End Source File
# End Group
# Begin Group "CGUI"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\cgui\cgui-main.c"
# End Source File
# Begin Source File

SOURCE="..\cgui\cgui-widgets.c"
# End Source File
# End Group
# Begin Group "MDPED"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\mdped\mdped.c
# End Source File
# End Group
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE="..\cgui\cgui-pal.h"
# End Source File
# Begin Source File

SOURCE="..\cgui\cgui-widgets.h"
# End Source File
# Begin Source File

SOURCE=..\cgui\cgui.h
# End Source File
# Begin Source File

SOURCE=..\defs.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=..\build.bat
# End Source File
# Begin Source File

SOURCE=..\Makefile
# End Source File
# Begin Source File

SOURCE=..\moon.bat
# End Source File
# End Group
# End Target
# End Project
