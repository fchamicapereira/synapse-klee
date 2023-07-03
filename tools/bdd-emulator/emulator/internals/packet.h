#pragma once

#include <iomanip>
#include <iostream>
#include <stdint.h>

namespace BDD {
namespace emulation {

struct pkt_t {
  const uint8_t *data;
  const uint8_t *base;
  uint32_t size;

  pkt_t(const uint8_t *_data, uint32_t _size)
      : data(_data), base(data), size(_size) {}

  pkt_t() : pkt_t(nullptr, 0) {}
};

inline std::ostream &operator<<(std::ostream &os, const pkt_t &pkt) {
  for (auto i = 0u; i < pkt.size; i++) {
    if (i % 8 == 0) {
      os << std::setw(4);
      os << std::setfill('0');
      os << i << ": ";
    }

    else if (i % 4 == 0) {
      os << " ";
    }

    os << std::hex;
    os << std::setw(2);
    os << std::setfill('0');
    os << (int)(pkt.base[i]) << " ";

    if ((i + 1) % 8 == 0) {
      os << "\n";
    }
  }

  os << std::resetiosflags(std::ios::showbase);

  return os;
}

} // namespace emulation
} // namespace BDD