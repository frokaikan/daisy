# This file is just for showing the executed commands.
# Do not run it directly!

# download & checkout
git clone https://gitlab.freedesktop.org/freetype/freetype.git
git checkout 1e2eb65048f75c64b68708efed6ce904c31f3b2f

# paste __TDDCaseConfig to .

# compile & get declarations
cmake .. -GNinja
TDD_DUMP_DECL=1 ninja -j1

# compile cases & instruct
TDD_CASE=1 $LLVM_DIR/build_release/bin/clang ftsample.c -o ftsample.exe -gdwarf-4 -fstandalone-debug -O0 -Xclang -disable-O0-optnone -fPIC -DNDEBUG -fplugin=/home/frokaikan/Desktop/workspace/passes/TDD_NewSuite.so -fpass-plugin=/home/frokaikan/Desktop/workspace/passes/TDD_NewPasses.so -I ../include -I ../include/freetype libfreetype.a -lz -lbz2 -lpng -lbrotlidec $TDD/TDD_Interceptors.so -DFT2_BUILD_LIBRARY
TDD_CASE=1 $LLVM_DIR/build_release/bin/clang test_afm.c -o test_afm.exe -gdwarf-4 -fstandalone-debug -O0 -Xclang -disable-O0-optnone -fPIC -DNDEBUG -fplugin=/home/frokaikan/Desktop/workspace/passes/TDD_NewSuite.so -fpass-plugin=/home/frokaikan/Desktop/workspace/passes/TDD_NewPasses.so -I ../include -I ../include/freetype libfreetype.a -lz -lbz2 -lpng -lbrotlidec $TDD/TDD_Interceptors.so -DFT2_BUILD_LIBRARY

# execute cases & get OUS
./test_afm.exe sample.afm
cp __TDDCallingChain.json __TDDCallingChain.testafm.json
./ftsample.exe codicon.ttf
cp __TDDCallingChain.json __TDDCallingChain.ftsample.json

# merge OUS & build fuzz driver
python $TDD/TDD_OUSMerger.py __TDDCallingChain.ftsample.json __TDDCallingChain.testafm.json
python $TDD/TDD_DriverGenerator.py

# fix errors & memory leaks
patch __TDDDriver.cc freetype.diff

# compile (build with asan) & run
$LLVM_DIR/build_release/bin/clang++ -gdwarf-4 -fstandalone-debug -O0 -DNDEBUG -Xclang -disable-O0-optnone -fPIC -fsanitize=address,fuzzer -fprofile-instr-generate -fcoverage-mapping -std=c++17 -o __TDDDriver.exe __TDDDriver.cc -I ../include -I ../include/freetype libfreetype.a -lz -lbz2 -lpng -lbrotlidec
./__TDDDriver.exe