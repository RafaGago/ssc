cwd=$(pwd)
bl=$cwd/base_library/

cd $bl/build/premake
./premake5 gmake
make -C ../linux -j4 config=debug > /dev/null
make -C ../linux -j4 config=release > /dev/null
