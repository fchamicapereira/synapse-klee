#include "klee-util.h"

using namespace kutil;

int main() {
  auto A1 = solver_toolbox.create_new_symbol("A", 64);
  std::cerr << "A1 " << expr_to_string(A1) << "\n";

  auto A2 = solver_toolbox.create_new_symbol("A", 64);
  std::cerr << "A2 " << expr_to_string(A2) << "\n";

  auto eq = solver_toolbox.exprBuilder->Eq(A1, A2);

  klee::ConstraintManager constraints;
  klee::Query sat_query(constraints, eq);

  bool result;
  bool success = solver_toolbox.solver->mustBeTrue(sat_query, result);
  assert(success);

  std::cerr << "Equal " << result << "\n";

  return 0;
}
