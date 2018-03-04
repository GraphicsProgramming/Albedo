@echo off

set FLAGS="/EHsc" "/O2"
set LIBRARIES=

mkdir build
pushd build
cl -Zi ../code/main.cpp %FLAGS% /I ../include %LIBRARIES% /link /LIBPATH:../lib
popd
