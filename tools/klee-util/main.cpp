#include "klee-util.h"

int main() {
  kutil::solver_toolbox.build();

  auto A1 = kutil::solver_toolbox.create_new_symbol("A", 64);
  std::cerr << "A1 " << kutil::expr_to_string(A1) << "\n";

  auto A2 = kutil::solver_toolbox.create_new_symbol("A", 64);
  std::cerr << "A2 " << kutil::expr_to_string(A2) << "\n";

  auto eq = kutil::solver_toolbox.exprBuilder->Eq(A1, A2);

  klee::ConstraintManager constraints;
  klee::Query sat_query(constraints, eq);

  bool result;
  bool success = kutil::solver_toolbox.solver->mustBeTrue(sat_query, result);
  assert(success);

  std::cerr << "Equal " << result << "\n";

  return 0;
}
