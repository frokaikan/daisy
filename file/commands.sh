# This file is just for showing the executed commands.
# Do not run it directly!

# clone & checkout
git clone https://github.com/file/file.git
cd file
git checkout 0c9d8c4dc527769ed871f510c80c25984f8191fc
git restore .

# cd to driver building directory
mkdir DriverBuilder
cd DriverBuilder

# set environment by
python $TDD/set.py normal

# compile & get declarations & compile case & instruct
../configure
cp $TDD/file/* src
TDD_DUMP_DECL=1 TDD_CASE=1 make -j1
cd src

# execute cases & get OUS
./file -m magic.mgc ./file
cp __TDDCallingChain.json __TDDCallingChain.file.json

# merge OUS & build fuzz driver
python $TDD/TDD_OUSMerger.py __TDDCallingChain.file.json
python $TDD/TDD_DriverGenerator.py

# fix errors & memory leaks
patch __TDDDriver.cc file.diff

# cd to fuzzing directory
cd ..
cd ..
mkdir Fuzzing
cd Fuzzing

# set environment by
python $TDD/set.py fuzzing

# compile & fuzz
../configure
make
cd src
cp ../../DriverBuilder/src/__TDDDriver.cc .
$LLVM_DIR/build_release/bin/clang++ -gdwarf-4 -fstandalone-debug -O0 -DNDEBUG -Xclang -disable-O0-optnone -fPIC -fsanitize=address,fuzzer -fprofile-instr-generate -fcoverage-mapping -std=c++17 -o __TDDDriver.exe __TDDDriver.cc -I ../../src -I . ./.libs/libmagic.so -DHAVE_STRCASESTR
LD_LIBRARY_PATH=./.libs ./__TDDDriver.exe

# show crash
cp $TDD/file/crash-* .
LD_LIBRARY_PATH=./.libs ./__TDDDriver.exe crash-*