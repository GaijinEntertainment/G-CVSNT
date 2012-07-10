REM Kill Explorer - could also use pskill.exe
taskkill /f /im explorer.exe
ren TrtseShl.dll TrtshShl%random%.dll
copy TortoiseShell.dll TrtseShl.dll
REM Restart Explorer - omit this if you want to start Explorer in a debugger
start explorer 