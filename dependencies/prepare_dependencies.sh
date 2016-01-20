#!/bin/sh

cwd=$(pwd)
bl=$cwd/base_library

cd $bl/dependencies
./prepare_dependencies.sh

cd $cwd/../build/premake
ln -sf $bl/build/premake/premake5

cd $cwd/libcoro
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_VERBOSE_MAKEFILE=ON .

cd $cwd
./recompile_base_library.sh

