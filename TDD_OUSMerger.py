from collections import defaultdict
from copy import deepcopy
import enum
import json
import os
import sys
from typing import DefaultDict, Dict, List, Set, Tuple, Union

class ObjectLoad:
    __slots__ = ("_address", "_value")
    _address : int
    _value : int

    def __init__(self, loadDict : Dict):
        assert loadDict["type"] == "LOAD"
        self._address = loadDict["address"]
        self._value = loadDict["value"]

    def dump(self) -> Dict:
        return {
            "type" : "LOAD",
            "address" : self._address,
            "value" : self._value
        }

class FunctionCall:
    class FunctionParameterType(enum.Enum):
        CONST     = enum.auto()
        PTR_SIZE  = enum.auto()
        NULL      = enum.auto()
        FILE_PATH = enum.auto()
        PTR       = enum.auto()
        FUNC      = enum.auto()

    class FunctionParameter:
        __slots__ = ("_idx", "_paramType", "_value", "_ptrIndex", "_name")
        _idx : int
        _paramType : "FunctionCall.FunctionParameterType"
        _value : int
        _ptrIndex : int
        _name : str

        def __init__(self, parameterDict : Dict):
            self._idx = parameterDict["idx"]
            self._paramType = FunctionCall.FunctionParameterType.__members__[parameterDict["paramType"]]
            if self._paramType == FunctionCall.FunctionParameterType.CONST:
                self._value = parameterDict["value"]
            elif self._paramType == FunctionCall.FunctionParameterType.PTR:
                self._ptrIndex = parameterDict["ptrIndex"]
            elif self._paramType == FunctionCall.FunctionParameterType.FUNC:
                self._name = parameterDict["name"]

        def dump(self) -> Dict:
            dic = {
                "idx" : self._idx,
                "paramType" : self._paramType.name
            }
            if self._paramType == FunctionCall.FunctionParameterType.CONST:
                dic["value"] = self._value
            elif self._paramType == FunctionCall.FunctionParameterType.PTR:
                dic["ptrIndex"] = self._ptrIndex
            elif self._paramType == FunctionCall.FunctionParameterType.FUNC:
                dic["name"] = self._name
            return dic

    __slots__ = ("_name", "_return", "_parameters")
    _name : str
    _return : int
    _parameters : List[FunctionParameter]

    def __init__(self, funcDict : Dict):
        assert funcDict["type"] == "CALL"
        self._name = funcDict["name"]
        if "return" in funcDict:
            self._return = funcDict["return"]
        else:
            self._return = -1
        self._parameters = []
        for parameter in funcDict["parameters"]:
            parameter : Dict
            self._parameters.append(FunctionCall.FunctionParameter(parameter))

    def dump(self) -> Dict:
        dic = {
            "name" : self._name,
            "parameters" : [parameter.dump() for parameter in self._parameters]
        }
        if self._return != -1:
            dic["return"] = self._return
        return dic

def intraCallMatch(call1 : FunctionCall, call2 : FunctionCall) -> bool:
    if call1._name != call2._name:
        return False
    if call1._return != -1 and call2._return != -1 and call1._return != call2._return:
        return False
    if len(call1._parameters) != len(call2._parameters):
        return False
    for param1, param2 in zip(call1._parameters, call2._parameters):
        if param1._idx != param2._idx:
            return False
        if param1._paramType != param2._paramType:
            return False
        if param1._paramType == FunctionCall.FunctionParameterType.CONST and param1._value != param2._value:
            return False
        elif param1._paramType == FunctionCall.FunctionParameterType.PTR and param1._ptrIndex != param2._ptrIndex:
            return False
        elif param1._paramType == FunctionCall.FunctionParameterType.FUNC and param1._name != param2._name:
            return False

    return True

def interCallMatch(call1 : FunctionCall, call2 : FunctionCall, ptrIdxMap : Dict[int, int]) -> bool:
    tempPtrIdxMap : Dict[int, int] = {}
    def ptrIdxMatch(idx1 : int, idx2 : int):
        nonlocal tempPtrIdxMap
        if idx2 in ptrIdxMap:
            return idx1 == ptrIdxMap[idx2]
        elif idx2 in tempPtrIdxMap:
            return idx1 == tempPtrIdxMap[idx2]
        else:
            tempPtrIdxMap[idx2] = idx1
            return True

    if call1._name != call2._name:
        return False
    if call1._return != -1 and call2._return != -1 and not ptrIdxMatch(call1._return, call2._return):
        return False
    if len(call1._parameters) != len(call2._parameters):
        return False
    for param1, param2 in zip(call1._parameters, call2._parameters):
        if param1._idx != param2._idx:
            return False
        if param1._paramType != param2._paramType:
            return False
        if param1._paramType == FunctionCall.FunctionParameterType.CONST and param1._value != param2._value:
            return False
        elif param1._paramType == FunctionCall.FunctionParameterType.PTR and not ptrIdxMatch(param1._ptrIndex, param2._ptrIndex):
            return False
        elif param1._paramType == FunctionCall.FunctionParameterType.FUNC and param1._name != param2._name:
            return False

    ptrIdxMap.update(tempPtrIdxMap)
    return True

# TODO : manipulate loads
class Graph:
    __slots__ = ("_nodes", "_loadPtrs", "_edges", "_reachables", "_nodeCount", "_nextPtrIdx")
    _nodes : List[FunctionCall]
    _loadPtrs : Dict[int, int]
    _edges : List[Tuple[int, int]]
    _reachables : DefaultDict[int, List[int]]
    _nodeCount : int
    _nextPtrIdx : int

    def __init__(self):
        self._nodes = [None, ]
        self._loadPtrs = {}
        self._edges = []
        self._reachables = defaultdict(list)
        self._nodeCount = 1
        self._nextPtrIdx = 0

    def addEdge(self, fromIdx : int, toIdx : int) -> bool:
        if fromIdx not in self._reachables[toIdx]:
            self._edges.append((fromIdx, toIdx))
            self._reachables[fromIdx].append(toIdx)
            for reachable in self._reachables.values():
                if fromIdx in reachable:
                    reachable.append(toIdx)
            return True
        else:
            return False

    def addCallNode(self, functionCall : FunctionCall, loadMap : Dict[int, int]) -> int:
        if functionCall._return != -1:
            assert functionCall._return not in loadMap
        for param in functionCall._parameters:
            if param._paramType == FunctionCall.FunctionParameterType.PTR and param._ptrIndex in loadMap:
                param._ptrIndex = loadMap[param._ptrIndex]

        for idx, preFunctionCall in enumerate(self._nodes[1:], 1):
            if intraCallMatch(preFunctionCall, functionCall):
                return idx
        self._nodes.append(functionCall)
        if functionCall._return != -1 and self._nextPtrIdx <= functionCall._return:
            self._nextPtrIdx = functionCall._return + 1
        for param in functionCall._parameters:
            if param._paramType == FunctionCall.FunctionParameterType.PTR and self._nextPtrIdx <= param._ptrIndex:
                self._nextPtrIdx = param._ptrIndex + 1
        self._nodeCount += 1
        return self._nodeCount - 1

    def addLoadNode(self, objectLoad : ObjectLoad, loadMap : Dict[int, int]):
        if objectLoad._address in self._loadPtrs:
            if objectLoad._value != self._loadPtrs[objectLoad._address]:
                loadMap[objectLoad._value] = self._loadPtrs[objectLoad._address]
        else:
            self._loadPtrs[objectLoad._address] = objectLoad._value

    def loadOUS(self, OUS : List[Union[FunctionCall, ObjectLoad]]):
        lastNodeIdx = 0
        loadMap : Dict[int, int] = {}
        for ou in OUS:
            if isinstance(ou, FunctionCall):
                thisNodeIdx = self.addCallNode(ou, loadMap)
                if self.addEdge(lastNodeIdx, thisNodeIdx):
                    lastNodeIdx = thisNodeIdx
            elif isinstance(ou, ObjectLoad):
                self.addLoadNode(ou, loadMap)
            else:
                assert False, f"Unknown ou type : {type(ou)}"

    def getNewIdx(self, idx : int, ptrMap : Dict[int, int]) -> int:
        if idx not in ptrMap:
            ptrMap[idx] = self._nextPtrIdx
            self._nextPtrIdx += 1
        return ptrMap[idx]

    def addCallNodeFromAnotherOUS(self, functionCall : FunctionCall, ptrMap : Dict[int, int]) -> int:
        for idx, preFunctionCall in enumerate(self._nodes[1:], 1):
            if interCallMatch(preFunctionCall, functionCall, ptrMap):
                return idx
        if functionCall._return != -1:
            functionCall._return = self.getNewIdx(functionCall._return, ptrMap)
        for param in functionCall._parameters:
            if param._paramType == FunctionCall.FunctionParameterType.PTR:
                param._ptrIndex = self.getNewIdx(param._ptrIndex, ptrMap)
        self._nodes.append(functionCall)
        self._nodeCount += 1
        return self._nodeCount - 1

    def addLoadNodeFromAnotherOUS(self, objectLoad : ObjectLoad, ptrMap : Dict[int, int]):
        assert objectLoad._value not in ptrMap
        mappedAddress = self.getNewIdx(objectLoad._address, ptrMap)
        if mappedAddress in self._loadPtrs:
            ptrMap[objectLoad._value] = self._loadPtrs[mappedAddress]
        else:
            self._loadPtrs[mappedAddress] = self.getNewIdx(objectLoad._value, ptrMap)

    def loadAnotherOUS(self, OUS : List[Union[FunctionCall, ObjectLoad]]):
        lastNodeIdx = 0
        ptrMap : Dict[int, int] = {}
        for ou in OUS:
            if isinstance(ou, FunctionCall):
                thisNodeIdx = self.addCallNodeFromAnotherOUS(ou, ptrMap)
                if self.addEdge(lastNodeIdx, thisNodeIdx):
                    lastNodeIdx = thisNodeIdx
            elif isinstance(ou, ObjectLoad):
                self.addLoadNodeFromAnotherOUS(ou, ptrMap)
            else:
                assert False, f"Unknown ou type : {type(ou)}"

    def dump(self, fileName : str):
        dic = {
            "edges" : list(self._edges),
            "loadPtrs" : tuple(self._loadPtrs.items()),
            "nodes" : [subNode.dump() if subNode else None for subNode in self._nodes],
            "pointerIdxCount" : self._nextPtrIdx,
        }
        with open(fileName, "wt") as f:
            json.dump(dic, f, indent = 4)

def loadOUSFromFile(file : str) -> List[Union[FunctionCall, ObjectLoad]]:
    with open(file, "rt") as f:
        lis : List[Dict] = json.load(f)
    ret = []
    for dic in lis:
        if dic["type"] == "LOAD":
            ret.append(ObjectLoad(dic))
        elif dic["type"] == "CALL":
            ret.append(FunctionCall(dic))
    return ret

if __name__ == "__main__":
    graph = Graph()
    if len(sys.argv) == 1:
        args = [x for x in os.listdir(".") if x.startswith("__TDDCallingChain")]
    else:
        args = sys.argv[1:]
    print(f"Merge {args}")
    graph.loadOUS(loadOUSFromFile(args[0]))
    for file in args[1:]:
        graph.loadAnotherOUS(loadOUSFromFile(file))
    graph.dump("__TDDFinalCallingChain.json")