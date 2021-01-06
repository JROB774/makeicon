@echo off

set VSWhere="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=1* delims=: " %%i in (`%VSWhere% -latest -requires Microsoft.Component.MSBuild -products *`) do (
    if /i "%%i"=="installationPath" set VSDevPath=%%j
)
set VSDevPath="%VSDevPath%\Common7\Tools\vsdevcmd.bat"
