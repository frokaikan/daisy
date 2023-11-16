LLVM_DIR = ~/Desktop/workspace/llvm-project-2
CXX = $(LLVM_DIR)/build_release/bin/clang++
CXX_DEBUG = $(LLVM_DIR)/build_debug/bin/clang++

.PHONY : all
all : TDD_Interceptors.so TDD_NewSuite.so TDD_NewPasses.so

.PHONY : debug
debug : TDD_Interceptors.debug.so TDD_NewSuite.debug.so TDD_NewPasses.debug.so

TDD_Interceptors.so : TDD_Interceptors.cc
	$(CXX) -gdwarf-4 -fstandalone-debug -O0 -Xclang -disable-O0-optnone -fPIC -std=c++17 -DNDEBUG -shared $^ -o $@ -ldl

TDD_Interceptors.debug.so : TDD_Interceptors.cc
	$(CXX) -gdwarf-4 -fstandalone-debug -O0 -Xclang -disable-O0-optnone -fPIC -std=c++17 -DNDEBUG -shared $^ -o $@ -ldl

TDD_NewPasses.so : TDD_NewPasses.cc
	$(CXX) -gdwarf-4 -fstandalone-debug -O3 -Xclang -disable-O0-optnone -fPIC -std=c++17 -DNDEBUG -shared $^ -o $@ -I $(LLVM_DIR)/llvm/include/ -I $(LLVM_DIR)/build_release/include/

TDD_NewPasses.debug.so : TDD_NewPasses.cc
	$(CXX_DEBUG) -gdwarf-4 -fstandalone-debug -O0 -Xclang -disable-O0-optnone -fPIC -std=c++17 -DNDEBUG -shared $^ -o $@ -I $(LLVM_DIR)/llvm/include/ -I $(LLVM_DIR)/build_debug/include/

TDD_NewSuite.so : TDD_NewSuite.cc
	$(CXX) -gdwarf-4 -fstandalone-debug -O3 -Xclang -disable-O0-optnone -fPIC -std=c++17 -DNDEBUG -shared $^ -o $@ -I $(LLVM_DIR)/clang/include/ -I $(LLVM_DIR)/build_release/tools/clang/include/ -I $(LLVM_DIR)/llvm/include/ -I $(LLVM_DIR)/build_release/include/

TDD_NewSuite.debug.so : TDD_NewSuite.cc
	$(CXX_DEBUG) -gdwarf-4 -fstandalone-debug -O0 -Xclang -disable-O0-optnone -fPIC -std=c++17 -DNDEBUG -shared $^ -o $@ -I $(LLVM_DIR)/clang/include/ -I $(LLVM_DIR)/build_debug/tools/clang/include/ -I $(LLVM_DIR)/llvm/include/ -I $(LLVM_DIR)/build_debug/include/

clean:
	rm -rf *.so