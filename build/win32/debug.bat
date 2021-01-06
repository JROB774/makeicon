@echo off
setlocal

pushd ..\..
call build\win32\findvs.bat
call build\win32\config.bat
call %VSDevPath% -no_logo -arch=%Architecture%
devenv %OutputExecutable%.exe
popd

endlocal
