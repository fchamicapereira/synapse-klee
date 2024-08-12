#pragma once

#include "bfs.h"
#include "dfs.h"
#include "random.h"
#include "least_reordered.h"
#include "max_switch_nodes.h"
#include "most_compact.h"
#include "gallium.h"
#include "max_throughput.h"

#define EXPLICIT_HEURISTIC_TEMPLATE_CLASS_INSTANTIATION(C)                     \
  template class C<BFSComparator>;                                             \
  template class C<DFSComparator>;                                             \
  template class C<GalliumComparator>;                                         \
  template class C<LeastReorderedComparator>;                                  \
  template class C<MaxSwitchNodesComparator>;                                  \
  template class C<MostCompactComparator>;                                     \
  template class C<MaxThroughputComparator>;                                   \
  template class C<RandomComparator>;
