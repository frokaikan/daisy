# !/usr/bin/env python3
# coding = utf-8

import enum
import json
import os
import random
import re
from   pathlib    import Path
import subprocess
from   typing     import Dict, List, Tuple

random.seed(2022)

class MyLogLevel(enum.Enum):
    LOG_NONE    = enum.auto()
    LOG_ERROR   = enum.auto()
    LOG_WARNING = enum.auto()
    LOG_INFO    = enum.auto()
    LOG_HINT    = enum.auto()
    LOG_SUMMARY = enum.auto()
    LOG_FULL    = enum.auto()

class GlobalConfig:
    # configs
    LOG_LEVEL            : MyLogLevel = MyLogLevel.LOG_HINT
    EXEC_TIMEOUT         : int        = 20
    CC_FILE              : str        = "__TDDDriver.cc"
    EXE_FILE             : str        = "__TDDDriver.exe"
    DRIVER_COMPILE_FLAGS : str        = "-gdwarf-4 -fstandalone-debug -O0 -DNDEBUG -Xclang -disable-O0-optnone -fPIC -fsanitize=address,fuzzer -fprofile-instr-generate -fcoverage-mapping -std=c++17"
    DRIVER_SKELETON      : Path       = Path() / os.environ["TDD"] / "TDD_DriverSkeleton.cc"
    # args
    PRE_OPERATIONS : str  = ""
    FLAGS          : str  = ""
    MIN_SIZE       : int  = 256
    MAX_SIZE       : int  = 4096
    NO_CONST_INT   : bool = False
    OPAQUE_TYPES   : str  = ""

    def __init__(self):
        assert False, "class GlobalConfig can NOT be initialized"

def readConfigFile(fileName = "__TDDCaseConfig"):
    if not os.path.exists(fileName):
        assert False, "__TDDCaseConfig does not exist"
    class Mode(enum.Enum):
        MODE_UNKNOWN      = enum.auto()
        MODE_PRE_OPS      = enum.auto()
        MODE_FLAGS        = enum.auto()
        MODE_MIN_SIZE     = enum.auto()
        MODE_MAX_SIZE     = enum.auto()
        MODE_NO_CONST_INT = enum.auto()
        MODE_OPAQUE_TYPES = enum.auto()
    mode = Mode.MODE_UNKNOWN
    preOperations = []
    flags = []
    minSize = 256
    maxSize = 4096
    noConstInt = False
    opaqueTypes = ""
    with open(fileName, "rt") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("# "):
                continue
            elif line == "$ pre operations":
                mode = Mode.MODE_PRE_OPS
            elif line == "$ flags":
                mode = Mode.MODE_FLAGS
            elif line == "$ min size":
                mode = Mode.MODE_MIN_SIZE
            elif line == "$ max size":
                mode = Mode.MODE_MAX_SIZE
            elif line == "$ no const int":
                mode = Mode.MODE_NO_CONST_INT
            elif line == "$ opaque types":
                mode = Mode.MODE_OPAQUE_TYPES
            elif line.startswith("$"):
                # other flags
                mode = Mode.MODE_UNKNOWN
            else:
                if mode == Mode.MODE_PRE_OPS:
                    preOperations.append(line)
                elif mode == Mode.MODE_FLAGS:
                    flags.append(line)
                elif mode == Mode.MODE_MIN_SIZE:
                    minSize = int(line)
                elif mode == Mode.MODE_MAX_SIZE:
                    maxSize = int(line)
                elif mode == Mode.MODE_NO_CONST_INT:
                    if line.startswith("1"):
                        noConstInt = True
                    else:
                        noConstInt = False
                elif mode == Mode.MODE_OPAQUE_TYPES:
                    opaqueTypes += f"struct {line} {{}};\n"
                else:
                    # other configs
                    pass

    assert minSize <= maxSize, f"minSize : {minSize} > maxSize : {maxSize}"
    GlobalConfig.PRE_OPERATIONS = "\n".join(preOperations)
    GlobalConfig.FLAGS = " ".join(flags)
    GlobalConfig.MIN_SIZE = minSize
    GlobalConfig.MAX_SIZE = maxSize
    GlobalConfig.NO_CONST_INT = noConstInt
    GlobalConfig.OPAQUE_TYPES = opaqueTypes

class CMDFormat:
    """
    coloring the command line.
    """
    __slots__ = ()

    def __init__(self):
        assert False, f"class CMDFormat can NOT be instantialized"

    @staticmethod
    def clear():
        print("\x1b[0m", end = "")

    @staticmethod
    def red():
        print("\x1b[31m", end = "")

    @staticmethod
    def green():
        print("\x1b[32m", end = "")

    @staticmethod
    def yellow():
        print("\x1b[33m", end = "")

    @staticmethod
    def blue():
        print("\x1b[34m", end = "")

    @staticmethod
    def magenta():
        print("\x1b[35m", end = "")

    @staticmethod
    def cyan():
        print("\x1b[36m", end = "")

    @staticmethod
    def colorit(colorFunc, msg : str):
        colorFunc()
        print(msg, end = "")
        CMDFormat.clear()
        print()

    @staticmethod
    def error(msg : str):
        if GlobalConfig.LOG_LEVEL.value >= MyLogLevel.LOG_ERROR.value:
            CMDFormat.red()
            print(f"[ERROR] ", end = "")
            CMDFormat.clear()
            print(msg)

    @staticmethod
    def warning(msg : str):
        if GlobalConfig.LOG_LEVEL.value >= MyLogLevel.LOG_WARNING.value:
            CMDFormat.yellow()
            print(f"[WARNING] ", end = "")
            CMDFormat.clear()
            print(msg)

    @staticmethod
    def info(msg : str):
        if GlobalConfig.LOG_LEVEL.value >= MyLogLevel.LOG_INFO.value:
            CMDFormat.blue()
            print(f"[INFO] ", end = "")
            CMDFormat.clear()
            print(msg)

    @staticmethod
    def hint(msg : str):
        if GlobalConfig.LOG_LEVEL.value >= MyLogLevel.LOG_HINT.value:
            CMDFormat.cyan()
            print(f"[HINT] ", end = "")
            CMDFormat.clear()
            print(msg)

    @staticmethod
    def runCommand(cmd : str, exitOnFail = False) -> int:
        """
        return the cmd's exit code (or -1 means timeout).
        """
        cmd = re.sub(r"\s+", " ", cmd.strip())
        if GlobalConfig.LOG_LEVEL.value >= MyLogLevel.LOG_SUMMARY.value:
            CMDFormat.colorit(CMDFormat.cyan, ">>>>>>>>>>>>>>>>>>>> Run Summary >>>>>>>>>>>>>>>>>>>>")
            CMDFormat.colorit(CMDFormat.blue, "[command] >>>>>")
            print(cmd)
            CMDFormat.colorit(CMDFormat.blue, "<<<<< [command]")
        proc = subprocess.Popen(cmd, shell = True, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
        programTimeout = False
        programError = False
        try:
            proc.wait(GlobalConfig.EXEC_TIMEOUT)
        except subprocess.TimeoutExpired:
            if GlobalConfig.LOG_LEVEL.value >= MyLogLevel.LOG_SUMMARY.value:
                CMDFormat.colorit(CMDFormat.red, f"Run Fail : Timeout in {GlobalConfig.EXEC_TIMEOUT} seconds")
            programTimeout = True
        else:
            if GlobalConfig.LOG_LEVEL.value >= MyLogLevel.LOG_SUMMARY.value:
                CMDFormat.colorit(CMDFormat.yellow, f"[stdout] >>>>>")
                print(proc.stdout.read().decode("utf-8", errors = "replace").strip())
                CMDFormat.colorit(CMDFormat.yellow, f"<<<<< [stdout]")
                CMDFormat.colorit(CMDFormat.red, "[stderr] >>>>>")
                print(proc.stderr.read().decode("utf-8", errors = "replace").strip())
                CMDFormat.colorit(CMDFormat.red, "<<<<< [stderr]")
                if proc.returncode == 0:
                    CMDFormat.colorit(CMDFormat.green, "Run Success")
                else:
                    CMDFormat.colorit(CMDFormat.red, f"Run Fail : return {proc.returncode}")
            programError = proc.returncode != 0
        finally:
            if GlobalConfig.LOG_LEVEL.value >= MyLogLevel.LOG_SUMMARY.value:
                CMDFormat.colorit(CMDFormat.cyan, "<<<<<<<<<<<<<<<<<<<< Run Summary <<<<<<<<<<<<<<<<<<<<")
        if programTimeout:
            if exitOnFail:
                assert False, f"command [ {cmd} ] timeout in {GlobalConfig.EXEC_TIMEOUT} seconds"
            else:
                return -1
        elif programError:
            if exitOnFail:
                assert False, f"command [ {cmd} ] exits with {proc.returncode}"
            else:
                return proc.returncode
        else:
            return 0

class FunctionDecl:
    class FunctionDeclType(enum.Enum):
        NORMAL_OR_STATIC = enum.auto()
        CXX_CONSTRUCTOR  = enum.auto()
        CXX_METHOD       = enum.auto()

    __slots__ = ("_name", "_returnType", "_parametersType", "_isCXXMethod", "_base", "_isStaticMethod", "_isCXXConstructor", "_isTemplate", "_templateArgs")
    _name : str
    _returnType : str
    _parametersType : Tuple[str, ...]
    _isCXXMethod : int
    _base : str
    _isStaticMethod : int
    _isCXXConstructor : int

    def __init__(self, name : str, d: Dict):
        self._parametersType = tuple(s.replace(" ", "") for s in d["parametersType"])
        self._returnType = d["returnType"].replace(" ", "")

        self._isCXXMethod = d["isCXXMethod"]
        if self._isCXXMethod:
            self._isStaticMethod = d["isStaticMethod"]
            if self._isStaticMethod:
                self._name = name
            else:
                lastNameSpacePos = name.rfind("::")
                assert lastNameSpacePos != -1
                self._name = name[lastNameSpacePos + 2 :]
                self._base = name[: lastNameSpacePos]
                self._isCXXConstructor = d["isCXXConstructor"]
        else:
            self._name = name

    @property
    def returnType(self) -> str:
        return self._returnType

    @property
    def parametersType(self) -> Tuple[str, ...]:
        return self._parametersType

    @property
    def isCXXMethod(self) -> int:
        return self._isCXXMethod

    @property
    def base(self) -> str:
        return self._base

    @property
    def isStaticMethod(self) -> int:
        assert self._isCXXMethod == 1
        return self._isStaticMethod

    @property
    def isCXXConstructor(self) -> int:
        assert self._isCXXMethod == 1
        return self._isCXXConstructor

    @property
    def functionType(self) -> FunctionDeclType:
        if not self._isCXXMethod:
            return FunctionDecl.FunctionDeclType.NORMAL_OR_STATIC
        elif self._isStaticMethod:
            return FunctionDecl.FunctionDeclType.NORMAL_OR_STATIC
        elif self._isCXXConstructor:
            return FunctionDecl.FunctionDeclType.CXX_CONSTRUCTOR
        else:
            return FunctionDecl.FunctionDeclType.CXX_METHOD

    def __str__(self) -> str:
        return self.identifiedName

    @property
    def identifiedName(self) -> str:
        ret = ""
        if self._isCXXMethod and not self._isStaticMethod:
            ret += f"{self._base}->"
        ret += self._name
        ret += "("
        for declName in self._parametersType:
            ret += f"{declName.classNameWithPtrRef},"
        ret += ")"
        return ret

class FunctionCall:
    class FunctionParameterType(enum.Enum):
        CONST     = enum.auto()
        PTR_SIZE  = enum.auto()
        NULL      = enum.auto()
        FILE_PATH = enum.auto()
        PTR       = enum.auto()
        FUNC      = enum.auto()

    class FunctionParameter:
        __slots__ = ("_idx", "_paramType", "_value", "_ptrIndex", "_offset", "_name")
        _idx : int
        _paramType : "FunctionCall.FunctionParameterType"
        _value : int
        _ptrIndex : int
        _offset : int
        _name : str

        def __init__(self, parameterDict : Dict):
            self._idx = parameterDict["idx"]
            self._paramType = FunctionCall.FunctionParameterType.__members__[parameterDict["paramType"]]
            if self._paramType == FunctionCall.FunctionParameterType.CONST:
                self._value = parameterDict["value"]
            elif self._paramType == FunctionCall.FunctionParameterType.PTR:
                self._ptrIndex = parameterDict["ptrIndex"]
                self._offset = parameterDict["offset"]
            elif self._paramType == FunctionCall.FunctionParameterType.FUNC:
                self._name = parameterDict["name"]

        @property
        def idx(self) -> int:
            return self._idx

        @property
        def paramType(self) -> "FunctionCall.FunctionParameterType":
            return self._paramType

        @property
        def value(self) -> int:
            return self._value

        @property
        def ptrIndex(self) -> int:
            return self._ptrIndex

        @property
        def offset(self) -> int:
            return self._offset

        @property
        def name(self) -> str:
            return self._name

    __slots__ = ("_name", "_returnIdx", "_parameters")
    _name : str
    _returnIdx : int
    _parameters : List[FunctionParameter]

    def __init__(self, funcDict : Dict):
        self._name = funcDict["name"]
        if "return" in funcDict:
            self._returnIdx = funcDict["return"]
        else:
            self._returnIdx = -1
        self._parameters = []
        for parameter in funcDict["parameters"]:
            parameter : Dict
            self._parameters.append(FunctionCall.FunctionParameter(parameter))

    @property
    def name(self) -> str:
        return self._name

    @property
    def returnIdx(self) -> int:
        return self._returnIdx

    @property
    def parameters(self) -> List[FunctionParameter]:
        return self._parameters

if __name__ == "__main__":
    readConfigFile()
    decls : Dict[str, FunctionDecl] = {}
    with open("__TDDFinalCallingChain.json", "rt") as f:
        ous = json.load(f)
    with open("__TDDDeclarations.json", "rt") as f:
        for name, others in json.load(f).items():
            decls[name] = FunctionDecl(name, others)

    with open(str(GlobalConfig.DRIVER_SKELETON), "rt") as f:
        skeleton = f.read()
    skeleton = skeleton.replace("// @@ DRIVER COMPILE COMMAND @@", f"$LLVM_DIR/build_release/bin/clang++ {GlobalConfig.DRIVER_COMPILE_FLAGS} -o {GlobalConfig.EXE_FILE} {GlobalConfig.CC_FILE} {GlobalConfig.FLAGS}")
    skeleton = skeleton.replace("// @@ MIN SIZE @@", f"{GlobalConfig.MAX_SIZE}").replace("// @@ MAX SIZE @@", f"{GlobalConfig.MAX_SIZE}")
    skeleton = skeleton.replace("// @@ PRE OPERATIONS @@", f"{GlobalConfig.PRE_OPERATIONS}")
    skeleton = skeleton.replace("// @@ OPAQUE TYPES @@", f"{GlobalConfig.OPAQUE_TYPES}")
    skeleton = skeleton.replace("/* @@ GRAPH NODE COUNT @@ */", f"{len(ous['nodes'])}")
    skeleton = skeleton.replace("/* @@ POINTER COUNT @@ */", f"{ous['pointerIdxCount']}")

    ptrLoads : List[str] = []
    graphEdges : List[str] = []
    graphNodes : List[str] = []

    def addPtrLoadInDriver(loadPair : Tuple[Tuple[int, int], int]):
        return f"    __TDD_load_ptr[{loadPair[1]}] = {{{loadPair[0][0]}, {loadPair[0][1]}}};"
    for loadPair in ous["loadPtrs"]:
        ptrLoads.append(addPtrLoadInDriver(loadPair))
    ptrLoadsStr = "\n".join(ptrLoads)
    skeleton = skeleton.replace("// @@ PTR LOADS @@", ptrLoadsStr)

    def addEdgeInDriver(edge : Tuple[int, int]) -> str:
        return f"    __TDD_graph.addEdge({edge[0]}, {edge[1]});"
    for edge in ous["edges"]:
        graphEdges.append(addEdgeInDriver(edge))
    graphEdgesStr = "\n".join(graphEdges)
    skeleton = skeleton.replace("// @@ GRAPH EDGES @@", graphEdgesStr)

    commands : List[str] = []
    tempVarIdx = 0
    lastPointerIndex = -1
    args : List[int] = []
    def addArgument(argument : FunctionCall.FunctionParameter, parameterType : str):
        global commands, tempVarIdx, lastPointerIndex
        if parameterType == "FILE*":
            commands.append(f"{parameterType} __TDD_tempVar_{tempVarIdx} = ({parameterType}) (__TDD_driver_FILEptr());")
            args.append(tempVarIdx)
            tempVarIdx += 1
        elif parameterType == "std::string":
            commands.append(f"{parameterType} __TDD_tempVar_{tempVarIdx} = ({parameterType}) (__TDD_driver_std_string());")
            args.append(tempVarIdx)
            tempVarIdx += 1
        elif argument.paramType == FunctionCall.FunctionParameterType.CONST:
            if GlobalConfig.NO_CONST_INT:
                commands.append(f"{parameterType} __TDD_tempVar_{tempVarIdx} = ({parameterType}) (__TDD_driver_get_typed_value<{parameterType}>());")
            else:
                commands.append(f"{parameterType} __TDD_tempVar_{tempVarIdx} = ({parameterType}) ({argument.value});")
            args.append(tempVarIdx)
            tempVarIdx += 1
        elif argument.paramType == FunctionCall.FunctionParameterType.FILE_PATH:
            commands.append(f"{parameterType} __TDD_tempVar_{tempVarIdx} = ({parameterType}) (__TDD_driver_file_name());")
            args.append(tempVarIdx)
            tempVarIdx += 1
        elif argument.paramType == FunctionCall.FunctionParameterType.FUNC:
            commands.append(f"{parameterType} __TDD_tempVar_{tempVarIdx} = ({parameterType}) ({argument.name});")
            args.append(tempVarIdx)
            tempVarIdx += 1
        elif argument.paramType == FunctionCall.FunctionParameterType.NULL:
            commands.append(f"{parameterType} __TDD_tempVar_{tempVarIdx} = ({parameterType}) (nullptr);")
            args.append(tempVarIdx)
            tempVarIdx += 1
        elif argument.paramType == FunctionCall.FunctionParameterType.PTR:
            commands.append(f"if (!__TDD_driver_get_ptr<{parameterType}>({argument.ptrIndex})) return false;");
            commands.append(f"{parameterType} __TDD_tempVar_{tempVarIdx} = ({parameterType}) (__TDD_driver_get_typed_object<{parameterType}>({argument.ptrIndex}, {argument.offset}));")
            args.append(tempVarIdx)
            tempVarIdx += 1
            lastPointerIndex = argument.ptrIndex
        elif argument.paramType == FunctionCall.FunctionParameterType.PTR_SIZE:
            assert lastPointerIndex != -1
            commands.append(f"{parameterType} __TDD_tempVar_{tempVarIdx} = ({parameterType}) (__TDD_ptr_size.at({lastPointerIndex}));")
            args.append(tempVarIdx)
            tempVarIdx += 1
        else:
            assert False, f"Unknown parameter type : {argument.paramType}"
    def addValueArgument(parameterType : str):
        global commands, tempVarIdx, lastPointerIndex
        commands.append(f"{parameterType} __TDD_tempVar_{tempVarIdx} = ({parameterType}) (__TDD_driver_get_typed_value<{parameterType}>());")
        args.append(tempVarIdx)
        tempVarIdx += 1
    def generateArguments(functionCall : FunctionCall, functionDecl : FunctionDecl, skipFirst : bool):
        arguments : List[FunctionCall.FunctionParameter] = functionCall.parameters
        currentArgIdx = 0
        if skipFirst:
            currentArgIdx = 1
        for idx, parameter in enumerate(functionDecl.parametersType):
            if currentArgIdx < len(arguments) and arguments[currentArgIdx].idx == idx:
                addArgument(arguments[currentArgIdx], parameter)
                currentArgIdx += 1
            else:
                addValueArgument(parameter)
    def getArgList() -> str:
        global args
        subList = ", ".join(f"__TDD_tempVar_{idx}" for idx in args)
        return f"({subList})"

    for node in ous["nodes"][1:]:
        commands.clear()
        tempVarIdx = 0
        lastPointerIndex = -1
        args.clear()

        functionCall = FunctionCall(node)
        functionDecl = decls[functionCall.name]
        generateArguments(functionCall, functionDecl, functionDecl.functionType != FunctionDecl.FunctionDeclType.NORMAL_OR_STATIC)
        if functionDecl.functionType == FunctionDecl.FunctionDeclType.NORMAL_OR_STATIC:
            if functionCall.returnIdx == -1:
                commands.append(f"{functionCall._name}{getArgList()};")
            else:
                commands.append(f"{functionDecl.returnType} __TDD_tempVar_{tempVarIdx} = ({functionDecl.returnType}) {functionCall._name}{getArgList()};")
                tempVarIdx += 1
        elif functionDecl.functionType == FunctionDecl.FunctionDeclType.CXX_METHOD:
            commands.append(f"if (!__TDD_driver_get_ptr<{functionDecl.base}*>({functionCall.parameters[0].ptrIndex})) return false;")
            commands.append(f"{functionDecl.base}* __TDD_tempVar_{tempVarIdx} = ({functionDecl.base}*) (__TDD_driver_get_typed_object<{functionDecl.base}*>({functionCall.parameters[0].ptrIndex}, {functionCall.parameters[0].offset}));")
            if functionCall.returnIdx == -1:
                commands.append(f"__TDD_tempVar_{tempVarIdx}->{functionCall._name}{getArgList()};")
            else:
                commands.append(f"{functionDecl.returnType} __TDD_tempVar_{tempVarIdx} = ({functionDecl.returnType}) __TDD_tempVar_{tempVarIdx}->{functionCall._name}{getArgList()};")
                tempVarIdx += 1
            tempVarIdx += 1
        elif functionDecl.functionType == FunctionDecl.FunctionDeclType.CXX_CONSTRUCTOR:
            assert functionCall.returnIdx == -1
            commands.append(f"if (!__TDD_driver_get_ptr<{functionDecl.base}*>({functionCall.parameters[0].ptrIndex})) return false;")
            commands.append(f"{functionDecl.base}* __TDD_tempVar_{tempVarIdx} = ({functionDecl.base}*) (__TDD_driver_get_typed_object<{functionDecl.base}*>({functionCall.parameters[0].ptrIndex}, {functionCall.parameters[0].offset}));")
            commands.append(f"new (__TDD_tempVar_{tempVarIdx}) {functionDecl.base}{getArgList()};")
            tempVarIdx += 1
        else:
            assert False, f"Unknown functionDeclType : {functionDecl.FunctionDeclType}"
        if functionCall.returnIdx != -1:
            commands.append(f"if (!__TDD_tempVar_{tempVarIdx - 1}) return false;")
            commands.append(f"__TDD_driver_set_ptr<{functionDecl.returnType}>({functionCall.returnIdx}, __TDD_tempVar_{tempVarIdx - 1});")

        fullCommand = "\n".join(f"        {subCommand}" for subCommand in commands)
        graphNodes.append(f"    nodes.push_back([&] () -> bool {{\n{fullCommand}\n        return true;\n    }});")

    graphNodesStr = "\n".join(graphNodes)
    skeleton = skeleton.replace("// @@ NODES @@", graphNodesStr)

    with open(GlobalConfig.CC_FILE, "wt") as f:
        f.write(skeleton)