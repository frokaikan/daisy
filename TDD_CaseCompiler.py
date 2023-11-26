import enum
import os
from pathlib import Path
import re
from typing import List, Tuple

class GlobalConfig:
    # configs
    COMPILE_FLAGS : str = "-gdwarf-4 -fstandalone-debug -O0 -DNDEBUG -Xclang -disable-O0-optnone -fPIC -std=c++17 -DNDEBUG -fpass-plugin=$TDD/TDD_NewPasses.so"
    # args
    CASES : Tuple[Tuple[str, bool]] = ()
    FLAGS : str                     = ""

    def __init__(self):
        assert False, "class GlobalConfig can NOT be initialized"

def readConfigFile(fileName = "__TDDCaseConfig"):
    if not os.path.exists(fileName):
        assert False, "__TDDCaseConfig does not exist"
    class Mode(enum.Enum):
        MODE_UNKNOWN = enum.auto()
        MODE_CASE    = enum.auto()
        MODE_FLAGS   = enum.auto()
    mode = Mode.MODE_UNKNOWN
    cases = []
    flags = []
    with open(fileName, "rt") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("# "):
                continue
            elif line == "$ case":
                mode = Mode.MODE_CASE
            elif line == "$ flags":
                mode = Mode.MODE_FLAGS
            elif line.startswith("$"):
                # other flags
                mode = Mode.MODE_UNKNOWN
            else:
                if mode == Mode.MODE_CASE:
                    line = line.split()
                    if len(line) >= 2 and line[1].startswith("1"):
                        cases.append((line[0], True))
                    else:
                        cases.append((line[0], False))
                elif mode == Mode.MODE_FLAGS:
                    flags.append(line)
                else:
                    # other configs
                    pass

    GlobalConfig.FLAGS = " ".join(flags)
    GlobalConfig.CASES = tuple(cases)

def runCommand(cmd):
    cmd = re.sub(r"\s+", " ", cmd).strip()
    print(cmd)
    return os.system(cmd)

if __name__ == "__main__":
    readConfigFile()
    for case in GlobalConfig.CASES:
        if case[1]:
            flags = f"{GlobalConfig.FLAGS} -fsanitize=fuzzer"
            output = f"{case[0]}.fuzzer.exe"
        else:
            flags = GlobalConfig.FLAGS.strip()
            output = f"{case[0]}.exe"
        cmd = f"TDD_CASE=1 $LLVM_DIR/build_release/bin/clang++ {GlobalConfig.COMPILE_FLAGS} {case[0]} -o {output} {flags} $TDD/TDD_Interceptors.so"
        assert runCommand(cmd) == 0
