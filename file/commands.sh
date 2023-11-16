# This file is just for showing the executed commands.
# Do not run it directly!

# clone & checkout
git clone https://github.com/file/file.git
git checkout 0c9d8c4dc527769ed871f510c80c25984f8191fc

# paste __TDDCaseConfig to ./src

# compile & get declarations & compile case & instruct
../configure
TDD_DUMP_DECL=1 TDD_CASE=1 ninja -j1
cd src

# execute cases & get OUS
./file -m magic.mgc ./file
cp __TDDCallingChain.json __TDDCallingChain.file.json

# merge OUS & build fuzz driver
python $TDD/TDD_OUSMerger.py __TDDCallingChain.file.json
python $TDD/TDD_DriverGenerator.py

# fix errors & memory leaks
patch __TDDDriver.cc file.diff

# compile (build with asan)
$LLVM_DIR/build_release/bin/clang++ -gdwarf-4 -fstandalone-debug -O0 -DNDEBUG -Xclang -disable-O0-optnone -fPIC -fsanitize=address,fuzzer -fprofile-instr-generate -fcoverage-mapping -std=c++17 -o __TDDDriver.exe __TDDDriver.cc -I ../../src -I . ./.libs/libmagic.so -DHAVE_STRCASESTR
LD_LIBRARY_PATH=./.libs ./__TDDDriver.exe