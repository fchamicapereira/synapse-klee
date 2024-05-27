#pragma once

#include <assert.h>
#include <iomanip>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "bdd_visualizer.h"
#include "../bdd-analyzer-report.h"

namespace bdd {

typedef float hit_rate_t;

class HitRateGraphvizGenerator : public BDDVisualizer {
public:
  HitRateGraphvizGenerator(const std::string &fname,
                           const bdd_node_counters &counters)
      : BDDVisualizer() {
    opts.fname = fname;
    opts.colors_per_node = get_colors_per_node(counters);
    opts.default_color.first = true;
    opts.annotations_per_node = get_annocations_per_node(counters);
    opts.default_color.second = hit_rate_to_color(0);
  }

  static void visualize(const BDD &bdd, const bdd_node_counters &counters,
                        bool interrupt) {
    bdd_visualizer_opts_t opts;

    opts.colors_per_node = get_colors_per_node(counters);
    opts.default_color.first = true;
    opts.annotations_per_node = get_annocations_per_node(counters);
    opts.default_color.second = hit_rate_to_color(0);

    BDDVisualizer::visualize(bdd, interrupt, opts);
  }

private:
  static uint64_t get_total_counter(const bdd_node_counters &counters) {
    uint64_t total_counter = 0;
    for (auto it = counters.begin(); it != counters.end(); it++) {
      total_counter = std::max(total_counter, it->second);
    }
    return total_counter;
  }

  static std::unordered_map<node_id_t, std::string>
  get_annocations_per_node(const bdd_node_counters &counters) {
    uint64_t total_counter = get_total_counter(counters);
    std::unordered_map<node_id_t, std::string> annocations_per_node;

    for (auto it = counters.begin(); it != counters.end(); it++) {
      auto node = it->first;
      auto counter = it->second;
      auto node_hit_rate = (float)counter / total_counter;

      std::stringstream ss;
      ss << "Hit rate: " << std::fixed << node_hit_rate << " (" << counter
         << "/" << total_counter << ")";
      annocations_per_node[node] = ss.str();
    }

    return annocations_per_node;
  }

  static std::unordered_map<node_id_t, std::string>
  get_colors_per_node(const bdd_node_counters &counters) {
    uint64_t total_counter = get_total_counter(counters);
    std::unordered_map<node_id_t, std::string> colors_per_node;

    for (auto it = counters.begin(); it != counters.end(); it++) {
      node_id_t node = it->first;
      auto counter = it->second;
      float node_hit_rate = (float)counter / total_counter;
      std::string color = hit_rate_to_color(node_hit_rate);

      colors_per_node[node] = color;
    }

    return colors_per_node;
  }

  static std::string hit_rate_to_color(float node_hit_rate) {
    // std::string color = hit_rate_to_rainbow(node_hit_rate);
    // std::string color = hit_rate_to_blue(node_hit_rate);
    std::string color = hit_rate_to_blue_red_scale(node_hit_rate);
    return color;
  }

  static std::string hit_rate_to_rainbow(float node_hit_rate) {
    rgb_t blue(0, 0, 1);
    rgb_t cyan(0, 1, 1);
    rgb_t green(0, 1, 0);
    rgb_t yellow(1, 1, 0);
    rgb_t red(1, 0, 0);

    std::vector<rgb_t> palette{blue, cyan, green, yellow, red};

    float value = node_hit_rate * (palette.size() - 1);
    int idx1 = (int)std::floor(value);
    int idx2 = (int)idx1 + 1;
    float frac = value - idx1;

    uint8_t r =
        0xff * ((palette[idx2].r - palette[idx1].r) * frac + palette[idx1].r);
    uint8_t g =
        0xff * ((palette[idx2].g - palette[idx1].g) * frac + palette[idx1].g);
    uint8_t b =
        0xff * ((palette[idx2].b - palette[idx1].b) * frac + palette[idx1].b);

    rgb_t color(r, g, b);
    return color.to_gv_repr();
  }

  static std::string hit_rate_to_blue(float node_hit_rate) {
    uint8_t r = 0xff * (1 - node_hit_rate);
    uint8_t g = 0xff * (1 - node_hit_rate);
    uint8_t b = 0xff;
    uint8_t o = 0xff * 0.5;

    rgb_t color(r, g, b, o);
    return color.to_gv_repr();
  }

  static std::string hit_rate_to_blue_red_scale(float node_hit_rate) {
    uint8_t r = 0xff * node_hit_rate;
    uint8_t g = 0;
    uint8_t b = 0xff * (1 - node_hit_rate);
    uint8_t o = 0xff * 0.33;

    rgb_t color(r, g, b, o);
    return color.to_gv_repr();
  }
};

} // namespace bdd
