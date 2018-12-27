@echo off
:: Copyright 2018 Abel Cheung
:: License same as main package

cd %APPVEYOR_BUILD_FOLDER%
goto %1

echo Unknown build target.
exit 1


:build
@echo on

set OLDPATH=%PATH%
PATH C:\msys64\%MSYSTEM%\bin;C:\msys64\usr\bin;%OLDPATH%

:: Build fails (unresolved dep) w/o updated msys2 runtime :-/
C:\msys64\usr\bin\pacman.exe --noconfirm --noprogressbar -Syu

:: Wastes too much time updating unnecessary non-core stuff
:: bash -lc "pacman --noconfirm --noprogressbar -Syu"

C:\msys64\usr\bin\pacman.exe --noconfirm --noprogressbar -S --needed zip markdown mingw-w64-%MSYS2_ARCH%-glib2
bash -lc "./autogen.sh && ./configure --enable-static && make -C po rifiuti.pot && make all"

@echo off
goto :eof


:check
@echo on

c:\msys64\usr\bin\file.exe src\rifiuti.exe
c:\msys64\usr\bin\ldd.exe  src\rifiuti.exe

c:\msys64\usr\bin\file.exe src\rifiuti-vista.exe
c:\msys64\usr\bin\ldd.exe  src\rifiuti-vista.exe

bash -lc "make check"

@echo off
goto :eof


:package
@echo on

bash -lc "make dist-win"

:: todo rename daily builds, publish artifact

@echo off
goto :eof

