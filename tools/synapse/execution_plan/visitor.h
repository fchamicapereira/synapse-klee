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
class VectorRegisterLookup;
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
  VISIT_TODO(tofino::VectorRegisterLookup)

  /*************************************
   *
   *            Tofino CPU
   *
   * **********************************/

  VISIT_TODO(tofino_cpu::Ignore)
  VISIT_TODO(tofino_cpu::ParseHeader)
  VISIT_TODO(tofino_cpu::ModifyHeader)
  VISIT_TODO(tofino_cpu::ChecksumUpdate)
  VISIT_TODO(tofino_cpu::If)
  VISIT_TODO(tofino_cpu::Then)
  VISIT_TODO(tofino_cpu::Else)
  VISIT_TODO(tofino_cpu::Forward)
  VISIT_TODO(tofino_cpu::Broadcast)
  VISIT_TODO(tofino_cpu::Drop)
  VISIT_TODO(tofino_cpu::SimpleTableLookup)
  VISIT_TODO(tofino_cpu::SimpleTableUpdate)
  VISIT_TODO(tofino_cpu::SimpleTableDelete)
  VISIT_TODO(tofino_cpu::DchainAllocateNewIndex)
  VISIT_TODO(tofino_cpu::DchainRejuvenateIndex)
  VISIT_TODO(tofino_cpu::DchainIsIndexAllocated)
  VISIT_TODO(tofino_cpu::DchainFreeIndex)
  VISIT_TODO(tofino_cpu::VectorRead)
  VISIT_TODO(tofino_cpu::VectorWrite)
  VISIT_TODO(tofino_cpu::MapGet)
  VISIT_TODO(tofino_cpu::MapPut)
  VISIT_TODO(tofino_cpu::MapErase)
  VISIT_TODO(tofino_cpu::ChtFindBackend)
  VISIT_TODO(tofino_cpu::HashObj)
  VISIT_TODO(tofino_cpu::SketchComputeHashes)
  VISIT_TODO(tofino_cpu::SketchExpire)
  VISIT_TODO(tofino_cpu::SketchFetch)
  VISIT_TODO(tofino_cpu::SketchRefresh)
  VISIT_TODO(tofino_cpu::SketchTouchBuckets)
  VISIT_TODO(tofino_cpu::VectorRegisterLookup)
  VISIT_TODO(tofino_cpu::VectorRegisterUpdate)

  /*************************************
   *
   *                x86
   *
   * **********************************/

  VISIT_TODO(x86::Ignore)
  VISIT_TODO(x86::If)
  VISIT_TODO(x86::Then)
  VISIT_TODO(x86::Else)
  VISIT_TODO(x86::Forward)
  VISIT_TODO(x86::Broadcast)
  VISIT_TODO(x86::Drop)
  VISIT_TODO(x86::ParseHeader)
  VISIT_TODO(x86::ModifyHeader)
  VISIT_TODO(x86::MapGet)
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
