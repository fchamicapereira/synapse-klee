#pragma once

#include "call-paths-to-bdd.h"
#include "bdd-visualizer.h"

#include "../visualizers/ep_visualizer.h"
#include "../visualizers/profiler_visualizer.h"
#include "../execution_plan/visitor.h"
#include "../log.h"

#include <string>

namespace synapse {

class EP;
class EPNode;

enum class TargetType;

enum class ModuleType {
  Tofino_SendToController,
  Tofino_Ignore,
  Tofino_IfSimple,
  Tofino_If,
  Tofino_ParserExtraction,
  Tofino_ParserCondition,
  Tofino_ParserReject,
  Tofino_Then,
  Tofino_Else,
  Tofino_Forward,
  Tofino_Drop,
  Tofino_Broadcast,
  Tofino_ModifyHeader,
  Tofino_SimpleTableLookup,
  Tofino_VectorRegisterLookup,
  Tofino_VectorRegisterUpdate,
  Tofino_FCFSCachedTableRead,
  Tofino_FCFSCachedTableReadOrWrite,
  Tofino_FCFSCachedTableWrite,
  Tofino_FCFSCachedTableDelete,
  Tofino_Recirculate,
  TofinoCPU_Ignore,
  TofinoCPU_ParseHeader,
  TofinoCPU_ModifyHeader,
  TofinoCPU_ChecksumUpdate,
  TofinoCPU_If,
  TofinoCPU_Then,
  TofinoCPU_Else,
  TofinoCPU_Forward,
  TofinoCPU_Broadcast,
  TofinoCPU_Drop,
  TofinoCPU_SimpleTableLookup,
  TofinoCPU_SimpleTableUpdate,
  TofinoCPU_SimpleTableDelete,
  TofinoCPU_FCFSCachedTableRead,
  TofinoCPU_FCFSCachedTableWrite,
  TofinoCPU_FCFSCachedTableDelete,
  TofinoCPU_DchainAllocateNewIndex,
  TofinoCPU_DchainRejuvenateIndex,
  TofinoCPU_DchainIsIndexAllocated,
  TofinoCPU_DchainFreeIndex,
  TofinoCPU_VectorRead,
  TofinoCPU_VectorWrite,
  TofinoCPU_MapGet,
  TofinoCPU_MapPut,
  TofinoCPU_MapErase,
  TofinoCPU_ChtFindBackend,
  TofinoCPU_HashObj,
  TofinoCPU_SketchExpire,
  TofinoCPU_SketchComputeHashes,
  TofinoCPU_SketchRefresh,
  TofinoCPU_SketchFetch,
  TofinoCPU_SketchTouchBuckets,
  TofinoCPU_VectorRegisterLookup,
  TofinoCPU_VectorRegisterUpdate,
  x86_Ignore,
  x86_If,
  x86_Then,
  x86_Else,
  x86_Forward,
  x86_ParseHeader,
  x86_ModifyHeader,
  x86_MapGet,
  x86_MapPut,
  x86_MapErase,
  x86_VectorRead,
  x86_VectorWrite,
  x86_DchainAllocateNewIndex,
  x86_DchainRejuvenateIndex,
  x86_DchainIsIndexAllocated,
  x86_DchainFreeIndex,
  x86_SketchExpire,
  x86_SketchComputeHashes,
  x86_SketchRefresh,
  x86_SketchFetch,
  x86_SketchTouchBuckets,
  x86_Drop,
  x86_Broadcast,
  x86_ExpireItemsSingleMap,
  x86_ExpireItemsSingleMapIteratively,
  x86_ChecksumUpdate,
  x86_ChtFindBackend,
  x86_HashObj,
};

class Module {
protected:
  ModuleType type;
  TargetType target;
  TargetType next_target;
  std::string name;
  const bdd::Node *node;

  Module(ModuleType _type, TargetType _target, const std::string &_name,
         const bdd::Node *_node)
      : type(_type), target(_target), next_target(_target), name(_name),
        node(_node) {}

  Module(ModuleType _type, TargetType _target, TargetType _next_target,
         const std::string &_name, const bdd::Node *_node)
      : type(_type), target(_target), next_target(_next_target), name(_name),
        node(_node) {}

public:
  Module(const Module &other) = delete;
  Module(Module &&other) = delete;

  Module &operator=(const Module &other) = delete;

  virtual ~Module() {}

  ModuleType get_type() const { return type; }
  const std::string &get_name() const { return name; }
  TargetType get_target() const { return target; }
  TargetType get_next_target() const { return next_target; }
  const bdd::Node *get_node() const { return node; }

  void set_node(const bdd::Node *new_node) { node = new_node; }

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const = 0;
  virtual Module *clone() const = 0;
};

} // namespace synapse
