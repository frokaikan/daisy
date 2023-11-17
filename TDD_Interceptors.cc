#include <cstdlib>
#include <dlfcn.h>
#include <fcntl.h>
#include <vector>
#include <map>
#include <set>
#include <iostream>
#include <fstream>

#include "json.hpp"

#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define ASSERT(x, info) if (!x) {std::cerr << info << " !!\n"; abort();} void(0)
#define GET_SYSTEM_FUNCTION(funcName) static decltype(funcName)* sys_##funcName = nullptr; if (UNLIKELY(!sys_##funcName)) {sys_##funcName = reinterpret_cast<decltype(funcName)*>(dlsym(RTLD_NEXT, #funcName));} void(0)
#define UNEG1 static_cast<uint64_t>(-1)

namespace {

bool ifTrack = false;

std::map<const void*, std::pair<size_t, size_t>> buffers;
uint64_t bufferIdx;
std::vector<const void*> buffersOnStack;
std::set<const void*> fileNames;

std::string currentFunction;
struct FunctionParameter;
std::vector<FunctionParameter> functionParameters;
struct FunctionParameter {
    enum struct FunctionParameterType {INT, PTR, FUNC};
    uint64_t idx;
    FunctionParameterType type;
    int64_t intValue;
    bool isPreviousSize;
    const void* ptrValue;
    uint64_t ptrIdx;
    std::string funcValue;
    static FunctionParameter getInt (int64_t idx, int64_t value) {
        FunctionParameter ret;
        ret.idx = idx;
        ret.type = FunctionParameterType::INT;
        ret.intValue = value;
        ret.isPreviousSize = false;
        if (!functionParameters.empty() && functionParameters.back().idx == idx - 1 && functionParameters.back().type == FunctionParameterType::PTR) {
            const void* previousPtr = functionParameters.back().ptrValue;
            if (buffers.count(previousPtr) && buffers[previousPtr].first == value) {
                ret.isPreviousSize = true;
            }
        }
        return ret;
    }
    static FunctionParameter getPtr (uint64_t idx, const void* value) {
        FunctionParameter ret;
        ret.idx = idx;
        ret.type = FunctionParameterType::PTR;
        ret.ptrValue = value;
        if (!buffers.count(value)) {
            buffers[value] = {0, bufferIdx++};
        }
        ret.ptrIdx = buffers.at(value).second;
        return ret;
    }
    static FunctionParameter getFunc (uint64_t idx, std::string value) {
        FunctionParameter ret;
        ret.idx = idx;
        ret.type = FunctionParameterType::FUNC;
        ret.funcValue = value;
        return ret;
    }
};
const void* functionReturn;

std::string getInterestingName (std::string functionName, bool check = true) {
    static nlohmann::json functionList;
    static bool hasFunctionList = false;
    if (UNLIKELY(!hasFunctionList)) {
        hasFunctionList = true;
        std::ifstream is;
        is.open("__TDDDeclarations.json");
        ASSERT (is.is_open(), "__TDDDeclarations.json not exist");
        is >> functionList;
    }
    std::string realFunctionName;
    for (const char& c : functionName) {
        if (
            ('0' <= c && c <= '9') ||
            ('A' <= c && c <= 'Z') ||
            ('a' <= c && c <= 'z') ||
            (c == '_') ||
            (c == ':') ||
            (c == '~')
        ) {
            realFunctionName.push_back(c);
        } else if (c != ' ') {
            break;
        }
    }
    if (UNLIKELY(!check)) {return realFunctionName;}
    else if (functionList.count(realFunctionName)) {return realFunctionName;}
    else {return "";}
}

std::vector<std::pair<nlohmann::json, bool>> allTraces;
nlohmann::json getSimplifiedTrace () {
    std::map<uint64_t, uint64_t> usedPtrIndex;
    uint64_t realIndex = 0;

    for (std::pair<nlohmann::json, bool>& subTrace : allTraces) {
        nlohmann::json& subJ = subTrace.first;
        std::string type = subJ.at("type").get<std::string>();
        if (type == "CALL") {
            for (nlohmann::json& param : subJ.at("parameters")) {
                if (param.at("paramType").get<std::string>() == "PTR") {
                    uint64_t ptrIdx = param.at("ptrIndex").get<uint64_t>();
                    if (!usedPtrIndex.count(ptrIdx)) {
                        usedPtrIndex[ptrIdx] = realIndex++;
                    }
                    param["ptrIndex"] = usedPtrIndex.at(ptrIdx);
                }
            }
            if (subJ.count("return")) {
                uint64_t ptrIdx = subJ.at("return").get<uint64_t>();
                if (!usedPtrIndex.count(ptrIdx)) {
                    usedPtrIndex[ptrIdx] = realIndex++;
                }
                subJ["return"] = usedPtrIndex.at(ptrIdx);
            }
            subTrace.second = true;
        } else if (type != "LOAD") {
            ASSERT (false, "Unknown type : " << type);
        }
    }

    bool hasChanged = true;
    while (hasChanged) {
        hasChanged = false;
        for (std::pair<nlohmann::json, bool>& subTrace : allTraces) {
            nlohmann::json& subJ = subTrace.first;
            std::string type = subJ.at("type").get<std::string>();
            if (type == "LOAD" && !subTrace.second) {
                uint64_t address = subJ.at("address").get<uint64_t>();
                uint64_t value = subJ.at("value").get<uint64_t>();
                if (usedPtrIndex.count(address) && usedPtrIndex.count(value)) {
                    subJ["address"] = usedPtrIndex.at(address);
                    subJ["value"] = usedPtrIndex.at(value);
                    subTrace.second = true;
                    hasChanged = true;
                }
            } else if (type != "LOAD" && type != "CALL") {
                ASSERT (false, "Unknown type : " << type);
            }
        }
    }

    nlohmann::json ret = nlohmann::json::array();
    for (std::pair<nlohmann::json, bool>& subTrace : allTraces) {
        if (subTrace.second) {
            ret.push_back(subTrace.first);
        }
    }
    return ret;
}

} // namspace (anonymous)

extern "C" {

void* malloc (size_t size) {
    GET_SYSTEM_FUNCTION(malloc);
    void* ret = sys_malloc(size);
    if (ifTrack) {
        ifTrack = false;
        // allTraces.push_back({{{"type", "ALLOC"}, {"index", bufferIdx}, {"size", size}}, false});
        buffers[ret] = {size, bufferIdx++};
        ifTrack = true;
    }
    return ret;
}

void free (void* ptr) {
    GET_SYSTEM_FUNCTION(free);
    if (ifTrack) {
        ifTrack = false;
        buffers.erase(ptr);
        ifTrack = true;
    }
    sys_free(ptr);
}

int open (const char *path, int flags, ...) {
    GET_SYSTEM_FUNCTION (open);
    int ret;
    if (__OPEN_NEEDS_MODE (flags)) {
        va_list list;
        va_start(list, flags);
        int mode = va_arg(list, int);
        va_end(list);
        ret = sys_open(path, flags, mode);
    } else {
        ret = sys_open(path, flags);
    }
    if (ifTrack) {
        ifTrack = false;
        if (!currentFunction.empty()) {
            fileNames.emplace(path);
        }
        ifTrack = true;
    } else if (!currentFunction.empty()) {
        fileNames.emplace(path);
    }
    return ret;
}

void TDD_endCase();
void exit (int code) {
    GET_SYSTEM_FUNCTION(exit);
    if (ifTrack) {
        TDD_endCase();
    }
    sys_exit(code);
}

/*
llvm::FunctionCallee
    TDD_traceAlloca        = M.getOrInsertFunction("TDD_traceAlloca",        builder.getVoidTy(), builder.getPtrTy(), builder.getInt64Ty()),
    TDD_traceLoad          = M.getOrInsertFunction("TDD_traceLoad",          builder.getVoidTy(), builder.getPtrTy()),
    TDD_traceCallPre       = M.getOrInsertFunction("TDD_traceCallPre",       builder.getVoidTy(), builder.getPtrTy()),
    TDD_traceIntParameter  = M.getOrInsertFunction("TDD_traceIntParameter",  builder.getVoidTy(), builder.getInt64Ty(), builder.getInt64Ty()),
    TDD_tracePtrParameter  = M.getOrInsertFunction("TDD_tracePtrParameter",  builder.getVoidTy(), builder.getInt64Ty(), builder.getPtrTy()),
    TDD_traceFuncParameter = M.getOrInsertFunction("TDD_traceFuncParameter", builder.getVoidTy(), builder.getPtrTy()),s
    TDD_traceReturnValue   = M.getOrInsertFunction("TDD_traceReturnValue",   builder.getVoidTy(), builder.getPtrTy()),
    TDD_traceCallPost      = M.getOrInsertFunction("TDD_traceCallPost",      builder.getVoidTy(), builder.getPtrTy()),
    TDD_onEnter            = M.getOrInsertFunction("TDD_onEnter",            builder.getVoidTy()),
    TDD_onExit             = M.getOrInsertFunction("TDD_onExit",             builder.getVoidTy()),
    TDD_startCase          = M.getOrInsertFunction("TDD_startCase",          builder.getVoidTy()),
    TDD_endCase            = M.getOrInsertFunction("TDD_endCase",            builder.getVoidTy());
*/

void TDD_traceAlloca (const void* ptr, uint64_t size) {
    if (ifTrack) {
        ifTrack = false;
        // allTraces.push_back({{{"type", "ALLOC"}, {"index", bufferIdx}, {"size", size}}, false});
        buffers[ptr] = {size, bufferIdx++};
        buffersOnStack.push_back(ptr);
        ifTrack = true;
    }
}

void TDD_traceLoad (const void* address, const void* value) {
    if (ifTrack && buffers.count(address) && !buffers.count(value)) {
        ifTrack = false;
        allTraces.push_back({{{"type", "LOAD"}, {"address", buffers[address].second}, {"value", bufferIdx}}, false});
        buffers[value] = {0, bufferIdx++};
        ifTrack = true;
    }
}

void TDD_traceIntParameter (uint64_t parameterIndex, int64_t value) {
    if (ifTrack) {
        ifTrack = false;
        functionParameters.push_back(FunctionParameter::getInt(parameterIndex, value));
        ifTrack = true;
    }
}

void TDD_tracePtrParameter (uint64_t parameterIndex, const void* value) {
    if (ifTrack) {
        ifTrack = false;
        functionParameters.push_back(FunctionParameter::getPtr(parameterIndex, value));
        ifTrack = true;
    }
}

void TDD_traceFuncParameter (uint64_t parameterIndex, char* funcName) {
    if (ifTrack) {
        ifTrack = false;
        functionParameters.push_back(FunctionParameter::getFunc(parameterIndex, funcName));
        ifTrack = true;
    }
}

void TDD_traceCallPre (const char* demangledFunctionName) {
    if (ifTrack) {
        ifTrack = false;
        functionReturn = nullptr;
        currentFunction = getInterestingName(demangledFunctionName);
        if (currentFunction.empty()) {
            ifTrack = true;
        }
    }
}

void TDD_traceReturnValue (const void* ptr) {
    functionReturn = ptr;
}

void TDD_traceCallPost (const char* demangledFunctionName) {
    if (!currentFunction.empty() && currentFunction == getInterestingName(demangledFunctionName)) {
        nlohmann::json result = nlohmann::json::object();
        result["type"] = "CALL";
        result["name"] = currentFunction;
        if (functionReturn && !buffers.count(functionReturn)) {
            result["return"] = bufferIdx;
            buffers[functionReturn] = {0, bufferIdx++};
        }
        nlohmann::json parameterResult = nlohmann::json::array();
        for (uint64_t idx = 0; idx < functionParameters.size(); ++idx) {
            nlohmann::json thisResult = nlohmann::json::object();
            const FunctionParameter& thisParameter = functionParameters.at(idx);
            if (thisParameter.type == FunctionParameter::FunctionParameterType::INT) {
                int64_t value = thisParameter.intValue;
                if (value == -1 || value == 0 || value == 1) {
                    thisResult["idx"] = thisParameter.idx;
                    thisResult["paramType"] = "CONST";
                    thisResult["value"] = value;
                    parameterResult.emplace_back(thisResult);
                } else if (thisParameter.isPreviousSize) {
                    thisResult["idx"] = thisParameter.idx;
                    thisResult["paramType"] = "PTR_SIZE";
                    parameterResult.emplace_back(thisResult);
                }
            } else if (thisParameter.type == FunctionParameter::FunctionParameterType::PTR) {
                const void* value = thisParameter.ptrValue;
                if (!value) {
                    thisResult["idx"] = thisParameter.idx;
                    thisResult["paramType"] = "NULL";
                    parameterResult.emplace_back(thisResult);
                } else if (fileNames.count(value)) {
                    thisResult["idx"] = thisParameter.idx;
                    thisResult["paramType"] = "FILE_PATH";
                    parameterResult.emplace_back(thisResult);
                } else {
                    thisResult["idx"] = thisParameter.idx;
                    thisResult["paramType"] = "PTR";
                    thisResult["ptrIndex"] = thisParameter.ptrIdx;
                    parameterResult.emplace_back(thisResult);
                }
            } else if (thisParameter.type == FunctionParameter::FunctionParameterType::FUNC) {
                std::string funcName = thisParameter.funcValue;
                thisResult["idx"] = thisParameter.idx;
                thisResult["paramType"] = "FUNC";
                thisResult["name"] = getInterestingName(funcName, false);
                parameterResult.emplace_back(thisResult);
            } else {
                ASSERT (false, "unknown parameter type");
            }
        }
        result["parameters"] = parameterResult;
        allTraces.push_back({result, false});
    }
    currentFunction.clear();
    functionParameters.clear();
    functionReturn = nullptr;
    ifTrack = true;
}

void TDD_onEnter () {
    if (ifTrack) {
        ifTrack = false;
        buffersOnStack.push_back(nullptr);
        ifTrack = true;
    }
}

void TDD_onExit () {
    if (ifTrack) {
        ifTrack = false;
        while (true) {
            const void* ptr = buffersOnStack.back();
            buffersOnStack.pop_back();
            if (ptr) {
                buffers.erase(ptr);
            } else {
                break;
            }
        }
        ifTrack = true;
    }
}

void TDD_startCase () {
    bufferIdx = 0;
    buffers.clear();
    buffersOnStack.clear();
    fileNames.clear();
    functionParameters.clear();
    functionReturn = nullptr;
    allTraces.clear();
    ifTrack = true;
}

void TDD_endCase () {
    ifTrack = false;
    getSimplifiedTrace();
    std::ofstream os;
    os.open("__TDDCallingChain.json");
    os << getSimplifiedTrace().dump(4);
}

} // extern "C"
