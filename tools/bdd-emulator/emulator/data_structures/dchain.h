#pragma once

#include "../internals/internals.h"
#include "data_structure.h"

#include <unordered_map>
#include <assert.h>

namespace BDD {
namespace emulation {

#define DCHAIN_RESERVED (2)

class Dchain : public DataStructure {
public:
  Dchain(addr_t _obj, uint64_t index_range) : DataStructure(DCHAIN, _obj) {
    cells = (struct dchain_cell *)malloc(sizeof(struct dchain_cell) *
                                         (index_range + DCHAIN_RESERVED));
    timestamps = (time_ns_t *)malloc(sizeof(time_ns_t) * (index_range));

    impl_init(index_range);
  }

  uint32_t allocate_new_index(uint32_t &index_out, time_ns_t time) {
    uint32_t ret = impl_allocate_new_index(index_out);

    if (ret) {
      timestamps[index_out] = time;
    }

    return ret;
  }

  uint32_t rejuvenate_index(uint32_t index, time_ns_t time) {
    uint32_t ret = impl_rejuvenate_index(index);

    if (ret) {
      timestamps[index] = time;
    }

    return ret;
  }

  uint32_t expire_one_index(uint32_t &index_out, time_ns_t time) {
    uint32_t has_ind = impl_get_oldest_index(index_out);

    if (has_ind) {
      if (timestamps[index_out] < time) {
        uint32_t rez = impl_free_index(index_out);
        return rez;
      }
    }

    return 0;
  }

  uint32_t is_index_allocated(uint32_t index) {
    return impl_is_index_allocated(index);
  }

  uint32_t free_index(uint32_t index) { return impl_free_index(index); }

  ~Dchain() {
    free(cells);
    free(timestamps);
  }

private:
  struct dchain_cell {
    uint32_t prev;
    uint32_t next;
  };

  enum DCHAIN_ENUM {
    ALLOC_LIST_HEAD = 0,
    FREE_LIST_HEAD = 1,
    INDEX_SHIFT = DCHAIN_RESERVED
  };

  struct dchain_cell *cells;
  time_ns_t *timestamps;

  void impl_init(uint32_t size) {
    struct dchain_cell *al_head = cells + ALLOC_LIST_HEAD;
    al_head->prev = 0;
    al_head->next = 0;
    uint32_t i = INDEX_SHIFT;

    struct dchain_cell *fl_head = cells + FREE_LIST_HEAD;
    fl_head->next = i;
    fl_head->prev = fl_head->next;

    while (i < (size + INDEX_SHIFT - 1)) {
      struct dchain_cell *current = cells + i;
      current->next = i + 1;
      current->prev = current->next;

      ++i;
    }

    struct dchain_cell *last = cells + i;
    last->next = FREE_LIST_HEAD;
    last->prev = last->next;
  }

  uint32_t impl_allocate_new_index(uint32_t &index) {
    struct dchain_cell *fl_head = cells + FREE_LIST_HEAD;
    struct dchain_cell *al_head = cells + ALLOC_LIST_HEAD;
    uint32_t allocated = fl_head->next;
    if (allocated == FREE_LIST_HEAD) {
      return 0;
    }

    struct dchain_cell *allocp = cells + allocated;
    // Extract the link from the "empty" chain.
    fl_head->next = allocp->next;
    fl_head->prev = fl_head->next;

    // Add the link to the "new"-end "alloc" chain.
    allocp->next = ALLOC_LIST_HEAD;
    allocp->prev = al_head->prev;

    struct dchain_cell *alloc_head_prevp = cells + al_head->prev;
    alloc_head_prevp->next = allocated;
    al_head->prev = allocated;

    index = allocated - INDEX_SHIFT;

    return 1;
  }

  uint32_t impl_free_index(uint32_t index) {
    uint32_t freed = index + INDEX_SHIFT;

    struct dchain_cell *freedp = cells + freed;
    uint32_t freed_prev = freedp->prev;
    uint32_t freed_next = freedp->next;

    // The index is already free.
    if (freed_next == freed_prev) {
      if (freed_prev != ALLOC_LIST_HEAD) {
        return 0;
      }
    }

    struct dchain_cell *fr_head = cells + FREE_LIST_HEAD;
    struct dchain_cell *freed_prevp = cells + freed_prev;
    freed_prevp->next = freed_next;

    struct dchain_cell *freed_nextp = cells + freed_next;
    freed_nextp->prev = freed_prev;

    freedp->next = fr_head->next;
    freedp->prev = freedp->next;

    fr_head->next = freed;
    fr_head->prev = fr_head->next;

    return 1;
  }

  uint32_t impl_get_oldest_index(uint32_t &index) {
    struct dchain_cell *al_head = cells + ALLOC_LIST_HEAD;

    // No allocated indexes.
    if (al_head->next == ALLOC_LIST_HEAD) {
      return 0;
    }

    index = al_head->next - INDEX_SHIFT;

    return 1;
  }

  uint32_t impl_rejuvenate_index(uint32_t index) {
    uint32_t lifted = index + INDEX_SHIFT;

    struct dchain_cell *liftedp = cells + lifted;
    uint32_t lifted_next = liftedp->next;
    uint32_t lifted_prev = liftedp->prev;

    if (lifted_next == lifted_prev) {
      if (lifted_next != ALLOC_LIST_HEAD) {
        return 0;
      } else {
        return 1;
      }
    }

    struct dchain_cell *lifted_prevp = cells + lifted_prev;
    lifted_prevp->next = lifted_next;

    struct dchain_cell *lifted_nextp = cells + lifted_next;
    lifted_nextp->prev = lifted_prev;

    struct dchain_cell *al_head = cells + ALLOC_LIST_HEAD;
    uint32_t al_head_prev = al_head->prev;

    liftedp->next = ALLOC_LIST_HEAD;
    liftedp->prev = al_head_prev;

    struct dchain_cell *al_head_prevp = cells + al_head_prev;
    al_head_prevp->next = lifted;

    al_head->prev = lifted;
    return 1;
  }

  uint32_t impl_is_index_allocated(uint32_t index) {
    uint32_t lifted = index + INDEX_SHIFT;

    struct dchain_cell *liftedp = cells + lifted;
    uint32_t lifted_next = liftedp->next;
    uint32_t lifted_prev = liftedp->prev;

    if (lifted_next == lifted_prev) {
      if (lifted_next != ALLOC_LIST_HEAD) {
        return 0;
      } else {
        return 1;
      }
    } else {
      return 1;
    }
  }

public:
  static Dchain *cast(const DataStructureRef &ds) {
    assert(ds->get_type() == DataStructureType::DCHAIN);
    return static_cast<Dchain *>(ds.get());
  }
};

} // namespace emulation
} // namespace BDD