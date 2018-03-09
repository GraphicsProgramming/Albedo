@echo off

set FLAGS="/EHsc"
set LIBRARIES=embree.lib

mkdir build
pushd build
cl -Zi ../code/main.cpp ../code/tiny_obj_loader.cc %FLAGS% /I ../include %LIBRARIES% /link /LIBPATH:../lib
popd
