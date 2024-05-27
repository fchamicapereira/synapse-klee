#pragma once

#include <assert.h>
#include <memory>

#define VISIT(M)                                                               \
  virtual void visit(const EPNode *ep_node, const M *m) {                      \
    assert(false && "Unexpected module.");                                     \
  }

namespace synapse {

class EP;
class EPNode;

namespace tofino {
class Ignore;
class If;
class Then;
class Else;
class Forward;
class ParseCustomHeader;
class ModifyCustomHeader;
class ParserCondition;
class IPv4TCPUDPChecksumsUpdate;
class Drop;
class SendToController;
class SetupExpirationNotifications;
class TableModule;
class TableLookup;
class TableRejuvenation;
class TableIsAllocated;
class IntegerAllocatorAllocate;
class IntegerAllocatorRejuvenate;
class IntegerAllocatorQuery;
class CounterRead;
class CounterIncrement;
class HashObj;
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
class PacketBorrowNextChunk;
class PacketReturnChunk;
class If;
class Then;
class Else;
class Forward;
class Broadcast;
class Drop;
class ExpireItemsSingleMap;
class ExpireItemsSingleMapIteratively;
class DchainRejuvenateIndex;
class VectorBorrow;
class VectorReturn;
class DchainAllocateNewIndex;
class MapPut;
class SetIpv4UdpTcpChecksum;
class DchainIsIndexAllocated;
class SketchComputeHashes;
class SketchExpire;
class SketchFetch;
class SketchRefresh;
class SketchTouchBuckets;
class MapErase;
class DchainFreeIndex;
class LoadBalancedFlowHash;
class ChtFindBackend;
class HashObj;
} // namespace x86

class EPVisitor {
public:
  virtual void visit(EP ep);
  virtual void visit(const EPNode *ep_node);

  /*************************************
   *
   *              Tofino
   *
   * **********************************/

  VISIT(tofino::Ignore)
  VISIT(tofino::If)
  VISIT(tofino::Then)
  VISIT(tofino::Else)
  VISIT(tofino::Forward)
  VISIT(tofino::ParseCustomHeader)
  VISIT(tofino::ModifyCustomHeader)
  VISIT(tofino::ParserCondition)
  VISIT(tofino::Drop)
  VISIT(tofino::SendToController)
  VISIT(tofino::SetupExpirationNotifications)
  VISIT(tofino::IPv4TCPUDPChecksumsUpdate)
  VISIT(tofino::TableModule)
  VISIT(tofino::TableLookup)
  VISIT(tofino::TableRejuvenation)
  VISIT(tofino::TableIsAllocated)
  VISIT(tofino::IntegerAllocatorAllocate)
  VISIT(tofino::IntegerAllocatorRejuvenate)
  VISIT(tofino::IntegerAllocatorQuery)
  VISIT(tofino::CounterRead)
  VISIT(tofino::CounterIncrement)
  VISIT(tofino::HashObj)

  /*************************************
   *
   *              x86 Tofino
   *
   * **********************************/

  VISIT(tofino_cpu::Ignore)
  VISIT(tofino_cpu::CurrentTime)
  VISIT(tofino_cpu::PacketParseCPU)
  VISIT(tofino_cpu::PacketParseEthernet)
  VISIT(tofino_cpu::PacketModifyEthernet)
  VISIT(tofino_cpu::PacketParseIPv4)
  VISIT(tofino_cpu::PacketModifyIPv4)
  VISIT(tofino_cpu::PacketParseIPv4Options)
  VISIT(tofino_cpu::PacketModifyIPv4Options)
  VISIT(tofino_cpu::PacketParseTCPUDP)
  VISIT(tofino_cpu::PacketModifyTCPUDP)
  VISIT(tofino_cpu::PacketParseTCP)
  VISIT(tofino_cpu::PacketModifyTCP)
  VISIT(tofino_cpu::PacketParseUDP)
  VISIT(tofino_cpu::PacketModifyUDP)
  VISIT(tofino_cpu::PacketModifyChecksums)
  VISIT(tofino_cpu::ForwardThroughTofino)
  VISIT(tofino_cpu::If)
  VISIT(tofino_cpu::Then)
  VISIT(tofino_cpu::Else)
  VISIT(tofino_cpu::Drop)
  VISIT(tofino_cpu::MapGet)
  VISIT(tofino_cpu::MapPut)
  VISIT(tofino_cpu::DchainAllocateNewIndex)
  VISIT(tofino_cpu::DchainIsIndexAllocated)
  VISIT(tofino_cpu::DchainRejuvenateIndex)
  VISIT(tofino_cpu::DchainFreeIndex)
  VISIT(tofino_cpu::HashObj)
  VISIT(tofino_cpu::MapErase)

  /*************************************
   *
   *                x86
   *
   * **********************************/

  VISIT(x86::MapGet)
  VISIT(x86::CurrentTime)
  VISIT(x86::PacketBorrowNextChunk)
  VISIT(x86::PacketReturnChunk)
  VISIT(x86::If)
  VISIT(x86::Then)
  VISIT(x86::Else)
  VISIT(x86::Forward)
  VISIT(x86::Broadcast)
  VISIT(x86::Drop)
  VISIT(x86::ExpireItemsSingleMap)
  VISIT(x86::ExpireItemsSingleMapIteratively)
  VISIT(x86::DchainRejuvenateIndex)
  VISIT(x86::VectorBorrow)
  VISIT(x86::VectorReturn)
  VISIT(x86::DchainAllocateNewIndex)
  VISIT(x86::MapPut)
  VISIT(x86::SetIpv4UdpTcpChecksum)
  VISIT(x86::DchainIsIndexAllocated)
  VISIT(x86::SketchComputeHashes)
  VISIT(x86::SketchExpire)
  VISIT(x86::SketchFetch)
  VISIT(x86::SketchRefresh)
  VISIT(x86::SketchTouchBuckets)
  VISIT(x86::MapErase)
  VISIT(x86::DchainFreeIndex)
  VISIT(x86::LoadBalancedFlowHash)
  VISIT(x86::ChtFindBackend)
  VISIT(x86::HashObj)

protected:
  virtual void log(const EPNode *ep_node) const;
};

} // namespace synapse
