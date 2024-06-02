#pragma once

#include <assert.h>
#include <memory>

#define VISIT_TODO(M)                                                          \
  virtual void visit(const EP *ep, const EPNode *ep_node, const M *m) {        \
    assert(false && "TODO");                                                   \
  }

namespace synapse {

class EP;
class EPNode;

namespace tofino {
class SendToController;
class Ignore;
class If;
class ParserCondition;
class Then;
class Else;
class Forward;
class Drop;
class Broadcast;
class ParserExtraction;
class ModifyHeader;
class SimpleTableLookup;
} // namespace tofino

namespace tofino_cpu {
class Ignore;
class CurrentTime;
class PacketParseCPU;
class PacketParseEthernet;
class PacketModifyEthernet;
class PacketParseIPv4;
class PacketModifyIPv4;
class PacketParseIPv4Options;
class PacketModifyIPv4Options;
class PacketParseTCPUDP;
class PacketModifyTCPUDP;
class PacketParseTCP;
class PacketModifyTCP;
class PacketParseUDP;
class PacketModifyUDP;
class PacketModifyChecksums;
class ForwardThroughTofino;
class If;
class Then;
class Else;
class Drop;
class MapGet;
class MapPut;
class MapErase;
class DchainAllocateNewIndex;
class DchainIsIndexAllocated;
class DchainRejuvenateIndex;
class DchainFreeIndex;
class HashObj;
} // namespace tofino_cpu

namespace x86 {
class MapGet;
class CurrentTime;
class ParseHeader;
class ModifyHeader;
class If;
class Then;
class Else;
class Forward;
class Broadcast;
class Drop;
class ExpireItemsSingleMap;
class ExpireItemsSingleMapIteratively;
class DchainRejuvenateIndex;
class VectorRead;
class VectorWrite;
class DchainAllocateNewIndex;
class MapPut;
class ChecksumUpdate;
class DchainIsIndexAllocated;
class SketchComputeHashes;
class SketchExpire;
class SketchFetch;
class SketchRefresh;
class SketchTouchBuckets;
class MapErase;
class DchainFreeIndex;
class ChtFindBackend;
class HashObj;
} // namespace x86

class EPVisitor {
public:
  virtual void visit(const EP *ep);
  virtual void visit(const EP *ep, const EPNode *ep_node);

  /*************************************
   *
   *              Tofino
   *
   * **********************************/

  VISIT_TODO(tofino::SendToController)
  VISIT_TODO(tofino::Ignore)
  VISIT_TODO(tofino::If)
  VISIT_TODO(tofino::ParserCondition)
  VISIT_TODO(tofino::Then)
  VISIT_TODO(tofino::Else)
  VISIT_TODO(tofino::Forward)
  VISIT_TODO(tofino::Drop)
  VISIT_TODO(tofino::Broadcast)
  VISIT_TODO(tofino::ParserExtraction)
  VISIT_TODO(tofino::ModifyHeader)
  VISIT_TODO(tofino::SimpleTableLookup)

  /*************************************
   *
   *            Tofino CPU
   *
   * **********************************/

  // VISIT_TODO(tofino_cpu::Ignore)
  // VISIT_TODO(tofino_cpu::CurrentTime)
  // VISIT_TODO(tofino_cpu::PacketParseCPU)
  // VISIT_TODO(tofino_cpu::PacketParseEthernet)
  // VISIT_TODO(tofino_cpu::PacketModifyEthernet)
  // VISIT_TODO(tofino_cpu::PacketParseIPv4)
  // VISIT_TODO(tofino_cpu::PacketModifyIPv4)
  // VISIT_TODO(tofino_cpu::PacketParseIPv4Options)
  // VISIT_TODO(tofino_cpu::PacketModifyIPv4Options)
  // VISIT_TODO(tofino_cpu::PacketParseTCPUDP)
  // VISIT_TODO(tofino_cpu::PacketModifyTCPUDP)
  // VISIT_TODO(tofino_cpu::PacketParseTCP)
  // VISIT_TODO(tofino_cpu::PacketModifyTCP)
  // VISIT_TODO(tofino_cpu::PacketParseUDP)
  // VISIT_TODO(tofino_cpu::PacketModifyUDP)
  // VISIT_TODO(tofino_cpu::PacketModifyChecksums)
  // VISIT_TODO(tofino_cpu::ForwardThroughTofino)
  // VISIT_TODO(tofino_cpu::If)
  // VISIT_TODO(tofino_cpu::Then)
  // VISIT_TODO(tofino_cpu::Else)
  // VISIT_TODO(tofino_cpu::Drop)
  // VISIT_TODO(tofino_cpu::MapGet)
  // VISIT_TODO(tofino_cpu::MapPut)
  // VISIT_TODO(tofino_cpu::DchainAllocateNewIndex)
  // VISIT_TODO(tofino_cpu::DchainIsIndexAllocated)
  // VISIT_TODO(tofino_cpu::DchainRejuvenateIndex)
  // VISIT_TODO(tofino_cpu::DchainFreeIndex)
  // VISIT_TODO(tofino_cpu::HashObj)
  // VISIT_TODO(tofino_cpu::MapErase)

  /*************************************
   *
   *                x86
   *
   * **********************************/

  VISIT_TODO(x86::If)
  VISIT_TODO(x86::Then)
  VISIT_TODO(x86::Else)
  VISIT_TODO(x86::Forward)
  VISIT_TODO(x86::Broadcast)
  VISIT_TODO(x86::Drop)
  VISIT_TODO(x86::ParseHeader)
  VISIT_TODO(x86::ModifyHeader)
  VISIT_TODO(x86::MapGet)
  VISIT_TODO(x86::CurrentTime)
  VISIT_TODO(x86::ExpireItemsSingleMap)
  VISIT_TODO(x86::ExpireItemsSingleMapIteratively)
  VISIT_TODO(x86::DchainRejuvenateIndex)
  VISIT_TODO(x86::VectorRead)
  VISIT_TODO(x86::VectorWrite)
  VISIT_TODO(x86::DchainAllocateNewIndex)
  VISIT_TODO(x86::MapPut)
  VISIT_TODO(x86::ChecksumUpdate)
  VISIT_TODO(x86::DchainIsIndexAllocated)
  VISIT_TODO(x86::SketchComputeHashes)
  VISIT_TODO(x86::SketchExpire)
  VISIT_TODO(x86::SketchFetch)
  VISIT_TODO(x86::SketchRefresh)
  VISIT_TODO(x86::SketchTouchBuckets)
  VISIT_TODO(x86::MapErase)
  VISIT_TODO(x86::DchainFreeIndex)
  VISIT_TODO(x86::ChtFindBackend)
  VISIT_TODO(x86::HashObj)

protected:
  virtual void log(const EPNode *ep_node) const;
};

} // namespace synapse
