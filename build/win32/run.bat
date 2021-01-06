@echo off
setlocal

pushd ..\..
call build\win32\config.bat
pushd %OutputPath%
%OutputName%.exe
popd
popd

endlocal
