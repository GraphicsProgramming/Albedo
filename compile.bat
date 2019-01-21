@echo off

set FLAGS=/EHsc /O2 /Zi
set LIBRARIES=embree.lib

mkdir build
pushd build
cl -nologo /c ../external/tiny_obj_loader.cc %FLAGS%
cl -nologo ../src/main.cpp tiny_obj_loader.obj %FLAGS% /I ../external %LIBRARIES% /link /LIBPATH:../lib
popd
