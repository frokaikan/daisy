#include "clang/AST/AST.h"
#include "clang/AST/QualTypeNames.h"
#include "clang/AST/Type.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Sema/Sema.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>

#include "json.hpp"

#define ASSERT(cond, msg) if (!(cond)) {llvm::errs() << msg << " !! \n"; abort();} void(0)
#define CAST(type, name, from) type *name = llvm::cast<type>(from)
#define DYN_CAST(type, name, from) if (type *name = llvm::dyn_cast<type>(from))
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

namespace {

bool checkEnv (const char* env) {
    const char* envValue = getenv(env);
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

bool isInterestingFile (llvm::StringRef fileName) {
    // read in "__TDDCaseConfig"
    static bool hasRead = false;
    static std::vector<llvm::SmallString<32>> pathVec, pathVecNot;
    if (UNLIKELY (!hasRead)) {
        hasRead = true;
        bool atLeastOne = false;
        std::ifstream is;
        std::string line;
        is.open("__TDDCaseConfig");
        ASSERT (is.is_open(), "file __TDDCaseConfig does NOT exist");
        bool addTarget = false;
        while (std::getline(is, line)) {
            llvm::StringRef lineRef (line);
            lineRef = lineRef.trim();
            if (lineRef.empty() || lineRef.startswith("#")) {
                continue;
            } else if (lineRef.startswith("$")) {
                if (lineRef.equals("$ target")) {
                    addTarget = true;
                } else {
                    addTarget = false;
                }
            } else if (addTarget) {
                std::vector<llvm::SmallString<32>>* thisVec = nullptr;
                if (lineRef.startswith("!")) {
                    lineRef = lineRef.drop_front();
                    thisVec = &pathVecNot;
                } else {
                    thisVec = &pathVec;
                }
                llvm::SmallString<32> absolutePath = getAbsolutePath(lineRef);
                thisVec->emplace_back(absolutePath);
                atLeastOne = true;
            }
        }
        ASSERT (atLeastOne, "__TDDCaseConfig does NOT contain any valid record");
        is.close();
    }

    if (fileName.empty()) {
        return false;
    }

    llvm::SmallString<32> fromFull = getAbsolutePath(fileName);

    if (fromFull.startswith("/usr")) {
        return false;
    }
    for (const auto &aPath : pathVecNot) {
        if (fromFull.startswith(aPath)) {
            return false;
        }
    }
    for (const auto &aPath : pathVec) {
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

std::string nameSimplify (std::string name) {
    while (true) {
        if (name.find("struct ") == 0) {
            name = name.substr(7);
        } else if (name.find("enum ") == 0) {
            name = name.substr(5);
        } else if (name.find("class ") == 0) {
            name = name.substr(6);
        } else if (name.find("const ") == 0) {
            name = name.substr(6);
        } else if (name.find("volatile" ) == 0) {
            name = name.substr(9);
        } else {
            break;
        }
    }
    return name;
}

std::string getFullTypeName (clang::QualType type, clang::ASTContext& ctx) {
    return nameSimplify(clang::TypeName::getFullyQualifiedName(type, ctx, ctx.getPrintingPolicy()));
}

bool isInterestingDecl (clang::Decl& decl, clang::ASTContext& ctx) {
    clang::SourceManager& manager = ctx.getSourceManager();
    clang::FullSourceLoc beginLoc = ctx.getFullLoc(decl.getBeginLoc()), endLoc = ctx.getFullLoc(decl.getEndLoc());
    auto checkLocInteresting = [&manager] (clang::FullSourceLoc &loc) -> bool {
        if (!loc.isValid() || !loc.hasManager()) {
            return false;
        }
        llvm::StringRef fileName;
        if (loc.isFileID()) {
            fileName = manager.getFilename(loc);
        } else if (loc.isMacroID()) {
            fileName = manager.getFilename(manager.getFileLoc(loc));
        } else {
            return false;
        }
        return isInterestingFile(fileName);
    };
    return checkLocInteresting(beginLoc) && checkLocInteresting(endLoc);
}

int32_t check (clang::Decl& decl, clang::ASTContext& ctx) {
    if (!isInterestingDecl(decl, ctx)) {return 0;}

    DYN_CAST (clang::NamedDecl, pNamedDecl, &decl) {
        clang::NamedDecl& namedDecl = *pNamedDecl;
        if (namedDecl.getIdentifier() && namedDecl.getName().size() == 0) {
            return 0;
        }
    }

    DYN_CAST (clang::FunctionDecl, pFunctionDecl, &decl) {
        clang::FunctionDecl& functionDecl = *pFunctionDecl;
        if (
            functionDecl.getTemplatedKind() != clang::FunctionDecl::TemplatedKind::TK_NonTemplate
        ) {
            return 0;
        } else if (llvm::isa<clang::CXXDeductionGuideDecl>(decl)) {
            return 0;
        } else DYN_CAST (clang::CXXMethodDecl, pI_CXXMethodDecl, &decl) {
            clang::CXXMethodDecl& i_CXXMethodDecl = *pI_CXXMethodDecl;
            if (llvm::isa<clang::CXXDestructorDecl>(&decl)) {
                return 0;
            } else if (llvm::isa<clang::CXXConversionDecl>(&decl)) {
                return 0;
            } else if (i_CXXMethodDecl.isOverloadedOperator()) {
                return 0;
            } else DYN_CAST (clang::CXXConstructorDecl, pI_CXXConstructorDecl, &decl) {
                clang::CXXConstructorDecl& i_CXXConstructorDecl = *pI_CXXConstructorDecl;
                if (i_CXXConstructorDecl.isCopyOrMoveConstructor()) {
                    return 0;
                }
            } else {
                if (i_CXXMethodDecl.isCopyAssignmentOperator() || i_CXXMethodDecl.isMoveAssignmentOperator()) {
                    return 0;
                }
                if (i_CXXMethodDecl.getParent()->isLambda()) {
                    return 0;
                }
            }
        } else {
            if (functionDecl.isStatic()) {
                return 1;
            }
        }
    }

    if (decl.isInAnonymousNamespace()) {return 1;}

    if (!decl.isUnconditionallyVisible()) {return 1;}

    clang::AccessSpecifier accessSpecifier = decl.getAccess();
    if (accessSpecifier == clang::AccessSpecifier::AS_protected || accessSpecifier == clang::AccessSpecifier::AS_private) {
        return 1;
    }

    return 2;
}

// visitor

class DeclVisitor : public clang::RecursiveASTVisitor<DeclVisitor> {
private:
    clang::ASTContext& ctx;
    nlohmann::json info;
public:
    explicit DeclVisitor (clang::ASTContext& ctx_) : ctx(ctx_), info(nlohmann::json::object()) {}

    bool VisitFunctionDecl (clang::FunctionDecl* functionDecl) {
        if (check(*functionDecl, this->ctx) != 2) {return true;}

        nlohmann::json thisFunction = nlohmann::json::object();
        thisFunction["returnType"] = getFullTypeName(functionDecl->getReturnType(), ctx);
        nlohmann::json parametersType = nlohmann::json::array();
        for (const clang::ParmVarDecl *parameter : functionDecl->parameters()) {
            std::string parameterType = getFullTypeName(parameter->getOriginalType(), ctx);
            parametersType.emplace_back(parameterType);
        }
        thisFunction["parametersType"] = parametersType;
        thisFunction["isCXXMethod"] = 0;
        DYN_CAST (clang::CXXMethodDecl, i_CXXMethodDecl, functionDecl) {
            thisFunction["isCXXMethod"] = 1;
            thisFunction["isStaticMethod"] = 0;
            thisFunction["isCXXConstructor"] = 0;
            if (i_CXXMethodDecl->isStatic()) {
                thisFunction["isStaticMethod"] = 1;
            }
            if (llvm::isa<clang::CXXConstructorDecl>(functionDecl)) {
                thisFunction["isCXXConstructor"] = 1;
            }
        }

        std::string functionName = functionDecl->getQualifiedNameAsString();
        this->info[functionName] = thisFunction;

        return true;
    }

    nlohmann::json getInfo () const {
        return info;
    }
};

class DepVisitor : public clang::RecursiveASTVisitor<DepVisitor> {
private:
    clang::ASTContext& ctx;
    std::string currentFunction;
    std::map<std::string, std::set<std::string>> calledFunctions;

public:
    explicit DepVisitor (clang::ASTContext& ctx_) : ctx(ctx_), currentFunction(), calledFunctions() {}

    bool VisitFunctionDecl (clang::FunctionDecl* functionDecl) {
        this->currentFunction.clear();
        if (check(*functionDecl, this->ctx) == 0) {return true;}
        if (!functionDecl->hasBody()) {return true;}

        this->currentFunction = functionDecl->getQualifiedNameAsString();
        this->calledFunctions[this->currentFunction] = {};
        return true;
    }

    bool VisitCallExpr (clang::CallExpr* callExpr) {
        clang::FunctionDecl* callee = callExpr->getDirectCallee();
        if (!callee) {return true;}
        if (!callee->hasBody()) {return true;}
        if (check(*callee, ctx) == 0) {return true;}
        if (this->currentFunction.empty()) {return true;}

        this->calledFunctions[this->currentFunction].emplace(callee->getQualifiedNameAsString());
        return true;
    }

    nlohmann::json getInfo () const {
        return this->calledFunctions;
    }
};

class TDDConsumer : public clang::ASTConsumer {
public:
    void HandleTranslationUnit (clang::ASTContext &ctx) override {
        if (checkEnv("TDD_DUMP_DECL") && isInterestingFile(ctx.getSourceManager().getFileEntryForID(ctx.getSourceManager().getMainFileID())->getName())) {
            clang::TranslationUnitDecl& unit = *ctx.getTranslationUnitDecl();
            DeclVisitor v(ctx);
            v.TraverseDecl(&unit);
            openAndUpdate("__TDDDeclarations.json", v.getInfo());
        }
        if (checkEnv("TDD_GET_DEP") && isInterestingFile(ctx.getSourceManager().getFileEntryForID(ctx.getSourceManager().getMainFileID())->getName())) {
            clang::TranslationUnitDecl& unit = *ctx.getTranslationUnitDecl();
            DepVisitor v(ctx);
            v.TraverseDecl(&unit);
            openAndUpdate("__TDDAnalysis.json", v.getInfo());
        }
    }
};

class TDDAction : public clang::PluginASTAction {
public:
    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef _unused) override {
        return std::make_unique<TDDConsumer>();
    }

    bool ParseArgs(const clang::CompilerInstance &CI, const std::vector<std::string> &args) override {
        return true;
    }

    clang::PluginASTAction::ActionType getActionType () override {
        return clang::PluginASTAction::ActionType::AddAfterMainAction;
    }
};

} // anonymous namespace

static clang::FrontendPluginRegistry::Add<TDDAction> _TDDAction("TDDActions", "TDD_DUMP_DECL for decl and TDD_GET_DEP for dep");

extern "C" int _ZTIN5clang15PluginASTActionE = 0;
extern "C" int _ZTIN5clang11ASTConsumerE = 0;