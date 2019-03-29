@echo off
:: Copyright 2018-2019 Abel Cheung
:: All rights reserved.

set OLDPATH=%PATH%
PATH C:\msys64\%MSYSTEM%\bin;C:\msys64\usr\bin;%OLDPATH%

cd %APPVEYOR_BUILD_FOLDER%
goto %1

echo Unknown build target.
exit 1


:build
@echo on

:: Build fails (unresolved dep) w/o updated msys2 runtime :-/
pacman.exe --noconfirm --noprogressbar -Syu

:: Second invocation updates everything else
:: Wastes too much time updating unnecessary non-core stuff
:: pacman --noconfirm --noprogressbar -Syu

pacman.exe --noconfirm --noprogressbar -S --needed markdown mingw-w64-%MSYS2_ARCH%-glib2
bash -lc "autoreconf -f -i -v && ./configure --enable-static && make all"

@echo off
goto :eof


:check
@echo on

file.exe src\rifiuti.exe
ldd.exe  src\rifiuti.exe

file.exe src\rifiuti-vista.exe
ldd.exe  src\rifiuti-vista.exe

bash -lc "make check"

@echo off
goto :eof


:package
@echo on

if "%APPVEYOR_REPO_TAG%" == "true" (
    echo "*** Building official release ***"
    bash -lc "make -f dist-win.mk dist-win"
) else (
    bash -lc "make -f dist-win.mk dist-win ZIPNAME=%APPVEYOR_PROJECT_SLUG%-%APPVEYOR_REPO_COMMIT:~0,8%-win-%MSYS2_ARCH%"
)

@echo off
goto :eof
