#include "llvm/ADT/StringExtras.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Demangle/Demangle.h"

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <set>

#include "json.hpp"

#define ASSERT(cond, msg) if (UNLIKELY (!(cond))) {llvm::errs() << msg << " !! \n"; abort();} void(0)
#define CAST(type, name, from) type* name = llvm::cast<type>(from)
#define DYN_CAST(type, name, from) if (type* name = llvm::dyn_cast<type>(from))
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

namespace {

bool checkEnv (const char* env) {
    char* envValue = getenv(env);
    if (envValue && envValue[0] == '1') {return true;}
    else {return false;}
}

llvm::SmallString<32> getAbsolutePath(llvm::SmallString<32> fileName) {
    llvm::SmallString<32> ret("/");
    llvm::sys::fs::make_absolute(fileName);
    std::vector<llvm::StringRef> tmpVec;
    for (auto aPath : llvm::split(fileName, "/")) {
        if (aPath.equals("..")) {
            tmpVec.pop_back();
        } else if (aPath.equals(".") || aPath.empty()) {
            continue;
        } else {
            tmpVec.emplace_back(aPath);
        }
    }
    ASSERT (!tmpVec.empty(), "get absolute path for " << fileName << "failed");
    for (const auto& aPath : tmpVec) {
        llvm::sys::path::append(ret, aPath);
    }
    return ret;
}

bool isInterestingCaseFile (llvm::StringRef fileName) {
    // read in "__TDDCaseConfig"
    static bool hasRead = false;
    static std::vector<llvm::SmallString<32>> casePathVec, casePathVecNot;
    if (UNLIKELY (!hasRead)) {
        hasRead = true;
        bool atLeastOne = false;
        std::ifstream is;
        std::string line;
        // __TDDCaseConfig
        is.open("__TDDCaseConfig");
        ASSERT (is.is_open(), "file __TDDCaseConfig does NOT exist");
        bool addCase = false;
        while (std::getline(is, line)) {
            llvm::StringRef lineRef (line);
            lineRef = lineRef.trim();
            if (lineRef.empty() || lineRef.startswith("#")) {
                continue;
            } else if (lineRef.startswith("$")) {
                if (lineRef.equals("$ case")) {
                    addCase = true;
                } else {
                    addCase = false;
                }
            } else if (addCase) {
                std::vector<llvm::SmallString<32>>* thisVec = nullptr;
                if (lineRef.endswith(" 1")) {
                    lineRef = lineRef.drop_back(2);
                }
                if (lineRef.startswith("!")) {
                    lineRef = lineRef.drop_front();
                    thisVec = &casePathVecNot;
                } else {
                    thisVec = &casePathVec;
                }
                llvm::SmallString<32> absolutePath = getAbsolutePath(lineRef);
                thisVec->emplace_back(absolutePath);
                atLeastOne = true;
            }
        }
        ASSERT (atLeastOne, "__TDDCaseConfig does NOT contain any valid case record");
        is.close();
    }

    if (fileName.empty()) {
        return false;
    }

    llvm::SmallString<32> fromFull = getAbsolutePath(fileName);

    if (fromFull.startswith("/usr")) {
        return false;
    }
    for (const auto &aPath : casePathVecNot) {
        if (fromFull.startswith(aPath)) {
            return false;
        }
    }
    for (const auto &aPath : casePathVec) {
        if (fromFull.startswith(aPath)) {
            return true;
        }
    }
    return false;
}

void openAndUpdate (const char* fileName, const nlohmann::json& j) {
    nlohmann::json fullJ;
    std::ifstream inputStream;
    inputStream.open(fileName);
    if (inputStream.is_open()) {
        inputStream >> fullJ;
    } else {
        fullJ = nlohmann::json::object();
    }
    inputStream.close();

    fullJ.update(j);
    std::ofstream outputStream;
    outputStream.open(fileName);
    outputStream << fullJ.dump(4);
    outputStream.close();
}

bool shouldFunctionBeInstructed (llvm::Function& F) {
    if (!F.isDeclaration() && !F.isIntrinsic() && !F.getName().startswith("TDD_") && F.hasMetadata("dbg")) {
        DYN_CAST (llvm::DISubprogram, pDbgMeta, F.getMetadata("dbg")) {
            llvm::DISubprogram& dbgMeta = *pDbgMeta;
            llvm::SmallString<32> target;
            llvm::StringRef
                fileName = dbgMeta.getFilename(),
                dirName = dbgMeta.getDirectory();
            if (!dirName.empty()) {
                target.append(dirName);
                target.append("/");
            }
            target.append(fileName);
            llvm::SmallString<32> realPath = getAbsolutePath(target);
            return isInterestingCaseFile(realPath);
        }
    }
    return false;
}

bool shouldFunctionBeTraced (llvm::Function& F) {
    if (F.isIntrinsic()) {return false;}
    if (F.getName().startswith("TDD_")) {return false;}
    if (F.hasMetadata("dbg")) {
        DYN_CAST (llvm::DISubprogram, pDbgMeta, F.getMetadata("dbg")) {
            llvm::DISubprogram& dbgMeta = *pDbgMeta;
            llvm::SmallString<32> target;
            llvm::StringRef
                fileName = dbgMeta.getFilename(),
                dirName = dbgMeta.getDirectory();
            if (!dirName.empty()) {
                target.append(dirName);
                target.append("/");
            }
            target.append(fileName);
            llvm::SmallString<32> realPath = getAbsolutePath(target);
            if (isInterestingCaseFile(realPath)) {return false;}
        }
    }
    std::set<std::string> exceptions({
        "malloc", "calloc", "free",
        "fopen", "fclose",
        "printf", "scanf",
        "memset", "memcpy", "memmove", "memcmp", "memmem",
        "strlen", "strnlen", "strcpy", "strncpy", "strcat", "strdup", "strndup"
    });
    if (exceptions.count(F.getName().str())) {return false;}
    return true;
}

void ScanModuleAndInstruct (llvm::Module& M) {
    llvm::IRBuilder builder(M.getContext());
    llvm::FunctionCallee
        TDD_traceAlloca        = M.getOrInsertFunction("TDD_traceAlloca",        builder.getVoidTy(), builder.getPtrTy(), builder.getInt64Ty()),
        TDD_traceLoad          = M.getOrInsertFunction("TDD_traceLoad",          builder.getVoidTy(), builder.getPtrTy(), builder.getPtrTy()),
        TDD_traceCallPre       = M.getOrInsertFunction("TDD_traceCallPre",       builder.getVoidTy(), builder.getPtrTy()),
        TDD_traceIntParameter  = M.getOrInsertFunction("TDD_traceIntParameter",  builder.getVoidTy(), builder.getInt64Ty(), builder.getInt64Ty()),
        TDD_tracePtrParameter  = M.getOrInsertFunction("TDD_tracePtrParameter",  builder.getVoidTy(), builder.getInt64Ty(), builder.getPtrTy()),
        TDD_traceFuncParameter = M.getOrInsertFunction("TDD_traceFuncParameter", builder.getVoidTy(), builder.getInt64Ty(), builder.getPtrTy()),
        TDD_traceReturnValue   = M.getOrInsertFunction("TDD_traceReturnValue",   builder.getVoidTy(), builder.getPtrTy()),
        TDD_traceCallPost      = M.getOrInsertFunction("TDD_traceCallPost",      builder.getVoidTy(), builder.getPtrTy()),
        TDD_onEnter            = M.getOrInsertFunction("TDD_onEnter",            builder.getVoidTy()),
        TDD_onExit             = M.getOrInsertFunction("TDD_onExit",             builder.getVoidTy()),
        TDD_startCase          = M.getOrInsertFunction("TDD_startCase",          builder.getVoidTy()),
        TDD_endCase            = M.getOrInsertFunction("TDD_endCase",            builder.getVoidTy());

    for (llvm::Function& F : M) {
        if (shouldFunctionBeInstructed(F)) {
            // instrument : alloca, store, load, call
            for (llvm::BasicBlock& BB : F) {
                for (llvm::Instruction& I : BB) {
                    DYN_CAST (llvm::AllocaInst, pAlloca, &I) {
                        llvm::AllocaInst& alloca = *pAlloca;
                        builder.SetInsertPoint(alloca.getNextNonDebugInstruction());
                        llvm::Optional<llvm::TypeSize> size = alloca.getAllocationSizeInBits(M.getDataLayout());
                        if (size) {
                            uint64_t sizeInBytes = (size.getValue().getFixedSize() + 7) / 8;
                            builder.CreateCall(TDD_traceAlloca, {&alloca, builder.getInt64(sizeInBytes)});
                        }
                    } else DYN_CAST (llvm::LoadInst, pLoad, &I) {
                        llvm::LoadInst& load = *pLoad;
                        builder.SetInsertPoint(load.getNextNonDebugInstruction());
                        if (load.getType()->isPointerTy()) {
                            builder.CreateCall(TDD_traceLoad, {load.getPointerOperand(), &load});
                        }
                    } else DYN_CAST (llvm::CallBase, pCall, &I) {
                        llvm::CallBase& call = *pCall;
                        builder.SetInsertPoint(&call);
                        if (call.getCalledFunction()) {
                            llvm::Function& calledFunc = *call.getCalledFunction();
                            if (!shouldFunctionBeTraced(calledFunc)) {continue;}
                            std::string demangledName = llvm::demangle(calledFunc.getName().str());
                            llvm::Constant& demangledNamePtr = *builder.CreateGlobalStringPtr(demangledName);
                            for (uint64_t idx = 0; idx < call.arg_size(); ++idx) {
                                llvm::Value& arg = *call.getArgOperand(idx);
                                if (arg.getType()->isIntegerTy()) {
                                    builder.CreateCall(TDD_traceIntParameter, {builder.getInt64(idx), builder.CreateIntCast(&arg, builder.getInt64Ty(), true)});
                                } else if (arg.getType()->isPointerTy()) {
                                    DYN_CAST (llvm::Function, pFuncArg, &arg) {
                                        llvm::Function& funcArg = *pFuncArg;
                                        std::string demangledFuncArgName = llvm::demangle(funcArg.getName().str());
                                        builder.CreateCall(TDD_traceFuncParameter, {builder.getInt64(idx), builder.CreateGlobalStringPtr(demangledFuncArgName)});
                                    } else {
                                        builder.CreateCall(TDD_tracePtrParameter, {builder.getInt64(idx), &arg});
                                    }
                                }
                            }
                            builder.CreateCall(TDD_traceCallPre, {&demangledNamePtr});
                            auto traceReturnAndDoPost = [&] () {
                                if (call.getType()->isPointerTy()) {
                                    builder.CreateCall(TDD_traceReturnValue, {&call});
                                }
                                builder.CreateCall(TDD_traceCallPost, {&demangledNamePtr});
                            };
                            DYN_CAST (llvm::CallInst, pCallInst, &call) {
                                llvm::CallInst& callInst = *pCallInst;
                                builder.SetInsertPoint(call.getNextNonDebugInstruction());
                                traceReturnAndDoPost();
                            } else DYN_CAST (llvm::InvokeInst, pInvokeInst, &call) {
                                llvm::InvokeInst& invokeInst = *pInvokeInst;
                                builder.SetInsertPoint(&*invokeInst.getNormalDest()->getFirstInsertionPt());
                                traceReturnAndDoPost();
                                builder.SetInsertPoint(&*invokeInst.getUnwindDest()->getFirstInsertionPt());
                                traceReturnAndDoPost();
                            }
                        }
                    }
                }
            }
            // instrument : begin function call
            llvm::BasicBlock& entryBlock = F.getEntryBlock();
            builder.SetInsertPoint(&*entryBlock.getFirstInsertionPt());
            builder.CreateCall(TDD_onEnter);
            // instrument : end function call (return & resume)
            for (llvm::BasicBlock& exitBlock : F) {
                if (llvm::isa<llvm::ReturnInst, llvm::ResumeInst>(exitBlock.back())) {
                    builder.SetInsertPoint(&exitBlock.back());
                    builder.CreateCall(TDD_onExit);
                }
            }
        }

        // main function
        if (F.getName().equals("main") || F.getName().equals("LLVMFuzzerTestOneInput")) {
            builder.SetInsertPoint(&*F.getEntryBlock().getFirstInsertionPt());
            builder.CreateCall(TDD_startCase);
            if (F.getName().equals("LLVMFuzzerTestOneInput")) {
                builder.CreateCall(TDD_traceAlloca, {F.getArg(0), builder.CreateIntCast(F.getArg(1), builder.getInt64Ty(), true)});
            }
            for (llvm::BasicBlock& BB : F) {
                DYN_CAST (llvm::ReturnInst, pReturnInst, BB.getTerminator()) {
                    llvm::ReturnInst& returnInst = *pReturnInst;
                    builder.SetInsertPoint(&returnInst);
                    builder.CreateCall(TDD_endCase);
                } else DYN_CAST (llvm::ResumeInst, pResumeInst, BB.getTerminator()) {
                    llvm::ResumeInst& resumeInst = *pResumeInst;
                    builder.SetInsertPoint(&resumeInst);
                    builder.CreateCall(TDD_endCase);
                }
            }
        }
    }
}

struct MyPass : public llvm::PassInfoMixin<MyPass> {
    llvm::PreservedAnalyses run (llvm::Module& M, llvm::ModuleAnalysisManager& MAM) {
        if (checkEnv("TDD_CASE")) {ScanModuleAndInstruct(M);}
        return llvm::PreservedAnalyses::all();
    }
};

// part 2 : learn from test cases

} // end anonymous namespace

extern "C" LLVM_ATTRIBUTE_WEAK llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    const auto callback = [](llvm::PassBuilder &PB) {
        PB.registerPipelineStartEPCallback(
            [&] (llvm::ModulePassManager &MPM, auto) {
                MPM.addPass(MyPass());
                return true;
            }
        );
    };
    return {LLVM_PLUGIN_API_VERSION, "TDDPass", "0.0.1", callback};
}