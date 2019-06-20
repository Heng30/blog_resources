@ECHO OFF

if "%1"=="clean" del *.exe *.obj *.dll *.lib *.exp
if not "%1"=="clean" cl /Fe:%1 *.c