#pragma once

#include "call-paths-to-bdd.h"

#include "../execution_plan/visitor.h"

#include <string>

namespace synapse {

class EP;
class EPNode;

enum class TargetType;

enum class ModuleType {
  Tofino_Ignore,
  Tofino_If,
  Tofino_IfHeaderValid,
  Tofino_Then,
  Tofino_Else,
  Tofino_Forward,
  Tofino_ParseCustomHeader,
  Tofino_ModifyCustomHeader,
  Tofino_ParserCondition,
  Tofino_EthernetConsume,
  Tofino_EthernetModify,
  Tofino_IPv4Consume,
  Tofino_IPv4Modify,
  Tofino_IPv4OptionsConsume,
  Tofino_IPv4OptionsModify,
  Tofino_TCPUDPConsume,
  Tofino_TCPUDPModify,
  Tofino_IPv4TCPUDPChecksumsUpdate,
  Tofino_TableLookup,
  Tofino_TableRejuvenation,
  Tofino_TableIsAllocated,
  Tofino_Drop,
  Tofino_SendToController,
  Tofino_SetupExpirationNotifications,
  Tofino_IntegerAllocatorAllocate,
  Tofino_IntegerAllocatorRejuvenate,
  Tofino_IntegerAllocatorQuery,
  Tofino_CounterRead,
  Tofino_CounterIncrement,
  Tofino_HashObj,
  TofinoCPU_Ignore,
  TofinoCPU_PacketParseCPU,
  TofinoCPU_SendToTofino,
  TofinoCPU_PacketParseEthernet,
  TofinoCPU_PacketModifyEthernet,
  TofinoCPU_PacketParseIPv4,
  TofinoCPU_PacketModifyIPv4,
  TofinoCPU_PacketParseIPv4Options,
  TofinoCPU_PacketModifyIPv4Options,
  TofinoCPU_PacketParseTCPUDP,
  TofinoCPU_PacketModifyTCPUDP,
  TofinoCPU_PacketParseTCP,
  TofinoCPU_PacketModifyTCP,
  TofinoCPU_PacketParseUDP,
  TofinoCPU_PacketModifyUDP,
  TofinoCPU_PacketModifyChecksums,
  TofinoCPU_If,
  TofinoCPU_Then,
  TofinoCPU_Else,
  TofinoCPU_Drop,
  TofinoCPU_ForwardThroughTofino,
  TofinoCPU_MapGet,
  TofinoCPU_MapPut,
  TofinoCPU_MapErase,
  TofinoCPU_DchainAllocateNewIndex,
  TofinoCPU_DchainIsIndexAllocated,
  TofinoCPU_DchainRejuvenateIndex,
  TofinoCPU_DchainFreeIndex,
  TofinoCPU_HashObj,
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
  x86_DchainRejuvenateIndex,
  x86_DchainAllocateNewIndex,
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

  Module(ModuleType _type, TargetType _target, const std::string &_name)
      : type(_type), target(_target), next_target(_target), name(_name),
        node(nullptr) {}

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

  virtual void visit(EPVisitor &visitor, const EPNode *ep_node) const = 0;
  virtual Module *clone() const = 0;
};

} // namespace synapse
