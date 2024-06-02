#pragma once

#include "call-paths-to-bdd.h"
#include "bdd-visualizer.h"

#include "../targets/target.h"
#include "../execution_plan/visitor.h"

#include <vector>

#define DECLARE_VISIT(M)                                                       \
  void visit(const EP *ep, const EPNode *ep_node, const M *node) override;

namespace synapse {

class EPVisualizer : public EPVisitor, public Graphviz {
public:
  EPVisualizer();

  static void visualize(const EP *ep, bool interrupt);

  void visit(const EP *ep) override;
  void visit(const EP *ep, const EPNode *ep_node) override;

  /********************************************
   *
   *                  Tofino
   *
   ********************************************/

  DECLARE_VISIT(tofino::SendToController)
  DECLARE_VISIT(tofino::Ignore)
  DECLARE_VISIT(tofino::If)
  DECLARE_VISIT(tofino::ParserCondition)
  DECLARE_VISIT(tofino::Then)
  DECLARE_VISIT(tofino::Else)
  DECLARE_VISIT(tofino::Forward)
  DECLARE_VISIT(tofino::Drop)
  DECLARE_VISIT(tofino::Broadcast)
  DECLARE_VISIT(tofino::ParserExtraction)
  DECLARE_VISIT(tofino::ModifyHeader)
  DECLARE_VISIT(tofino::SimpleTableLookup)

  /********************************************
   *
   *              Tofino CPU
   *
   ********************************************/

  // DECLARE_VISIT(tofino_cpu::If)
  // DECLARE_VISIT(tofino_cpu::Then)
  // DECLARE_VISIT(tofino_cpu::Else)
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

  DECLARE_VISIT(x86::If)
  DECLARE_VISIT(x86::Then)
  DECLARE_VISIT(x86::Else)
  DECLARE_VISIT(x86::Forward)
  DECLARE_VISIT(x86::Broadcast)
  DECLARE_VISIT(x86::Drop)
  DECLARE_VISIT(x86::ParseHeader)
  DECLARE_VISIT(x86::ModifyHeader)
  DECLARE_VISIT(x86::ChecksumUpdate)
  DECLARE_VISIT(x86::MapGet)
  DECLARE_VISIT(x86::MapPut)
  DECLARE_VISIT(x86::MapErase)
  DECLARE_VISIT(x86::ExpireItemsSingleMap)
  DECLARE_VISIT(x86::ExpireItemsSingleMapIteratively)
  DECLARE_VISIT(x86::VectorRead)
  DECLARE_VISIT(x86::VectorWrite)
  DECLARE_VISIT(x86::DchainRejuvenateIndex)
  DECLARE_VISIT(x86::DchainAllocateNewIndex)
  DECLARE_VISIT(x86::DchainIsIndexAllocated)
  DECLARE_VISIT(x86::DchainFreeIndex)
  DECLARE_VISIT(x86::SketchComputeHashes)
  DECLARE_VISIT(x86::SketchExpire)
  DECLARE_VISIT(x86::SketchFetch)
  DECLARE_VISIT(x86::SketchRefresh)
  DECLARE_VISIT(x86::SketchTouchBuckets)
  DECLARE_VISIT(x86::HashObj)
  DECLARE_VISIT(x86::ChtFindBackend)

protected:
  virtual void log(const EPNode *ep_node) const override;

private:
  void function_call(const EPNode *ep_node, const bdd::Node *node,
                     TargetType target, const std::string &label);
  void branch(const EPNode *ep_node, const bdd::Node *node, TargetType target,
              const std::string &label);
};
} // namespace synapse
