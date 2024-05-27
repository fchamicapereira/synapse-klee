#pragma once

#include "call-paths-to-bdd.h"

#include "context.h"
#include "target.h"

#include "../log.h"

namespace synapse {

class EP;
class EPNode;
class EPVisitor;

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
  x86_CurrentTime,
  x86_If,
  x86_Then,
  x86_Else,
  x86_MapGet,
  x86_MapPut,
  x86_MapErase,
  x86_VectorBorrow,
  x86_VectorReturn,
  x86_DchainRejuvenateIndex,
  x86_DchainAllocateNewIndex,
  x86_DchainIsIndexAllocated,
  x86_DchainFreeIndex,
  x86_SketchExpire,
  x86_SketchComputeHashes,
  x86_SketchRefresh,
  x86_SketchFetch,
  x86_SketchTouchBuckets,
  x86_PacketBorrowNextChunk,
  x86_PacketReturnChunk,
  x86_Forward,
  x86_Drop,
  x86_Broadcast,
  x86_ExpireItemsSingleMap,
  x86_ExpireItemsSingleMapIteratively,
  x86_SetIpv4UdpTcpChecksum,
  x86_LoadBalancedFlowHash,
  x86_ChtFindBackend,
  x86_HashObj,
};

class Module {
protected:
  ModuleType type;
  TargetType target;
  TargetType next_target;
  const std::string &name;
  const bdd::Node *node;

  Module(ModuleType _type, TargetType _target, const std::string &_name,
         const bdd::Node *_node)
      : type(_type), target(_target), next_target(_target), name(_name),
        node(_node) {}

  Module(ModuleType _type, TargetType _target, const std::string &_name)
      : type(_type), target(_target), next_target(_target), name(_name),
        node(nullptr) {}

public:
  Module(const Module &m) : Module(m.type, m.target, m.name, m.node) {}

  ModuleType get_type() const { return type; }
  const std::string &get_name() const { return name; }
  TargetType get_target() const { return target; }
  TargetType get_next_target() const { return next_target; }
  const bdd::Node *get_node() const { return node; }

  void replace_node(const bdd::Node *_node) {
    node = _node;
    assert(node);
  }

  virtual void visit(EPVisitor &visitor, const EPNode *ep_node) const = 0;
  virtual Module *clone() const = 0;
  virtual bool equals(const Module *other) const = 0;

protected:
  // General useful queries
  bool query_contains_map_has_key(const bdd::Branch *node) const;

  std::vector<const bdd::Node *> get_prev_fn(const EP &ep,
                                             const bdd::Node *node,
                                             const std::string &fnames,
                                             bool ignore_targets = false) const;
  std::vector<const bdd::Node *>
  get_prev_fn(const EP &ep, const bdd::Node *node,
              const std::vector<std::string> &functions_names,
              bool ignore_targets = false) const;

  std::vector<const bdd::Node *>
  get_all_functions_after_node(const bdd::Node *root,
                               const std::vector<std::string> &functions,
                               bool stop_on_branches = false) const;

  bool is_parser_drop(const bdd::Node *root) const;

  std::vector<const Module *>
  get_prev_modules(const EP &ep, const std::vector<ModuleType> &) const;

  bool is_expr_only_packet_dependent(klee::ref<klee::Expr> expr) const;

  struct counter_data_t {
    bool valid;
    std::vector<const bdd::Node *> reads;
    std::vector<const bdd::Node *> writes;
    std::pair<bool, uint64_t> max_value;

    counter_data_t() : valid(false) {}
  };

  // Check if a given data structure is a counter. We expect counters to be
  // implemented with vectors, such that (1) the value it stores is <= 64 bits,
  // and (2) the only write operations performed on them increment the stored
  // value.
  counter_data_t is_counter(const EP &ep, addr_t obj) const;

  // When we encounter a vector_return operation and want to retrieve its
  // vector_borrow value counterpart. This is useful to compare changes to the
  // value expression and retrieve the performed modifications (if any).
  klee::ref<klee::Expr> get_original_vector_value(const EP &ep,
                                                  const bdd::Node *node,
                                                  addr_t target_addr) const;
  klee::ref<klee::Expr>
  get_original_vector_value(const EP &ep, const bdd::Node *node,
                            addr_t target_addr, const bdd::Node *&source) const;

  // Get the data associated with this address.
  klee::ref<klee::Expr> get_expr_from_addr(const EP &ep, addr_t addr) const;

  struct map_coalescing_data_t {
    bool valid;
    addr_t map;
    addr_t dchain;
    std::unordered_set<addr_t> vectors;

    map_coalescing_data_t() : valid(false) {}
  };

  map_coalescing_data_t get_map_coalescing_data_t(const EP &ep,
                                                  addr_t map_addr) const;
};

} // namespace synapse
