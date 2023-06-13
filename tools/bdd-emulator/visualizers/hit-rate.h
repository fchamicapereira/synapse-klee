#pragma once

#include <assert.h>
#include <iomanip>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "../common.h"
#include "call-paths-to-bdd.h"

namespace BDD {

class HitRateGraphvizGenerator : public GraphvizGenerator {
private:
  bdd_hit_rate_t hit_rate;

public:
  HitRateGraphvizGenerator(std::ostream &os, const bdd_hit_rate_t &_hit_rate)
      : GraphvizGenerator(os), hit_rate(_hit_rate) {
    auto colors_per_node = get_colors_per_node(_hit_rate);
    set_node_colors(colors_per_node);
  }

  static void visualize(const BDD &bdd, const bdd_hit_rate_t &hit_rate,
                        bool interrupt = true) {
    bdd_visualizer_opts_t opts;

    opts.colors_per_node = get_colors_per_node(hit_rate);
    opts.default_color.first = true;
    opts.default_color.second = hit_rate_to_blue(0);

    GraphvizGenerator::visualize(bdd, opts, interrupt);
  }

private:
  static std::unordered_map<node_id_t, std::string>
  get_colors_per_node(const bdd_hit_rate_t &hit_rate) {
    std::unordered_map<node_id_t, std::string> colors_per_node;

    for (auto it = hit_rate.begin(); it != hit_rate.end(); it++) {
      auto node = it->first;
      auto node_hit_rate = it->second;
      auto color = hit_rate_to_blue(node_hit_rate);

      colors_per_node[node] = color;
    }

    return colors_per_node;
  }

  static std::string hit_rate_to_rainbow(emulation::hit_rate_t node_hit_rate) {
    auto blue = color_t(0, 0, 1);
    auto cyan = color_t(0, 1, 1);
    auto green = color_t(0, 1, 0);
    auto yellow = color_t(1, 1, 0);
    auto red = color_t(1, 0, 0);

    auto palette = std::vector<color_t>{blue, cyan, green, yellow, red};

    auto value = node_hit_rate * (palette.size() - 1);
    auto idx1 = (int)std::floor(value);
    auto idx2 = (int)idx1 + 1;
    auto frac = value - idx1;

    auto r =
        0xff * ((palette[idx2].r - palette[idx1].r) * frac + palette[idx1].r);
    auto g =
        0xff * ((palette[idx2].g - palette[idx1].g) * frac + palette[idx1].g);
    auto b =
        0xff * ((palette[idx2].b - palette[idx1].b) * frac + palette[idx1].b);

    auto color = color_t(r, g, b);
    return color.to_gv_repr();
  }

  static std::string hit_rate_to_blue(emulation::hit_rate_t node_hit_rate) {
    auto r = 0xff * (1 - node_hit_rate);
    auto g = 0xff * (1 - node_hit_rate);
    auto b = 0xff;
    auto o = 0xff * 0.5;

    auto color = color_t(r, g, b, o);
    return color.to_gv_repr();
  }
};
} // namespace BDD
