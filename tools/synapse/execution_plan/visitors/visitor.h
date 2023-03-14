#pragma once

#include <assert.h>
#include <memory>

#define VISIT(MODULE)                                                          \
  virtual void visit(const MODULE *m) { assert(false && "Unexpected module."); }

namespace synapse {

class ExecutionPlan;
class ExecutionPlanNode;

namespace targets {

namespace x86_bmv2 {
class MapGet;
class CurrentTime;
class PacketBorrowNextChunk;
class PacketGetMetadata;
class PacketReturnChunk;
class If;
class Then;
class Else;
class Forward;
class Broadcast;
class Drop;
class ExpireItemsSingleMap;
class RteEtherAddrHash;
class DchainRejuvenateIndex;
class VectorBorrow;
class VectorReturn;
class DchainAllocateNewIndex;
class MapPut;
class PacketGetUnreadLength;
class SetIpv4UdpTcpChecksum;
class DchainIsIndexAllocated;
} // namespace x86_bmv2

namespace bmv2 {
class SendToController;
class ParserConsume;
class Ignore;
class SetupExpirationNotifications;
class If;
class Then;
class Else;
class EthernetConsume;
class EthernetModify;
class TableLookup;
class IPv4Consume;
class IPv4Modify;
class TcpUdpConsume;
class TcpUdpModify;
class IPOptionsConsume;
class IPOptionsModify;
class Drop;
class Forward;
class VectorReturn;
} // namespace bmv2

namespace tofino {
class Ignore;
class If;
class Then;
class Else;
class Forward;
class EthernetConsume;
class EthernetModify;
class TableLookup;
class Drop;
class SendToController;
} // namespace tofino

namespace x86_tofino {
class Ignore;
class CurrentTime;
class PacketParseCPU;
class PacketParseEthernet;
class If;
class Then;
class Else;
class Drop;
class PacketModifyEthernet;
class ForwardThroughTofino;
} // namespace x86_tofino

} // namespace targets

class ExecutionPlanVisitor {
public:
  virtual void visit(ExecutionPlan ep);
  virtual void visit(const ExecutionPlanNode *ep_node);

  /*************************************
   *
   *              x86 BMv2
   *
   * **********************************/

  VISIT(targets::x86_bmv2::MapGet)
  VISIT(targets::x86_bmv2::CurrentTime)
  VISIT(targets::x86_bmv2::PacketBorrowNextChunk)
  VISIT(targets::x86_bmv2::PacketReturnChunk)
  VISIT(targets::x86_bmv2::PacketGetMetadata)
  VISIT(targets::x86_bmv2::If)
  VISIT(targets::x86_bmv2::Then)
  VISIT(targets::x86_bmv2::Else)
  VISIT(targets::x86_bmv2::Forward)
  VISIT(targets::x86_bmv2::Broadcast)
  VISIT(targets::x86_bmv2::Drop)
  VISIT(targets::x86_bmv2::ExpireItemsSingleMap)
  VISIT(targets::x86_bmv2::RteEtherAddrHash)
  VISIT(targets::x86_bmv2::DchainRejuvenateIndex)
  VISIT(targets::x86_bmv2::VectorBorrow)
  VISIT(targets::x86_bmv2::VectorReturn)
  VISIT(targets::x86_bmv2::DchainAllocateNewIndex)
  VISIT(targets::x86_bmv2::MapPut)
  VISIT(targets::x86_bmv2::PacketGetUnreadLength)
  VISIT(targets::x86_bmv2::SetIpv4UdpTcpChecksum)
  VISIT(targets::x86_bmv2::DchainIsIndexAllocated)

  /*************************************
   *
   *                 BMv2
   *
   * **********************************/

  VISIT(targets::bmv2::SendToController)
  VISIT(targets::bmv2::Ignore)
  VISIT(targets::bmv2::SetupExpirationNotifications)
  VISIT(targets::bmv2::If)
  VISIT(targets::bmv2::Then)
  VISIT(targets::bmv2::Else)
  VISIT(targets::bmv2::EthernetConsume)
  VISIT(targets::bmv2::EthernetModify)
  VISIT(targets::bmv2::TableLookup)
  VISIT(targets::bmv2::IPv4Consume)
  VISIT(targets::bmv2::IPv4Modify)
  VISIT(targets::bmv2::TcpUdpConsume)
  VISIT(targets::bmv2::TcpUdpModify)
  VISIT(targets::bmv2::IPOptionsConsume)
  VISIT(targets::bmv2::IPOptionsModify)
  VISIT(targets::bmv2::Drop)
  VISIT(targets::bmv2::Forward)
  VISIT(targets::bmv2::VectorReturn)

  /*************************************
   *
   *              Tofino
   *
   * **********************************/

  VISIT(targets::tofino::Ignore)
  VISIT(targets::tofino::If)
  VISIT(targets::tofino::Then)
  VISIT(targets::tofino::Else)
  VISIT(targets::tofino::Forward)
  VISIT(targets::tofino::EthernetConsume)
  VISIT(targets::tofino::EthernetModify)
  VISIT(targets::tofino::TableLookup)
  VISIT(targets::tofino::Drop)
  VISIT(targets::tofino::SendToController)

  /*************************************
   *
   *              x86 Tofino
   *
   * **********************************/

  VISIT(targets::x86_tofino::Ignore)
  VISIT(targets::x86_tofino::CurrentTime)
  VISIT(targets::x86_tofino::PacketParseCPU)
  VISIT(targets::x86_tofino::PacketParseEthernet)
  VISIT(targets::x86_tofino::If)
  VISIT(targets::x86_tofino::Then)
  VISIT(targets::x86_tofino::Else)
  VISIT(targets::x86_tofino::Drop)
  VISIT(targets::x86_tofino::PacketModifyEthernet)
  VISIT(targets::x86_tofino::ForwardThroughTofino)

protected:
  virtual void log(const ExecutionPlanNode *ep_node) const;
};

} // namespace synapse
