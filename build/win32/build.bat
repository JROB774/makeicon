@echo off
setlocal

pushd ..\..

call build\win32\findvs.bat
call build\win32\config.bat

call %VSDevPath% -no_logo -arch=%Architecture%

if not exist %OutputPath% mkdir %OutputPath%

if %BuildMode%==Release rc -nologo -i %ResourcePath% %ResourceFile%

call build\win32\timer.bat "cl %IncludeDirs% %Defines% %CompilerFlags% %CompilerWarnings% -Fe%OutputExecutable% %InputSource% -link %LinkerFlags% %LinkerWarnings% %LibraryDirs% %Libraries% %InputResource%"

pushd %OutputPath%
if %BuildMode%==Release del %ResourcePath%*.res
del *.ilk *.res *.obj *.exp *.lib
popd
del *.ilk *.res *.obj *.exp *.lib

popd

endlocal
