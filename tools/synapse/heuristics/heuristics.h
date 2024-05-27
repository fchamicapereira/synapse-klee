#pragma once

#include "biggest.h"
#include "dfs.h"
#include "least_reordered.h"
#include "maximize_switch_nodes.h"
#include "most_compact.h"
#include "gallium.h"

#define EXPLICIT_HEURISTIC_TEMPLATE_CLASS_INSTANTIATION(C)                     \
  template class C<BiggestComparator>;                                         \
  template class C<DFSComparator>;                                             \
  template class C<GalliumComparator>;                                         \
  template class C<LeastReorderedComparator>;                                  \
  template class C<MaximizeSwitchNodesComparator>;                             \
  template class C<MostCompactComparator>;
