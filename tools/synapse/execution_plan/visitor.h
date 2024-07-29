#pragma once

#include <assert.h>
#include <memory>

#define DECLARE_VISIT(M)                                                       \
  void visit(const EP *ep, const EPNode *ep_node, const M *node) override;

#define VISIT_NOP(M)                                                           \
  virtual void visit(const EP *ep, const EPNode *ep_node, const M *m) {}

#define VISIT_TODO(M)                                                          \
  virtual void visit(const EP *ep, const EPNode *ep_node, const M *m) {        \
    assert(false && "TODO");                                                   \
  }

namespace synapse {

class EP;
class EPNode;

namespace tofino {
class SendToController;
class Recirculate;
class Ignore;
class IfSimple;
class If;
class Then;
class Else;
class Forward;
class Drop;
class Broadcast;
class ParserExtraction;
class ParserCondition;
class ParserReject;
class ModifyHeader;
class SimpleTableLookup;
class VectorRegisterLookup;
class VectorRegisterUpdate;
class TTLCachedTableRead;
class TTLCachedTableReadOrWrite;
class TTLCachedTableWrite;
class TTLCachedTableConditionalDelete;
class TTLCachedTableDelete;
} // namespace tofino

namespace tofino_cpu {
class Ignore;
class ParseHeader;
class ModifyHeader;
class ChecksumUpdate;
class If;
class Then;
class Else;
class Forward;
class Broadcast;
class Drop;
class SimpleTableLookup;
class SimpleTableUpdate;
class DchainAllocateNewIndex;
class DchainRejuvenateIndex;
class DchainIsIndexAllocated;
class DchainFreeIndex;
class VectorRead;
class VectorWrite;
class MapGet;
class MapPut;
class MapErase;
class ChtFindBackend;
class HashObj;
class SketchComputeHashes;
class SketchExpire;
class SketchFetch;
class SketchRefresh;
class SketchTouchBuckets;
class SimpleTableDelete;
class VectorRegisterLookup;
class VectorRegisterUpdate;
class TTLCachedTableRead;
class TTLCachedTableWrite;
class TTLCachedTableDelete;
} // namespace tofino_cpu

namespace x86 {
class Ignore;
class ParseHeader;
class ModifyHeader;
class If;
class Then;
class Else;
class Forward;
class Broadcast;
class Drop;
class MapGet;
class MapPut;
class MapErase;
class ExpireItemsSingleMap;
class ExpireItemsSingleMapIteratively;
class VectorRead;
class VectorWrite;
class ChecksumUpdate;
class DchainAllocateNewIndex;
class DchainRejuvenateIndex;
class DchainIsIndexAllocated;
class DchainFreeIndex;
class SketchComputeHashes;
class SketchExpire;
class SketchFetch;
class SketchRefresh;
class SketchTouchBuckets;
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

  VISIT_NOP(tofino::SendToController)
  VISIT_NOP(tofino::Ignore)
  VISIT_NOP(tofino::IfSimple)
  VISIT_NOP(tofino::If)
  VISIT_NOP(tofino::Then)
  VISIT_NOP(tofino::Else)
  VISIT_NOP(tofino::Forward)
  VISIT_NOP(tofino::Drop)
  VISIT_NOP(tofino::Broadcast)
  VISIT_NOP(tofino::ParserExtraction)
  VISIT_NOP(tofino::ParserCondition)
  VISIT_NOP(tofino::ParserReject)
  VISIT_NOP(tofino::ModifyHeader)
  VISIT_NOP(tofino::SimpleTableLookup)
  VISIT_NOP(tofino::VectorRegisterLookup)
  VISIT_NOP(tofino::VectorRegisterUpdate)
  VISIT_NOP(tofino::TTLCachedTableRead)
  VISIT_NOP(tofino::TTLCachedTableReadOrWrite)
  VISIT_NOP(tofino::TTLCachedTableWrite)
  VISIT_NOP(tofino::TTLCachedTableConditionalDelete)
  VISIT_NOP(tofino::TTLCachedTableDelete)
  VISIT_NOP(tofino::Recirculate)

  /*************************************
   *
   *            Tofino CPU
   *
   * **********************************/

  VISIT_NOP(tofino_cpu::Ignore)
  VISIT_NOP(tofino_cpu::ParseHeader)
  VISIT_NOP(tofino_cpu::ModifyHeader)
  VISIT_NOP(tofino_cpu::ChecksumUpdate)
  VISIT_NOP(tofino_cpu::If)
  VISIT_NOP(tofino_cpu::Then)
  VISIT_NOP(tofino_cpu::Else)
  VISIT_NOP(tofino_cpu::Forward)
  VISIT_NOP(tofino_cpu::Broadcast)
  VISIT_NOP(tofino_cpu::Drop)
  VISIT_NOP(tofino_cpu::SimpleTableLookup)
  VISIT_NOP(tofino_cpu::SimpleTableUpdate)
  VISIT_NOP(tofino_cpu::SimpleTableDelete)
  VISIT_NOP(tofino_cpu::DchainAllocateNewIndex)
  VISIT_NOP(tofino_cpu::DchainRejuvenateIndex)
  VISIT_NOP(tofino_cpu::DchainIsIndexAllocated)
  VISIT_NOP(tofino_cpu::DchainFreeIndex)
  VISIT_NOP(tofino_cpu::VectorRead)
  VISIT_NOP(tofino_cpu::VectorWrite)
  VISIT_NOP(tofino_cpu::MapGet)
  VISIT_NOP(tofino_cpu::MapPut)
  VISIT_NOP(tofino_cpu::MapErase)
  VISIT_NOP(tofino_cpu::ChtFindBackend)
  VISIT_NOP(tofino_cpu::HashObj)
  VISIT_NOP(tofino_cpu::SketchComputeHashes)
  VISIT_NOP(tofino_cpu::SketchExpire)
  VISIT_NOP(tofino_cpu::SketchFetch)
  VISIT_NOP(tofino_cpu::SketchRefresh)
  VISIT_NOP(tofino_cpu::SketchTouchBuckets)
  VISIT_NOP(tofino_cpu::VectorRegisterLookup)
  VISIT_NOP(tofino_cpu::VectorRegisterUpdate)
  VISIT_NOP(tofino_cpu::TTLCachedTableRead)
  VISIT_NOP(tofino_cpu::TTLCachedTableWrite)
  VISIT_NOP(tofino_cpu::TTLCachedTableDelete)

  /*************************************
   *
   *                x86
   *
   * **********************************/

  VISIT_NOP(x86::Ignore)
  VISIT_NOP(x86::If)
  VISIT_NOP(x86::Then)
  VISIT_NOP(x86::Else)
  VISIT_NOP(x86::Forward)
  VISIT_NOP(x86::Broadcast)
  VISIT_NOP(x86::Drop)
  VISIT_NOP(x86::ParseHeader)
  VISIT_NOP(x86::ModifyHeader)
  VISIT_NOP(x86::MapGet)
  VISIT_NOP(x86::ExpireItemsSingleMap)
  VISIT_NOP(x86::ExpireItemsSingleMapIteratively)
  VISIT_NOP(x86::DchainRejuvenateIndex)
  VISIT_NOP(x86::VectorRead)
  VISIT_NOP(x86::VectorWrite)
  VISIT_NOP(x86::DchainAllocateNewIndex)
  VISIT_NOP(x86::MapPut)
  VISIT_NOP(x86::ChecksumUpdate)
  VISIT_NOP(x86::DchainIsIndexAllocated)
  VISIT_NOP(x86::SketchComputeHashes)
  VISIT_NOP(x86::SketchExpire)
  VISIT_NOP(x86::SketchFetch)
  VISIT_NOP(x86::SketchRefresh)
  VISIT_NOP(x86::SketchTouchBuckets)
  VISIT_NOP(x86::MapErase)
  VISIT_NOP(x86::DchainFreeIndex)
  VISIT_NOP(x86::ChtFindBackend)
  VISIT_NOP(x86::HashObj)

protected:
  virtual void log(const EPNode *ep_node) const;
};

} // namespace synapse
