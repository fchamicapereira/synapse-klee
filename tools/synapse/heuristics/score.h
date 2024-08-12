#pragma once

#include <iostream>
#include <map>
#include <vector>

#include "../execution_plan/execution_plan.h"
#include "../execution_plan/node.h"
#include "../targets/targets.h"
#include "../random_engine.h"
#include "../log.h"

namespace synapse {

enum class ScoreCategory {
  SendToControllerNodes,
  Recirculations,
  ReorderedNodes,
  SwitchNodes,
  SwitchProgressionNodes,
  SwitchLeaves,
  Nodes,
  ControllerNodes,
  SwitchDataStructures,
  Depth,
  ConsecutiveObjectOperationsInSwitch,
  HasNextStatefulOperationInSwitch,
  ProcessedBDD,
  ProcessedBDDPercentage,
  Throughput,
  SpeculativeThroughput,
  Random,
};

enum class ScoreObjective { MIN, MAX };

class Score {
private:
  typedef int64_t (Score::*ComputerPtr)(const EP *ep) const;

  // Responsible for calculating the score value for a given ScoreCategory.
  std::map<ScoreCategory, ComputerPtr> computers;

  // The order of the elements in this vector matters.
  // It defines a lexicographic order.
  std::vector<std::pair<ScoreCategory, ScoreObjective>> categories;

  // The actual score values.
  std::vector<int64_t> values;

public:
  Score(const Score &score)
      : computers(score.computers), categories(score.categories),
        values(score.values) {}

  Score(const EP *ep,
        const std::vector<std::pair<ScoreCategory, ScoreObjective>>
            &categories_objectives)
      : computers({
            {
                ScoreCategory::SendToControllerNodes,
                &Score::get_nr_send_to_controller,
            },
            {
                ScoreCategory::Recirculations,
                &Score::get_nr_recirculations,
            },
            {
                ScoreCategory::ReorderedNodes,
                &Score::get_nr_reordered_nodes,
            },
            {
                ScoreCategory::Nodes,
                &Score::get_nr_nodes,
            },
            {
                ScoreCategory::SwitchNodes,
                &Score::get_nr_switch_nodes,
            },
            {
                ScoreCategory::SwitchProgressionNodes,
                &Score::get_nr_switch_progression_nodes,
            },
            {
                ScoreCategory::SwitchLeaves,
                &Score::get_nr_switch_leaves,
            },
            {
                ScoreCategory::ControllerNodes,
                &Score::get_nr_controller_nodes,
            },
            {
                ScoreCategory::SwitchDataStructures,
                &Score::get_nr_switch_data_structures,
            },
            {
                ScoreCategory::Depth,
                &Score::get_depth,
            },
            {
                ScoreCategory::ConsecutiveObjectOperationsInSwitch,
                &Score::next_op_same_obj_in_switch,
            },
            {
                ScoreCategory::HasNextStatefulOperationInSwitch,
                &Score::next_op_is_stateful_in_switch,
            },
            {
                ScoreCategory::ProcessedBDD,
                &Score::get_processed_bdd,
            },
            {
                ScoreCategory::ProcessedBDDPercentage,
                &Score::get_percentage_of_processed_bdd,
            },
            {
                ScoreCategory::Throughput,
                &Score::get_throughput_prediction,
            },
            {
                ScoreCategory::SpeculativeThroughput,
                &Score::get_throughput_speculation,
            },
            {
                ScoreCategory::Random,
                &Score::get_random,
            },
        }) {
    for (const auto &category_objective : categories_objectives) {
      ScoreCategory ScoreCategory = category_objective.first;
      ScoreObjective ScoreObjective = category_objective.second;

      add(ScoreCategory, ScoreObjective);

      int64_t value = compute(ep, ScoreCategory, ScoreObjective);
      values.push_back(value);
    }
  }

  const std::vector<int64_t> &get() const { return values; }

  inline bool operator<(const Score &other) {
    assert(values.size() == other.values.size());

    for (auto i = 0u; i < values.size(); i++) {
      int64_t this_score = values[i];
      int64_t other_score = other.values[i];

      if (this_score > other_score) {
        return false;
      }

      if (this_score < other_score) {
        return true;
      }
    }

    return false;
  }

  inline bool operator==(const Score &other) {
    assert(values.size() == other.values.size());

    for (auto i = 0u; i < values.size(); i++) {
      int64_t this_score = values[i];
      int64_t other_score = other.values[i];

      if (this_score != other_score) {
        return false;
      }
    }

    return true;
  }

  inline bool operator>(const Score &other) {
    return !((*this) < other) && !((*this) == other);
  }

  inline bool operator<=(const Score &other) { return !((*this) > other); }
  inline bool operator>=(const Score &other) { return !((*this) < other); }
  inline bool operator!=(const Score &other) { return !((*this) == other); }

  friend std::ostream &operator<<(std::ostream &os, const Score &dt);

private:
  int64_t compute(const EP *ep, ScoreCategory ScoreCategory,
                  ScoreObjective ScoreObjective) const {
    auto found_it = computers.find(ScoreCategory);
    assert(found_it != computers.end() &&
           "ScoreCategory not found in lookup table");

    ComputerPtr computer = found_it->second;
    int64_t value = (this->*computer)(ep);

    if (ScoreObjective == ScoreObjective::MIN) {
      value *= -1;
    }

    return value;
  }

  void add(ScoreCategory score_category, ScoreObjective score_objective) {
    auto found_it = std::find_if(
        categories.begin(), categories.end(),
        [&score_category](
            const std::pair<ScoreCategory, ScoreObjective> &saved) {
          return saved.first == score_category;
        });

    assert(found_it == categories.end() && "ScoreCategory already inserted");
    categories.emplace_back(score_category, score_objective);
  }

  std::vector<const EPNode *>
  get_nodes_with_type(const EP *ep, const std::vector<ModuleType> &types) const;

  int64_t get_nr_nodes(const EP *ep) const;
  int64_t get_depth(const EP *ep) const;
  int64_t get_nr_switch_nodes(const EP *ep) const;
  int64_t get_nr_switch_progression_nodes(const EP *ep) const;
  int64_t get_nr_controller_nodes(const EP *ep) const;
  int64_t get_nr_reordered_nodes(const EP *ep) const;
  int64_t get_nr_switch_leaves(const EP *ep) const;
  int64_t next_op_same_obj_in_switch(const EP *ep) const;
  int64_t next_op_is_stateful_in_switch(const EP *ep) const;
  int64_t get_processed_bdd(const EP *ep) const;
  int64_t get_percentage_of_processed_bdd(const EP *ep) const;
  int64_t get_nr_send_to_controller(const EP *ep) const;
  int64_t get_nr_switch_data_structures(const EP *ep) const;
  int64_t get_nr_recirculations(const EP *ep) const;
  int64_t get_throughput_prediction(const EP *ep) const;
  int64_t get_throughput_speculation(const EP *ep) const;
  int64_t get_random(const EP *ep) const;
};

std::ostream &operator<<(std::ostream &os, const Score &score);
std::ostream &operator<<(std::ostream &os, ScoreCategory score_category);

} // namespace synapse