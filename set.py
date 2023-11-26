# !/usr/bin/python3
# coding = utf-8

import os
import sys

realDir = os.path.realpath(f"{sys.path[0]}/..")

if len(sys.argv) == 2 and sys.argv[1] == "normal":
    data = f"""
export LLVM_DIR="{realDir}/llvm-project-2"
export TDD="{realDir}/passes"
export CC="{realDir}/llvm-project-2/build_release/bin/clang"
export CXX="{realDir}/llvm-project-2/build_release/bin/clang++"
export CFLAGS="-gdwarf-4 -fstandalone-debug -O0 -Xclang -disable-O0-optnone -fPIC -DNDEBUG -fplugin=$TDD/TDD_NewSuite.so -fpass-plugin=$TDD/TDD_NewPasses.so"
export CXXFLAGS="-gdwarf-4 -fstandalone-debug -O0 -Xclang -disable-O0-optnone -fPIC -std=c++17 -DNDEBUG -fplugin=$TDD/TDD_NewSuite.so -fpass-plugin=$TDD/TDD_NewPasses.so"
export LDFLAGS="$TDD/TDD_Interceptors.so"
export ASAN_OPTIONS="max_allocation_size_mb=8192:alloc_dealloc_mismatch=0"
"""
elif len(sys.argv) == 2 and sys.argv[1] == "fuzzing":
    data = f"""
export LLVM_DIR="{realDir}/llvm-project-2"
export TDD="{realDir}/passes"
export CC="{realDir}/llvm-project-2/build_release/bin/clang"
export CXX="{realDir}/llvm-project-2/build_release/bin/clang++"
export CFLAGS="-gdwarf-4 -fstandalone-debug -O0 -Xclang -disable-O0-optnone -fPIC -DNDEBUG -fprofile-instr-generate -fcoverage-mapping -fsanitize=address,fuzzer-no-link"
export CXXFLAGS="-gdwarf-4 -fstandalone-debug -O0 -Xclang -disable-O0-optnone -fPIC -std=c++17 -DNDEBUG -fprofile-instr-generate -fcoverage-mapping -fsanitize=address,fuzzer-no-link"
export LDFLAGS=""
export ASAN_OPTIONS="max_allocation_size_mb=8192:alloc_dealloc_mismatch=0"
"""
elif len(sys.argv) == 2 and sys.argv[1] == "unset":
    data = f"""
export LLVM_DIR=""
export TDD=""
export CC="clang"
export CXX="clang++"
export CFLAGS="-g -fstandalone-debug -O3 -Xclang -disable-O0-optnone -fPIC"
export CXXFLAGS="-g -fstandalone-debug -O3 -Xclang -disable-O0-optnone -fPIC -std=c++17"
export LDFLAGS=""
export ASAN_OPTIONS=""
"""
elif len(sys.argv) == 2 and sys.argv[1] == "fullUnset":
    data = f"""
export LLVM_DIR=""
export TDD=""
export CC="clang"
export CXX="clang++"
export CFLAGS=""
export CXXFLAGS=""
export ASAN_OPTIONS=""
"""
elif len(sys.argv) == 2 and sys.argv[1] == "compile":
    data = f"""
$CXX -gdwarf-4 -fstandalone-debug -O3 -Xclang -disable-O0-optnone -fPIC -std=c++17 -DNDEBUG -shared TDD_NewPasses.cc -o TDD_NewPasses.so -I $LLVM_DIR/llvm/include/ -I $LLVM_DIR/build_release/include/
$CXX -gdwarf-4 -fstandalone-debug -O3 -Xclang -disable-O0-optnone -fPIC -std=c++17 -DNDEBUG -shared TDD_Interceptors.cc -o TDD_Interceptors.so -ldl
$CXX -gdwarf-4 -fstandalone-debug -O3 -Xclang -disable-O0-optnone -fPIC -std=c++17 -DNDEBUG -shared TDD_NewSuite.cc -o TDD_NewSuite.so -I $LLVM_DIR/clang/include/ -I $LLVM_DIR/build_release/tools/clang/include/ -I $LLVM_DIR/llvm/include/ -I $LLVM_DIR/build_release/include/

-fpass-plugin=$TDD/TDD_NewPasses.so
-fplugin=$TDD/TDD_NewSuite.so
"""
elif len(sys.argv) == 2 and sys.argv[1] == "showEnv":
    data = f"""
TDD_DUMP_DECL (suite - instrument)
TDD_GET_DEP (suite - instrument)
TDD_CASE (case - instrument)
TDD_NO_CHAIN (case - execute)
$ target (target files)
$ case (case files)
$ pre operations
$ flags
$ min size
$ max size
$ no const int
"""
else:
    raise ValueError(f"Usage : eval $(python {sys.argv[0]} normal | fuzzing | unset | fullUnset | compile | showEnv)")

print(data)
