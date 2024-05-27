#pragma once

#include "call-paths-to-bdd.h"
#include "bdd-visualizer.h"

#include "../targets/target.h"
#include "../execution_plan/visitor.h"

#include <vector>

#define DECLARE_VISIT(M)                                                       \
  void visit(const EPNode *ep_node, const M *node) override;

namespace synapse {

class EPVisualizer : public EPVisitor, public Graphviz {
public:
  EPVisualizer();

  static void visualize(const EP &ep, bool interrupt);

  void visit(EP ep) override;
  void visit(const EPNode *ep_node) override;

  /********************************************
   *
   *                  Tofino
   *
   ********************************************/

  // DECLARE_VISIT(tofino::Ignore)
  // DECLARE_VISIT(tofino::If)
  // DECLARE_VISIT(tofino::Then)
  // DECLARE_VISIT(tofino::Else)
  // DECLARE_VISIT(tofino::Forward)
  // DECLARE_VISIT(tofino::ParseCustomHeader)
  // DECLARE_VISIT(tofino::ModifyCustomHeader)
  // DECLARE_VISIT(tofino::ParserCondition)
  // DECLARE_VISIT(tofino::IPv4TCPUDPChecksumsUpdate)
  // DECLARE_VISIT(tofino::TableModule)
  // DECLARE_VISIT(tofino::TableLookup)
  // DECLARE_VISIT(tofino::TableRejuvenation)
  // DECLARE_VISIT(tofino::TableIsAllocated)
  // DECLARE_VISIT(tofino::IntegerAllocatorAllocate)
  // DECLARE_VISIT(tofino::IntegerAllocatorRejuvenate)
  // DECLARE_VISIT(tofino::IntegerAllocatorQuery)
  // DECLARE_VISIT(tofino::Drop)
  // DECLARE_VISIT(tofino::SendToController)
  // DECLARE_VISIT(tofino::SetupExpirationNotifications)
  // DECLARE_VISIT(tofino::CounterRead)
  // DECLARE_VISIT(tofino::CounterIncrement)
  // DECLARE_VISIT(tofino::HashObj)

  /********************************************
   *
   *                x86 Tofino
   *
   ********************************************/

  // DECLARE_VISIT(tofino_cpu::Ignore)
  // DECLARE_VISIT(tofino_cpu::PacketParseCPU)
  // DECLARE_VISIT(tofino_cpu::PacketParseEthernet)
  // DECLARE_VISIT(tofino_cpu::PacketModifyEthernet)
  // DECLARE_VISIT(tofino_cpu::ForwardThroughTofino)
  // DECLARE_VISIT(tofino_cpu::PacketParseIPv4)
  // DECLARE_VISIT(tofino_cpu::PacketModifyIPv4)
  // DECLARE_VISIT(tofino_cpu::PacketParseIPv4Options)
  // DECLARE_VISIT(tofino_cpu::PacketModifyIPv4Options)
  // DECLARE_VISIT(tofino_cpu::PacketParseTCPUDP)
  // DECLARE_VISIT(tofino_cpu::PacketModifyTCPUDP)
  // DECLARE_VISIT(tofino_cpu::PacketModifyChecksums)
  // DECLARE_VISIT(tofino_cpu::If)
  // DECLARE_VISIT(tofino_cpu::Then)
  // DECLARE_VISIT(tofino_cpu::Else)
  // DECLARE_VISIT(tofino_cpu::Drop)
  // DECLARE_VISIT(tofino_cpu::MapGet)
  // DECLARE_VISIT(tofino_cpu::MapPut)
  // DECLARE_VISIT(tofino_cpu::MapErase)
  // DECLARE_VISIT(tofino_cpu::DchainAllocateNewIndex)
  // DECLARE_VISIT(tofino_cpu::DchainIsIndexAllocated)
  // DECLARE_VISIT(tofino_cpu::DchainRejuvenateIndex)
  // DECLARE_VISIT(tofino_cpu::DchainFreeIndex)
  // DECLARE_VISIT(tofino_cpu::PacketParseTCP)
  // DECLARE_VISIT(tofino_cpu::PacketModifyTCP)
  // DECLARE_VISIT(tofino_cpu::PacketParseUDP)
  // DECLARE_VISIT(tofino_cpu::PacketModifyUDP)
  // DECLARE_VISIT(tofino_cpu::HashObj)

  /********************************************
   *
   *                  x86
   *
   ********************************************/

  // DECLARE_VISIT(x86::MapGet)
  // DECLARE_VISIT(x86::CurrentTime)
  // DECLARE_VISIT(x86::PacketBorrowNextChunk)
  // DECLARE_VISIT(x86::PacketReturnChunk)
  // DECLARE_VISIT(x86::If)
  // DECLARE_VISIT(x86::Then)
  // DECLARE_VISIT(x86::Else)
  // DECLARE_VISIT(x86::Forward)
  // DECLARE_VISIT(x86::Broadcast)
  // DECLARE_VISIT(x86::Drop)
  // DECLARE_VISIT(x86::ExpireItemsSingleMap)
  // DECLARE_VISIT(x86::ExpireItemsSingleMapIteratively)
  // DECLARE_VISIT(x86::DchainRejuvenateIndex)
  // DECLARE_VISIT(x86::VectorBorrow)
  // DECLARE_VISIT(x86::VectorReturn)
  // DECLARE_VISIT(x86::DchainAllocateNewIndex)
  // DECLARE_VISIT(x86::MapPut)
  // DECLARE_VISIT(x86::SetIpv4UdpTcpChecksum)
  // DECLARE_VISIT(x86::DchainIsIndexAllocated)
  // DECLARE_VISIT(x86::SketchComputeHashes)
  // DECLARE_VISIT(x86::SketchExpire)
  // DECLARE_VISIT(x86::SketchFetch)
  // DECLARE_VISIT(x86::SketchRefresh)
  // DECLARE_VISIT(x86::SketchTouchBuckets)
  // DECLARE_VISIT(x86::MapErase)
  // DECLARE_VISIT(x86::DchainFreeIndex)
  // DECLARE_VISIT(x86::LoadBalancedFlowHash)
  // DECLARE_VISIT(x86::ChtFindBackend)
  // DECLARE_VISIT(x86::HashObj)

private:
  void function_call(const EPNode *ep_node, const bdd::Node *node,
                     TargetType target, const std::string &label);
  void branch(const EPNode *ep_node, const bdd::Node *node, TargetType target,
              const std::string &label);
};
} // namespace synapse
