#pragma once

#include <assert.h>
#include <iomanip>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "bdd-visualizer.h"
#include "../profiler.h"

namespace synapse {

typedef double hit_rate_t;

class ProfilerVisualizer : public bdd::BDDVisualizer {
public:
  static void visualize(const bdd::BDD *bdd, const Profiler *profiler,
                        bool interrupt) {
    std::unordered_map<bdd::node_id_t, double> fractions_per_node =
        get_fractions_per_node(bdd, profiler);

    bdd::bdd_visualizer_opts_t opts;

    opts.colors_per_node = get_colors_per_node(fractions_per_node);
    opts.default_color.first = true;
    opts.annotations_per_node = get_annocations_per_node(fractions_per_node);
    opts.default_color.second = fraction_to_color(0);

    bdd::BDDVisualizer::visualize(bdd, interrupt, opts);
  }

private:
  static std::unordered_map<bdd::node_id_t, double>
  get_fractions_per_node(const bdd::BDD *bdd, const Profiler *profiler) {
    std::unordered_map<bdd::node_id_t, double> fractions_per_node;
    const bdd::Node *root = bdd->get_root();

    root->visit_nodes([&fractions_per_node, profiler](const bdd::Node *node) {
      constraints_t constraints = node->get_ordered_branch_constraints();
      std::optional<double> fraction = profiler->get_fraction(constraints);
      assert(fraction.has_value());
      fractions_per_node[node->get_id()] = fraction.value();
      return bdd::NodeVisitAction::VISIT_CHILDREN;
    });

    return fractions_per_node;
  }

  static std::unordered_map<bdd::node_id_t, std::string>
  get_annocations_per_node(
      const std::unordered_map<bdd::node_id_t, double> &fraction_per_node) {
    std::unordered_map<bdd::node_id_t, std::string> annocations_per_node;

    for (const auto &[node, fraction] : fraction_per_node) {
      std::string color = fraction_to_color(fraction);
      std::stringstream ss;
      ss << "HR: " << std::fixed << fraction;
      annocations_per_node[node] = ss.str();
    }

    return annocations_per_node;
  }

  static std::unordered_map<bdd::node_id_t, std::string> get_colors_per_node(
      const std::unordered_map<bdd::node_id_t, double> &fraction_per_node) {
    std::unordered_map<bdd::node_id_t, std::string> colors_per_node;

    for (const auto &[node, fraction] : fraction_per_node) {
      std::string color = fraction_to_color(fraction);
      colors_per_node[node] = color;
    }

    return colors_per_node;
  }

  static std::string fraction_to_color(double fraction) {
    // std::string color = hit_rate_to_rainbow(fraction);
    // std::string color = hit_rate_to_blue(fraction);
    std::string color = hit_rate_to_blue_red_scale(fraction);
    return color;
  }

  static std::string hit_rate_to_rainbow(double fraction) {
    rgb_t blue(0, 0, 1);
    rgb_t cyan(0, 1, 1);
    rgb_t green(0, 1, 0);
    rgb_t yellow(1, 1, 0);
    rgb_t red(1, 0, 0);

    std::vector<rgb_t> palette{blue, cyan, green, yellow, red};

    double value = fraction * (palette.size() - 1);
    int idx1 = (int)std::floor(value);
    int idx2 = (int)idx1 + 1;
    double frac = value - idx1;

    uint8_t r =
        0xff * ((palette[idx2].r - palette[idx1].r) * frac + palette[idx1].r);
    uint8_t g =
        0xff * ((palette[idx2].g - palette[idx1].g) * frac + palette[idx1].g);
    uint8_t b =
        0xff * ((palette[idx2].b - palette[idx1].b) * frac + palette[idx1].b);

    rgb_t color(r, g, b);
    return color.to_gv_repr();
  }

  static std::string hit_rate_to_blue(double fraction) {
    uint8_t r = 0xff * (1 - fraction);
    uint8_t g = 0xff * (1 - fraction);
    uint8_t b = 0xff;
    uint8_t o = 0xff * 0.5;

    rgb_t color(r, g, b, o);
    return color.to_gv_repr();
  }

  static std::string hit_rate_to_blue_red_scale(double fraction) {
    uint8_t r = 0xff * fraction;
    uint8_t g = 0;
    uint8_t b = 0xff * (1 - fraction);
    uint8_t o = 0xff * 0.33;

    rgb_t color(r, g, b, o);
    return color.to_gv_repr();
  }
};

} // namespace synapse
