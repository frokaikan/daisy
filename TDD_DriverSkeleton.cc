/* TDD Driver from OUS
 * Compile Command : // @@ DRIVER COMPILE COMMAND @@
 */

#ifndef __TDD_DRIVER
#define __TDD_DRIVER

#include <cerrno>
#include <cinttypes>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <type_traits>

#define __TDD_ERROR_EXIT_CODE 110
#define __TDD_OBJECT_CREATE_FAIL 120
#define __TDD_UNEG1 static_cast<uint64_t>(-1)
#define __TDD_DEBUG__ 1
#if (defined (__TDD_DEBUG__) && __TDD_DEBUG__)
#define __TDD_ASSERT(cond, msg) \
    if (!(cond)) { \
        std::cerr << msg << " !! \n"; \
        std::cerr << "Error in ==> " << __FILE__ << " :: " << __LINE__ << " \n"; \
        exit(__TDD_ERROR_EXIT_CODE); \
    } \
    void(0)
#else
#define __TDD_ASSERT(cond, msg) void(0)
#endif
#define __TDD_DRIVER_MIN_SIZE // @@ MIN SIZE @@
#define __TDD_DRIVER_MAX_SIZE // @@ MAX SIZE @@
#define __TDD_FLOATING_EPS 1e-9
#define __TDD_FLOATING_EQUAL(a, b) (-__TDD_FLOATING_EPS <= a - b && a - b <= __TDD_FLOATING_EPS)

// @@ PRE OPERATIONS @@

namespace TDD {

const uint8_t* __TDD_global_begin  = nullptr;
const uint8_t* __TDD_global_end    = nullptr;
const uint8_t* __TDD_global_cursor = nullptr;
std::default_random_engine __TDD_random_engine;
std::uniform_real_distribution<double> __TDD_double_distribution = std::uniform_real_distribution<double>(0, 1);
std::vector<std::function<void ()>> __TDD_funcitons_run_on_exit;
uint64_t __TDD_file_count = 0;
std::vector<void*> __TDD_ptr;
std::vector<uint64_t> __TDD_ptr_size;
std::map<uint64_t, uint64_t> __TDD_load_ptr;

void __TDD_driver_save_to_file (char* fileName, const void* data, size_t size) {
    if (size == 0) {
        size = strlen(static_cast<const char*>(data));
    }
    FILE* file = fopen(fileName, "w");
    fwrite(data, size, 1, file);
    fclose(file);
}

void __TDD_driver_save_to_file_append (char* fileName, const void* data, size_t size) {
    if (size == 0) {
        size = strlen(static_cast<const char*>(data));
    }
    FILE* file = fopen(fileName, "a");
    fwrite(data, size, 1, file);
    fclose(file);
}

void __TDD_driver_register_free (std::function<void ()> func) {
    __TDD_funcitons_run_on_exit.emplace_back(func);
}

uint8_t __TDD_driver_next () {
    uint8_t ret = *__TDD_global_cursor++;
    if (__TDD_global_cursor == __TDD_global_end) {
        __TDD_global_cursor = __TDD_global_begin;
    }
    return ret;
}

uint64_t __TDD_driver_integer () {
    uint64_t ret = 0;
    for (int _ = 0; _ < 8; ++_) {
        ret <<= 8;
        ret |= __TDD_driver_next();
    }
    return ret;
}

double __TDD_driver_floating () {
    return __TDD_double_distribution(__TDD_random_engine);
}

uint64_t __TDD_driver_size () {
    uint64_t size = __TDD_driver_integer();
    size %= __TDD_DRIVER_MAX_SIZE - __TDD_DRIVER_MIN_SIZE + 1;
    size += __TDD_DRIVER_MIN_SIZE;
    return size;
}

void* __TDD_driver_alloc (uint64_t size, bool allZero = true) {
    uint8_t* ret = static_cast<uint8_t*>(malloc(size));
    if (!allZero) {
        for (uint64_t i = 0; i < size; ++i) {
            ret[i] = __TDD_driver_next();
        }
    } else {
        memset(ret, 0, size);
    }
    __TDD_driver_register_free([=] () {free(ret);});
    return ret;
}

void* __TDD_driver_string (uint64_t& size) {
    uint8_t* ret;
    size = __TDD_driver_size();
    ret = static_cast<uint8_t*>(malloc(size + 1));
    for (uint64_t i = 0; i < size; ++i) {
        ret[i] = __TDD_driver_next();
    }
    ret[size] = '\0';
    __TDD_driver_register_free([=] () {free(ret);});
    return ret;
}

template <typename T>
void* __TDD_driver_typed_alloc (uint64_t& size) {
    if (std::is_same_v<T, void*> || std::is_same_v<T, const void*>) {
        return __TDD_driver_string(size);
    } else if (std::is_reference_v<T>) {
        size = sizeof (std::remove_reference_t<T>);
    } else if (std::is_pointer_v<T>) {
        size = sizeof (std::remove_pointer_t<T>);
    } else {
        __TDD_ASSERT (false, "__TDD_driver_typed_alloc without a reference or pointer type");
    }
    if (size != 1) {
        return __TDD_driver_alloc(size);
    } else {
        return __TDD_driver_string(size);
    }
}

template <typename T>
bool __TDD_driver_get_ptr (uint64_t idx) {
    if (__TDD_ptr.at(idx)) {
        return true;
    }
    std::vector<std::pair<uint64_t, uint64_t>> loadChain;
    for (uint64_t dst = idx; __TDD_load_ptr.count(dst); ) {
        uint64_t src = __TDD_load_ptr.at(dst);
        loadChain.push_back({dst, src});
        if (__TDD_ptr[src]) {
            break;
        } else {
            dst = src;
        }
    }
    if (!loadChain.empty()) {
        while (!loadChain.empty()) {
            std::pair<uint64_t, uint64_t> loadPair = loadChain.back();
            uint64_t dst = loadPair.first, src = loadPair.second;
            loadChain.pop_back();
            if (!__TDD_ptr[src]) {
                return false;
            } else {
                __TDD_ptr[dst] = *static_cast<void**>(__TDD_ptr[src]);
                if (!__TDD_ptr[dst]) {
                    return false;
                }
            }
        }
        return true;
    } else {
        void*& ptr = __TDD_ptr.at(idx);
        uint64_t& size = __TDD_ptr_size.at(idx);
        ptr = __TDD_driver_typed_alloc<T>(size);
        return true;
    }
}

template <typename T>
T __TDD_driver_get_typed_object (uint64_t idx) {
    if (std::is_same_v<T, void*> || std::is_same_v<T, const void*>) {
        return static_cast<T>(__TDD_ptr.at(idx));
    } else if (std::is_reference_v<T>) {
        return *static_cast<std::add_pointer_t<std::remove_reference_t<T>>>(__TDD_ptr.at(idx));
    } else if (std::is_pointer_v<T>) {
        return static_cast<T>(__TDD_ptr.at(idx));
    } else {
        __TDD_ASSERT (false, "__TDD_driver_get_typed_object without a reference or pointer type");
    }
}

template <typename T>
T __TDD_driver_get_typed_value () {
    if (std::is_integral_v<T>) {
        return static_cast<T>(__TDD_driver_integer());
    } else if (std::is_floating_point_v<T>) {
        return static_cast<T>(__TDD_driver_floating());
    } else {
        return {};
    }
}

template <typename T>
void __TDD_driver_set_ptr (uint64_t idx, T value) {
    __TDD_ASSERT (!__TDD_ptr.at(idx), "set on existing ptr");
    if (std::is_same_v<T, void*> || std::is_same_v<T, const void*>) {
        __TDD_ptr.at(idx) = static_cast<void*>(value);
    } else if (std::is_reference_v<T>) {
        __TDD_ptr.at(idx) = static_cast<void*>(&value);
    } else if (std::is_pointer_v<T>) {
        __TDD_ptr.at(idx) = static_cast<void*>(value);
    } else {
        __TDD_ASSERT (false, "__TDD_driver_set_ptr without a reference or pointer type");
    }
}

void* __TDD_driver_file_name () {
    std::string realFileName = "__TDD_file_" + std::to_string(__TDD_file_count++);
    remove(realFileName.c_str());
    FILE* file = fopen(realFileName.c_str(), "w");
    uint64_t size = __TDD_driver_size();
    uint8_t* tmp = static_cast<uint8_t*>(malloc(size));
    for (uint64_t i = 0; i < size; ++i) {
        tmp[i] = __TDD_driver_next();
    }
    fwrite(tmp, size, 1, file);
    free(tmp);
    fclose(file);
    void* ret = malloc(realFileName.size() + 1);
    __TDD_driver_register_free([=] () {free(ret);});
    memcpy(ret, realFileName.c_str(), realFileName.size());
    static_cast<char*>(ret)[realFileName.size()] = '\0';
    return ret;
}

std::string __TDD_driver_std_string () {
    std::string ret;
    uint64_t size = __TDD_driver_size();
    ret.resize(size);
    for (uint64_t i = 0; i < size; ++i) {
        ret[i] = __TDD_driver_next();
    }
    return ret;
}

FILE* __TDD_driver_FILEptr () {
    char* fileName = static_cast<char*>(__TDD_driver_file_name());
    FILE* ret = fopen(fileName, "r+");
    fseek(ret, 0, SEEK_SET);
    __TDD_driver_register_free([=] () {fclose(ret);});
    return ret;
}

void __TDD_swap (uint64_t& a, uint64_t& b) {
    if (&a != &b) {
        a ^= b;
        b ^= a;
        a ^= b;
    }
}

struct TDD_Driver_Graph {
    std::vector<uint64_t> head, to, next, inDegree, currentNodes;
    uint64_t edgeCount, nodeCount;

    TDD_Driver_Graph (uint64_t nodeCount_) : head(nodeCount_, __TDD_UNEG1), inDegree(nodeCount_, 0), currentNodes(1, 0), edgeCount(0), nodeCount(nodeCount_) {}
    void addEdge (uint64_t fromIdx, uint64_t toIdx) {
        to.push_back(toIdx);
        next.push_back(head.at(fromIdx));
        inDegree.at(toIdx)++;
        head.at(fromIdx) = edgeCount++;
    }

    uint64_t getNext () {
        if (currentNodes.empty()) {
            return __TDD_UNEG1;
        }
        uint64_t selectedIdx = __TDD_driver_integer() % currentNodes.size();
        __TDD_swap(currentNodes.at(selectedIdx), currentNodes.back());
        uint64_t ret = currentNodes.back();
        currentNodes.pop_back();
        for (uint64_t edgeIdx = head.at(ret); edgeIdx != __TDD_UNEG1; edgeIdx = next.at(edgeIdx)) {
            uint64_t toIdx = to.at(edgeIdx);
            if (--inDegree.at(toIdx) == 0) {
                currentNodes.push_back(toIdx);
            }
        }
        return ret;
    }
};

void __TDD_driver_init (const uint8_t* buffer, size_t size) {
    __TDD_global_begin  = buffer;
    __TDD_global_end    = buffer + size;
    __TDD_global_cursor = buffer;
    __TDD_file_count = 0;
    __TDD_random_engine.seed(__TDD_driver_integer());
    __TDD_funcitons_run_on_exit.clear();
    __TDD_ptr.clear();
    __TDD_ptr_size.clear();
    __TDD_load_ptr.clear();
}

int __TDD_driver_main ();

void __TDD_driver_fin () {
    while (!__TDD_funcitons_run_on_exit.empty()) {
        auto func = __TDD_funcitons_run_on_exit.back();
        func();
        __TDD_funcitons_run_on_exit.pop_back();
    }
    __TDD_global_begin  = nullptr;
    __TDD_global_end    = nullptr;
    __TDD_global_cursor = nullptr;
}

extern "C" int LLVMFuzzerTestOneInput (const uint8_t* buffer, size_t size) {
    if (size == 0) {
        return 0;
    }
    __TDD_driver_init(buffer, size);
    __TDD_driver_main();
    __TDD_driver_fin();
    return 0;
}

int __TDD_driver_main () {
    // Pointers
    __TDD_ptr.resize(/* @@ POINTER COUNT @@ */, nullptr);
    __TDD_ptr_size.resize(/* @@ POINTER COUNT @@ */, 0);

    // Loads
// @@ PTR LOADS @@

    // Graph
    TDD_Driver_Graph __TDD_graph(/* @@ GRAPH NODE COUNT @@ */);
// @@ GRAPH EDGES @@

    // Nodes
    std::vector<std::function<bool ()>> nodes;
    nodes.push_back([&] () -> bool {
        __TDD_ASSERT (false, "Node 0 should never be executed");
    });

// @@ NODES @@

    for (uint64_t idx = __TDD_graph.getNext(); idx != __TDD_UNEG1; idx = __TDD_graph.getNext()) {
        if (idx != 0) {
            if (!nodes.at(idx)()) {
                break;
            }
        }
    }

    return 0;
}

} // namespace TDD

#endif // __TDD_DRIVER