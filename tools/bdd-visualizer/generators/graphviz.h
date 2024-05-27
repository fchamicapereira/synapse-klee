#pragma once

#include <iomanip>
#include <sstream>
#include <stdint.h>

struct rgb_t {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t o;

  rgb_t(uint8_t r, uint8_t g, uint8_t b);
  rgb_t(uint8_t r, uint8_t g, uint8_t b, uint8_t o);

  std::string to_gv_repr() const;
};

class Graphviz {
protected:
  std::string fpath;
  std::stringstream ss;

public:
  Graphviz(const std::string &fpath);
  Graphviz();

  void write() const;
  void show(bool interrupt) const;

  static void sanitize_html_label(std::string &label);
};
