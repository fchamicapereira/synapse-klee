#include "call-paths-groups.h"

#include "klee-util.h"

namespace bdd {

void CallPathsGroup::group_call_paths() {
  assert(call_paths.cps.size());

  for (call_path_t *cp : call_paths.cps) {
    on_true.cps.clear();
    on_false.cps.clear();

    if (cp->calls.size() == 0)
      continue;

    const call_t &call = cp->calls[0];

    for (call_path_t *cp : call_paths.cps) {
      if (cp->calls.size() && are_calls_equal(cp->calls[0], call)) {
        on_true.cps.push_back(cp);
        continue;
      }

      on_false.cps.push_back(cp);
    }

    // all calls are equal, no need do discriminate
    if (on_false.cps.size() == 0)
      return;

    constraint = find_discriminating_constraint();

    if (!constraint.isNull())
      return;
  }

  // no more calls
  if (on_true.cps.size() == 0 && on_false.cps.size() == 0) {
    on_true = call_paths;
    return;
  }

  // Last shot...
  for (unsigned i = 0; i < call_paths.cps.size(); i++) {
    on_true.cps.clear();
    on_false.cps.clear();

    for (unsigned j = 0; j < call_paths.cps.size(); j++) {
      call_path_t *call_path = call_paths.cps[j];

      if (i == j) {
        on_true.cps.push_back(call_path);
      } else {
        on_false.cps.push_back(call_path);
      }
    }

    constraint = find_discriminating_constraint();

    if (!constraint.isNull())
      return;
  }

  assert(false && "Could not group call paths");
}

bool CallPathsGroup::are_calls_equal(call_t c1, call_t c2) {
  if (c1.function_name != c2.function_name)
    return false;

  for (auto arg_name_value_pair : c1.args) {
    const std::string &arg_name = arg_name_value_pair.first;

    // exception: we don't care about 'p' differences
    if (arg_name == "p" || arg_name == "src_devices")
      continue;

    const arg_t &c1_arg = c1.args[arg_name];
    const arg_t &c2_arg = c2.args[arg_name];

    if (!c1_arg.out.isNull() &&
        !kutil::solver_toolbox.are_exprs_always_equal(c1_arg.in, c1_arg.out))
      continue;

    // comparison between modifications to the received packet
    if (!c1_arg.in.isNull() &&
        !kutil::solver_toolbox.are_exprs_always_equal(c1_arg.in, c2_arg.in))
      return false;

    if (c1_arg.in.isNull() &&
        !kutil::solver_toolbox.are_exprs_always_equal(c1_arg.expr, c2_arg.expr))
      return false;
  }

  return true;
}

klee::ref<klee::Expr> CallPathsGroup::find_discriminating_constraint() {
  assert(on_true.cps.size());

  auto possible_discriminating_constraints =
      get_possible_discriminating_constraints();

  for (auto constraint : possible_discriminating_constraints) {
    if (check_discriminating_constraint(constraint))
      return constraint;
  }

  return klee::ref<klee::Expr>();
}

std::vector<klee::ref<klee::Expr>>
CallPathsGroup::get_possible_discriminating_constraints() const {
  std::vector<klee::ref<klee::Expr>> possible_discriminating_constraints;
  assert(on_true.cps.size());

  for (auto constraint : on_true.cps[0]->constraints) {
    if (satisfies_constraint(on_true.cps, constraint))
      possible_discriminating_constraints.push_back(constraint);
  }

  return possible_discriminating_constraints;
}

bool CallPathsGroup::satisfies_constraint(
    std::vector<call_path_t *> call_paths,
    klee::ref<klee::Expr> constraint) const {
  for (const auto &call_path : call_paths) {
    if (!satisfies_constraint(call_path, constraint))
      return false;
  }
  return true;
}

bool CallPathsGroup::satisfies_constraint(
    call_path_t *call_path, klee::ref<klee::Expr> constraint) const {
  auto not_constraint = kutil::solver_toolbox.exprBuilder->Not(constraint);
  return kutil::solver_toolbox.is_expr_always_false(call_path->constraints,
                                                    not_constraint);
}

bool CallPathsGroup::satisfies_not_constraint(
    std::vector<call_path_t *> call_paths,
    klee::ref<klee::Expr> constraint) const {
  for (const auto &call_path : call_paths) {
    if (!satisfies_not_constraint(call_path, constraint))
      return false;
  }
  return true;
}

bool CallPathsGroup::satisfies_not_constraint(
    call_path_t *call_path, klee::ref<klee::Expr> constraint) const {
  auto not_constraint = kutil::solver_toolbox.exprBuilder->Not(constraint);
  return kutil::solver_toolbox.is_expr_always_true(call_path->constraints,
                                                   not_constraint);
}

bool CallPathsGroup::check_discriminating_constraint(
    klee::ref<klee::Expr> constraint) {
  assert(on_true.cps.size());
  assert(on_false.cps.size());

  call_paths_t _on_true = on_true;
  call_paths_t _on_false;

  for (call_path_t *call_path : on_false.cps) {
    if (satisfies_constraint(call_path, constraint)) {
      _on_true.cps.push_back(call_path);
    } else {
      _on_false.cps.push_back(call_path);
    }
  }

  if (_on_false.cps.size() &&
      satisfies_not_constraint(_on_false.cps, constraint)) {
    on_true = _on_true;
    on_false = _on_false;
    return true;
  }

  return false;
}

} // namespace bdd