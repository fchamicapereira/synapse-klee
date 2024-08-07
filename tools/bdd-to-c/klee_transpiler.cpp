#include "klee_transpiler.h"
#include "call-paths-to-bdd.h"
#include "klee-util.h"

Type_ptr type_from_klee_expr(klee::ref<klee::Expr> expr,
                             bool force_byte_array) {
  auto width = expr->getWidth();
  assert(width != klee::Expr::InvalidWidth);
  return type_from_size(width, force_byte_array);
}

Type_ptr klee_width_to_type(klee::Expr::Width width) {
  assert(width != klee::Expr::InvalidWidth);
  return type_from_size(width);
}

Constant_ptr const_to_ast_expr(const klee::ref<klee::Expr> &e) {
  assert(!e.isNull());

  klee::ref<klee::Expr> expr = e;

  if (expr->getKind() != klee::Expr::Kind::Constant && e->getWidth() <= 64) {
    auto value = kutil::solver_toolbox.value_from_expr(expr);
    auto value_expr =
        kutil::solver_toolbox.exprBuilder->Constant(value, expr->getWidth());
    auto always_eq =
        kutil::solver_toolbox.are_exprs_always_equal(expr, value_expr);

    if (always_eq) {
      expr = value_expr;
    }
  }

  if (expr->getKind() != klee::Expr::Kind::Constant) {
    return nullptr;
  }

  auto *constant = static_cast<klee::ConstantExpr *>(expr.get());
  auto type = klee_width_to_type(constant->getWidth());

  auto constant_node = Constant::build(type);

  if (type->get_type_kind() == Type::TypeKind::ARRAY) {
    Array *array = static_cast<Array *>(type.get());

    for (unsigned int offset = 0; offset < array->get_n_elems(); offset++) {
      auto byte =
          kutil::solver_toolbox.exprBuilder->Extract(constant, offset * 8, 8);
      auto value = kutil::solver_toolbox.value_from_expr(byte);
      constant_node->set_value(value, offset);
    }
  } else {
    assert(type->get_size() <= 64);
    uint64_t value = constant->getZExtValue();
    constant_node->set_value(value);
  }

  return constant_node;
}

Expr_ptr ptr_to_int(Expr_ptr ptr) {
  if (ptr->get_type()->get_type_kind() == Type::TypeKind::POINTER) {
    auto zero = Constant::build(PrimitiveType::PrimitiveKind::INT, 0);
    auto int_type = PrimitiveType::build(PrimitiveType::PrimitiveKind::INT);
    auto casted = Cast::build(ptr, Pointer::build(int_type));
    ptr = Read::build(casted, int_type, zero);
  }

  return ptr;
}

Expr_ptr transpile(AST *ast, const klee::ref<klee::Expr> &e,
                   bool pointer_to_int) {
  Expr_ptr result = const_to_ast_expr(e);

  if (result) {
    return result;
  }

  result = ast->get_from_local(e);

  if (result && pointer_to_int) {
    result = ptr_to_int(result);
  }

  if (result) {
    return result;
  }

  KleeExprToASTNodeConverter converter(ast, pointer_to_int);
  converter.visit(e);

  result = converter.get_result();
  assert(result);

  auto simplified = result->simplify(ast);

  return simplified;
}

uint64_t get_first_concat_idx(const klee::ref<klee::Expr> &e) {
  assert(e->getKind() == klee::Expr::Kind::Concat);

  klee::ref<klee::Expr> curr_node = e;
  while (curr_node->getKind() == klee::Expr::Kind::Concat) {
    curr_node = curr_node->getKid(1);
  }

  assert(curr_node->getKind() == klee::Expr::Kind::Read);
  klee::ReadExpr *read = static_cast<klee::ReadExpr *>(curr_node.get());

  Expr_ptr idx = const_to_ast_expr(read->index);
  assert(idx->get_kind() == Node::NodeKind::CONSTANT);

  Constant *idx_const = static_cast<Constant *>(idx.get());
  return idx_const->get_value();
}

uint64_t get_last_concat_idx(const klee::ref<klee::Expr> &e) {
  assert(e->getKind() == klee::Expr::Kind::Concat);

  klee::ref<klee::Expr> left = e->getKid(0);

  assert(left->getKind() == klee::Expr::Kind::Read);
  klee::ReadExpr *read = static_cast<klee::ReadExpr *>(left.get());

  Expr_ptr idx = const_to_ast_expr(read->index);
  assert(idx->get_kind() == Node::NodeKind::CONSTANT);

  Constant *idx_const = static_cast<Constant *>(idx.get());
  return idx_const->get_value();
}

std::vector<Expr_ptr> build_and_fill_byte_array(AST *ast, Expr_ptr var,
                                                klee::ref<klee::Expr> expr) {
  std::vector<Expr_ptr> statements;

  auto bit_size = expr->getWidth();
  assert(bit_size % 8 == 0);

  Type_ptr byte_type =
      PrimitiveType::build(PrimitiveType::PrimitiveKind::UINT8_T);
  Type_ptr expr_type = Array::build(byte_type, bit_size / 8);

  for (unsigned int byte = 0; byte < bit_size / 8; byte++) {
    Constant_ptr byte_const =
        Constant::build(PrimitiveType::PrimitiveKind::UINT32_T, byte);
    Expr_ptr var_read = Read::build(var, byte_type, byte_const);

    auto extracted = kutil::solver_toolbox.exprBuilder->Extract(
        expr, byte * 8, klee::Expr::Int8);
    Expr_ptr val_read = transpile(ast, extracted);

    statements.push_back(Assignment::build(var_read, val_read));
  }

  return statements;
}

static bool same_exprs_byte(const klee::ref<klee::Expr> &e1,
                            const klee::ref<klee::Expr> &e2, unsigned byte) {
  assert(e1->getWidth() / 8 > byte);
  assert(e2->getWidth() / 8 > byte);

  auto e1_byte = kutil::solver_toolbox.exprBuilder->Extract(e1, byte * 8,
                                                            klee::Expr::Int8);
  auto e2_byte = kutil::solver_toolbox.exprBuilder->Extract(e2, byte * 8,
                                                            klee::Expr::Int8);

  return kutil::solver_toolbox.are_exprs_always_equal(e1_byte, e2_byte);
}

std::vector<Expr_ptr> copy_byte_by_byte(AST *ast, Expr_ptr variable,
                                        klee::ref<klee::Expr> before,
                                        klee::ref<klee::Expr> after) {
  std::vector<Expr_ptr> changes;

  for (unsigned int byte = 0; byte < after->getWidth() / 8; byte++) {
    if (same_exprs_byte(before, after, byte)) {
      continue;
    }

    Type_ptr byte_type =
        PrimitiveType::build(PrimitiveType::PrimitiveKind::UINT8_T);
    Constant_ptr byte_const =
        Constant::build(PrimitiveType::PrimitiveKind::UINT32_T, byte);
    Expr_ptr var_read = Read::build(variable, byte_type, byte_const);

    auto extracted = kutil::solver_toolbox.exprBuilder->Extract(
        after, byte * 8, klee::Expr::Int8);
    Expr_ptr val_read = transpile(ast, extracted);

    changes.push_back(Assignment::build(var_read, val_read));
  }

  return changes;
}

std::vector<Expr_ptr> apply_changes(AST *ast, klee::ref<klee::Expr> before,
                                    klee::ref<klee::Expr> after) {
  assert(before->getWidth() == after->getWidth());

  std::vector<Expr_ptr> changes;

  if (kutil::solver_toolbox.are_exprs_always_equal(before, after)) {
    return changes;
  }

  Expr_ptr variable = transpile(ast, before);
  Type_ptr var_type = variable->get_type();

  switch (var_type->get_type_kind()) {
  case Type::TypeKind::PRIMITIVE: {
    Expr_ptr after_expr = transpile(ast, after);
    Expr_ptr change = Assignment::build(variable, after_expr);
    changes.push_back(change->simplify(ast));
    return changes;
  }
  case Type::TypeKind::STRUCT: {
    Struct *s = static_cast<Struct *>(var_type.get());
    std::vector<Variable_ptr> fields = s->query_fields();

    unsigned int offset = 0;
    for (auto field : fields) {
      auto field_size = field->get_type()->get_size();

      auto e1_chunk = kutil::solver_toolbox.exprBuilder->Extract(before, offset,
                                                                 field_size);
      auto e2_chunk =
          kutil::solver_toolbox.exprBuilder->Extract(after, offset, field_size);

      bool eq =
          kutil::solver_toolbox.are_exprs_always_equal(e1_chunk, e2_chunk);

      if (!eq) {
        auto field_changes = apply_changes(ast, e1_chunk, e2_chunk);
        changes.insert(changes.end(), field_changes.begin(),
                       field_changes.end());
      }

      offset += field_size;
    }

    return changes;
  }
  case Type::TypeKind::POINTER: {
    Pointer *ptr = static_cast<Pointer *>(var_type.get());
    Type_ptr pointee_type = ptr->get_type();

    if (pointee_type->get_type_kind() != Type::TypeKind::PRIMITIVE ||
        pointee_type->get_size() != 8) {
      variable =
          Cast::build(variable, Pointer::build(PrimitiveType::build(
                                    PrimitiveType::PrimitiveKind::UINT8_T)));
      var_type = variable->get_type();
    }
    break;
  }
  case Type::TypeKind::ARRAY: {
    variable =
        Cast::build(variable, Pointer::build(PrimitiveType::build(
                                  PrimitiveType::PrimitiveKind::UINT8_T)));
    var_type = variable->get_type();
    break;
  }
  }

  std::vector<kutil::expr_group_t> groups = kutil::get_expr_groups(after);

  unsigned offset = 0;
  for (auto group_it = groups.rbegin(); group_it != groups.rend(); group_it++) {
    auto &group = *group_it;

    auto e1_chunk = kutil::solver_toolbox.exprBuilder->Extract(
        before, offset * 8, klee::Expr::Int8 * group.n_bytes);
    bool eq =
        kutil::solver_toolbox.are_exprs_always_equal(e1_chunk, group.expr);

    if (eq) {
      continue;
    }

    if (group.has_symbol) {
      assert(var_type->get_type_kind() == Type::TypeKind::POINTER);
      Pointer *ptr = static_cast<Pointer *>(var_type.get());
      Type_ptr base_type = ptr->get_type();

      assert(base_type->get_type_kind() == Type::TypeKind::PRIMITIVE &&
             base_type->get_size() == 8);

      auto group_expr = transpile(ast, group.expr);
      assert(group_expr);

      // hack
      if (group_expr->get_type()->get_type_kind() != Type::TypeKind::POINTER) {
        group_expr = Cast::build(AddressOf::build(group_expr),
                                 Pointer::build(PrimitiveType::build(
                                     PrimitiveType::PrimitiveKind::VOID)));
      }

      std::vector<ExpressionType_ptr> args{
          Add::build(
              variable,
              Constant::build(PrimitiveType::PrimitiveKind::UINT64_T, offset)),
          group_expr,
          Constant::build(PrimitiveType::PrimitiveKind::UINT64_T,
                          group.n_bytes),
      };

      Type_ptr ret_type =
          PrimitiveType::build(PrimitiveType::PrimitiveKind::VOID);
      FunctionCall_ptr fcall = FunctionCall::build("memcpy", args, ret_type);

      changes.push_back(fcall);
    } else {
      auto new_changes = copy_byte_by_byte(ast, variable, e1_chunk, group.expr);
      changes.insert(changes.end(), new_changes.begin(), new_changes.end());
    }

    offset += group.n_bytes;
  }

  return changes;
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitRead(const klee::ReadExpr &e) {
  klee::ref<klee::Expr> eref = const_cast<klee::ReadExpr *>(&e);

  Type_ptr type = klee_width_to_type(e.getWidth());
  Expr_ptr idx = transpile(ast, e.index, pointer_to_int);

  klee::UpdateList ul = e.updates;
  const klee::Array *root = ul.root;
  std::string symbol = root->name;

  if (symbol == "DEVICE") {
    symbol = "src_devices";
  } else if (symbol.find("next_time") != std::string::npos) {
    symbol = "now";
  } else if (symbol.find("data_len") != std::string::npos) {
    symbol = "pkt_len";
  } else if (symbol == "packet_chunks") {
    assert(idx->get_kind() == Node::NodeKind::CONSTANT);
    Constant *idx_const = static_cast<Constant *>(idx.get());

    AST::chunk_t chunk_info = ast->get_chunk_from_local(idx_const->get_value());
    Variable_ptr var = chunk_info.var;
    assert(var != nullptr);

    unsigned new_idx_value = idx_const->get_value() - chunk_info.start_index;

    PrimitiveType *p =
        static_cast<PrimitiveType *>(idx_const->get_type().get());
    Constant_ptr new_idx =
        Constant::build(p->get_primitive_kind(), new_idx_value);

    Read_ptr read = Read::build(var, type, new_idx);
    assert(read);

    save_result(read);
    return klee::ExprVisitor::Action::skipChildren();
  }

  symbol_width = std::make_pair(true, root->getSize() * 8);

  Expr_ptr var = ast->get_from_local(symbol);

  if (var == nullptr) {
    var = ast->get_from_local(eref);

    if (var == nullptr) {
      ast->dump_stack();

      std::cerr << "\n";
      std::cerr << "Variable with symbol '" << symbol << "' not found:"
                << "\n";
      std::cerr << kutil::expr_to_string(eref) << "\n";
      std::cerr << "\n";

      assert(false && "Variable not found");
    }
  }

  Read_ptr read = Read::build(var, type, idx);
  assert(read);

  save_result(read);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitSelect(const klee::SelectExpr &e) {
  assert(e.getNumKids() == 3);

  Expr_ptr cond = transpile(ast, e.getKid(0), pointer_to_int);
  assert(cond);

  Expr_ptr first = transpile(ast, e.getKid(1), pointer_to_int);
  assert(first);

  Expr_ptr second = transpile(ast, e.getKid(2), pointer_to_int);
  assert(second);

  Select_ptr select = Select::build(cond, first, second);

  save_result(select);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitConcat(const klee::ConcatExpr &e) {
  Expr_ptr left = transpile(ast, e.getKid(0), pointer_to_int);
  Expr_ptr right = transpile(ast, e.getKid(1), pointer_to_int);
  Type_ptr type = klee_width_to_type(e.getWidth());

  if (left->get_kind() == Node::NodeKind::CONSTANT &&
      left->get_type()->get_type_kind() == Type::TypeKind::PRIMITIVE) {
    Constant *constant = static_cast<Constant *>(left.get());
    auto value = constant->get_value();
    if (value == 0) {
      save_result(right);
      return klee::ExprVisitor::Action::skipChildren();
    }
  }

  Concat_ptr concat = Concat::build(left, right, type);

  kutil::SymbolRetriever retriever;
  retriever.visit(klee::ref<klee::Expr>(const_cast<klee::ConcatExpr *>(&e)));
  auto symbols = retriever.get_retrieved_strings();

  if (symbols.size() != 1) {
    save_result(concat);
    return klee::ExprVisitor::Action::skipChildren();
  }

  Expr_ptr simplified = concat->simplify(ast);

  save_result(simplified);
  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitExtract(const klee::ExtractExpr &e) {
  auto expr = e.getKid(0);
  auto offset_value = e.offset;
  auto size = e.width;

  assert(offset_value % 8 == 0);

  Type_ptr type = klee_width_to_type(e.getWidth());

  Expr_ptr ast_expr = transpile(ast, expr, pointer_to_int);
  assert(ast_expr);

  while (ast_expr->get_kind() == Node::NodeKind::CONCAT) {
    Concat *concat = static_cast<Concat *>(ast_expr.get());

    Expr_ptr left = concat->get_left();
    Expr_ptr right = concat->get_right();

    auto right_size = right->get_type()->get_size();
    auto left_size = left->get_type()->get_size();

    if (offset_value == right_size && size == left_size) {
      ast_expr = left;
      offset_value = 0;
      break;
    }

    if (offset_value == 0 && size == right_size) {
      ast_expr = right;
      break;
    }

    if (offset_value + size <= right_size) {
      ast_expr = right;
    } else if (offset_value >= right_size) {
      ast_expr = left;
      offset_value -= right_size;
    } else {
      break;
    }
  }

  if (ast_expr->get_kind() == Node::NodeKind::VARIABLE) {
    Expr_ptr offset = Constant::build(PrimitiveType::PrimitiveKind::UINT64_T,
                                      offset_value / 8);
    Read_ptr read = Read::build(ast_expr, type, offset);

    save_result(read);
    return klee::ExprVisitor::Action::skipChildren();
  }

  if (ast_expr->get_kind() == Node::NodeKind::CONSTANT) {
    Constant *constant = static_cast<Constant *>(ast_expr.get());
    switch (constant->get_type()->get_type_kind()) {
    case Type::TypeKind::PRIMITIVE: {
      Constant_ptr new_constant = Constant::build(type);
      new_constant->set_value((constant->get_value() >> (offset_value)) &
                              ((1 << size) - 1));

      save_result(new_constant);
      return klee::ExprVisitor::Action::skipChildren();
    }
    case Type::TypeKind::POINTER:
    case Type::TypeKind::STRUCT:
      assert(false && "Not implemented");
      break;
    case Type::TypeKind::ARRAY: {
      Array *array = static_cast<Array *>(constant->get_type().get());

      assert(offset_value % array->get_elem_type()->get_size() == 0);
      assert(size % array->get_elem_type()->get_size() == 0);

      unsigned int new_size = size / array->get_elem_type()->get_size();
      unsigned int old_idx = offset_value / array->get_elem_type()->get_size();
      unsigned int new_idx = 0;

      if (new_size == 1) {
        Constant_ptr new_constant = Constant::build(array->get_elem_type());
        new_constant->set_value(constant->get_value(old_idx));

        save_result(new_constant);
        return klee::ExprVisitor::Action::skipChildren();
      }

      Array_ptr new_array = Array::build(array->get_elem_type(), new_size);
      Constant_ptr new_constant = Constant::build(new_array);

      while (size > 0) {
        new_constant->set_value(constant->get_value(old_idx), new_idx);

        size -= array->get_elem_type()->get_size();
        new_idx++;
        old_idx++;
      }

      save_result(new_constant);
      return klee::ExprVisitor::Action::skipChildren();
    }
    }
  }

  Expr_ptr extract;

  if (offset_value > 0) {
    Expr_ptr mask = Constant::build(PrimitiveType::PrimitiveKind::UINT64_T,
                                    (1 << size) - 1, true);
    Expr_ptr offset =
        Constant::build(PrimitiveType::PrimitiveKind::UINT64_T, offset_value);
    ShiftRight_ptr shift = ShiftRight::build(ast_expr, offset);
    extract = And::build(shift, mask);
  } else {
    extract = ast_expr;
  }

  Cast_ptr cast = Cast::build(extract, type);
  save_result(cast);
  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitZExt(const klee::ZExtExpr &e) {
  assert(e.getNumKids() == 1);

  Type_ptr type = klee_width_to_type(e.getWidth());
  auto expr = e.getKid(0);

  Expr_ptr ast_expr = transpile(ast, expr, pointer_to_int);
  assert(ast_expr);

  if (type->get_size() > ast_expr->get_type()->get_size()) {
    save_result(ast_expr);
    return klee::ExprVisitor::Action::skipChildren();
  }

  Cast_ptr cast = Cast::build(ast_expr, type);

  save_result(cast);
  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitSExt(const klee::SExtExpr &e) {
  assert(e.getNumKids() == 1);

  Type_ptr type = klee_width_to_type(e.getWidth());
  auto expr = e.getKid(0);

  Expr_ptr ast_expr =
      SignedExpression::build(transpile(ast, expr, pointer_to_int));
  assert(ast_expr);

  if (type->get_size() > ast_expr->get_type()->get_size()) {
    save_result(ast_expr);
    return klee::ExprVisitor::Action::skipChildren();
  }

  Cast_ptr cast = Cast::build(ast_expr, type);

  save_result(cast);
  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitAdd(const klee::AddExpr &e) {
  assert(e.getNumKids() == 2);

  Expr_ptr left = transpile(ast, e.getKid(0), pointer_to_int);
  assert(left);

  Expr_ptr right = transpile(ast, e.getKid(1), pointer_to_int);
  assert(right);

  Add_ptr a = Add::build(left, right);
  save_result(a);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitSub(const klee::SubExpr &e) {
  assert(e.getNumKids() == 2);

  Expr_ptr left = transpile(ast, e.getKid(0), pointer_to_int);
  assert(left);

  Expr_ptr right = transpile(ast, e.getKid(1), pointer_to_int);
  assert(right);

  Sub_ptr s = Sub::build(left, right);
  save_result(s);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitMul(const klee::MulExpr &e) {
  assert(e.getNumKids() == 2);

  Expr_ptr left = transpile(ast, e.getKid(0), pointer_to_int);
  assert(left);

  Expr_ptr right = transpile(ast, e.getKid(1), pointer_to_int);
  assert(right);

  Mul_ptr m = Mul::build(left, right);
  save_result(m);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitUDiv(const klee::UDivExpr &e) {
  assert(e.getNumKids() == 2);

  Expr_ptr left = transpile(ast, e.getKid(0), pointer_to_int);
  assert(left);

  Expr_ptr right = transpile(ast, e.getKid(1), pointer_to_int);
  assert(right);

  Div_ptr d = Div::build(left, right);
  save_result(d);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitSDiv(const klee::SDivExpr &e) {
  assert(e.getNumKids() == 2);

  Expr_ptr left = transpile(ast, e.getKid(0), pointer_to_int);
  assert(left);

  Expr_ptr right = transpile(ast, e.getKid(1), pointer_to_int);
  assert(right);

  Cast_ptr cast = Cast::build(left, true);
  Div_ptr d = Div::build(cast, right);
  save_result(d);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitURem(const klee::URemExpr &e) {
  assert(e.getNumKids() == 2);

  Expr_ptr left = transpile(ast, e.getKid(0), pointer_to_int);
  assert(left);

  Expr_ptr right = transpile(ast, e.getKid(1), pointer_to_int);
  assert(right);

  Mod_ptr m = Mod::build(left, right);
  save_result(m);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitSRem(const klee::SRemExpr &e) {
  assert(e.getNumKids() == 2);

  Expr_ptr left = transpile(ast, e.getKid(0), pointer_to_int);
  assert(left);

  Expr_ptr right = transpile(ast, e.getKid(1), pointer_to_int);
  assert(right);

  Cast_ptr cast = Cast::build(left, true);
  Mod_ptr m = Mod::build(cast, right);

  save_result(m);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitNot(const klee::NotExpr &e) {
  assert(e.getNumKids() == 1);

  Expr_ptr arg = transpile(ast, e.getKid(0), pointer_to_int);
  save_result(Not::build(arg));

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitAnd(const klee::AndExpr &e) {
  assert(e.getNumKids() == 2);

  Expr_ptr left = transpile(ast, e.getKid(0), pointer_to_int);
  assert(left);

  Expr_ptr right = transpile(ast, e.getKid(1), pointer_to_int);
  assert(right);

  And_ptr a = And::build(left, right);
  save_result(a);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitOr(const klee::OrExpr &e) {
  assert(e.getNumKids() == 2);

  Expr_ptr left = transpile(ast, e.getKid(0), pointer_to_int);
  assert(left);

  Expr_ptr right = transpile(ast, e.getKid(1), pointer_to_int);
  assert(right);

  Or_ptr o = Or::build(left, right);
  save_result(o);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitXor(const klee::XorExpr &e) {
  assert(e.getNumKids() == 2);

  Expr_ptr left = transpile(ast, e.getKid(0), pointer_to_int);
  assert(left);

  Expr_ptr right = transpile(ast, e.getKid(1), pointer_to_int);
  assert(right);

  Xor_ptr x = Xor::build(left, right);
  save_result(x);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitShl(const klee::ShlExpr &e) {
  assert(e.getNumKids() == 2);

  Expr_ptr left = transpile(ast, e.getKid(0), pointer_to_int);
  assert(left);

  Expr_ptr right = transpile(ast, e.getKid(1), pointer_to_int);
  assert(right);

  ShiftLeft_ptr sl = ShiftLeft::build(left, right);
  save_result(sl);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitLShr(const klee::LShrExpr &e) {
  assert(e.getNumKids() == 2);

  Expr_ptr left = transpile(ast, e.getKid(0), pointer_to_int);
  assert(left);

  Expr_ptr right = transpile(ast, e.getKid(1), pointer_to_int);
  assert(right);

  ShiftRight_ptr sr = ShiftRight::build(left, right);
  save_result(sr);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitAShr(const klee::AShrExpr &e) {
  assert(e.getNumKids() == 2);

  Expr_ptr left = transpile(ast, e.getKid(0), pointer_to_int);
  assert(left);

  Expr_ptr right = transpile(ast, e.getKid(1), pointer_to_int);
  assert(right);

  Cast_ptr cast = Cast::build(left, true);
  ShiftRight_ptr sr = ShiftRight::build(cast, right);

  save_result(sr);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitEq(const klee::EqExpr &e) {
  assert(e.getNumKids() == 2);

  Expr_ptr left = transpile(ast, e.getKid(0), pointer_to_int);
  assert(left);

  Expr_ptr right = transpile(ast, e.getKid(1), pointer_to_int);
  assert(right);

  if (right->get_kind() == Node::NodeKind::EQUALS &&
      left->get_kind() == Node::NodeKind::CONSTANT) {

    Constant *left_const = static_cast<Constant *>(left.get());
    Equals *right_eq = static_cast<Equals *>(right.get());
    Expr_ptr right_eq_left = right_eq->get_lhs();

    if (right_eq_left->get_kind() == Node::NodeKind::CONSTANT) {
      Constant *right_eq_left_const =
          static_cast<Constant *>(right_eq_left.get());

      if (right_eq_left_const->get_value() == 0 &&
          left_const->get_value() == 0) {
        save_result(right_eq->get_rhs());
        return klee::ExprVisitor::Action::skipChildren();
      }
    }
  } else if (right->get_kind() == Node::NodeKind::VARIABLE &&
             left->get_kind() == Node::NodeKind::CONSTANT) {
    Type_ptr right_type = right->get_type();

    if (right_type->get_type_kind() == Type::TypeKind::ARRAY ||
        right_type->get_type_kind() == Type::TypeKind::POINTER) {
      assert(right_type->get_size() <= 64);
      Type_ptr new_type = Pointer::build(left->get_type());
      Expr_ptr cast = Cast::build(right, new_type);
      right = Read::build(cast);
    }
  }

  Equals_ptr equals = Equals::build(left, right);
  save_result(equals);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitNe(const klee::NeExpr &e) {
  assert(e.getNumKids() == 2);

  Expr_ptr left = transpile(ast, e.getKid(0), pointer_to_int);
  assert(left);

  Expr_ptr right = transpile(ast, e.getKid(1), pointer_to_int);
  assert(right);

  NotEquals_ptr ne = NotEquals::build(left, right);

  save_result(ne);
  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitUlt(const klee::UltExpr &e) {
  assert(e.getNumKids() == 2);

  Expr_ptr left = transpile(ast, e.getKid(0), pointer_to_int);
  assert(left);

  Expr_ptr right = transpile(ast, e.getKid(1), pointer_to_int);
  assert(right);

  Less_ptr lt = Less::build(left, right);

  save_result(lt);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitUle(const klee::UleExpr &e) {
  assert(e.getNumKids() == 2);

  Expr_ptr left = transpile(ast, e.getKid(0), pointer_to_int);
  assert(left);

  Expr_ptr right = transpile(ast, e.getKid(1), pointer_to_int);
  assert(right);

  // major hack!
  if (right->get_type()->get_type_kind() == Type::TypeKind::POINTER) {
    Pointer *right_ptr = static_cast<Pointer *>(right->get_type().get());
    auto type_pointed = right_ptr->get_type();
    auto left_type = left->get_type();
    auto left_type_ptr = Pointer::build(left_type);
    auto left_type_sz = Constant::build(PrimitiveType::PrimitiveKind::INT,
                                        left_type->get_size());
    auto zero = Constant::build(PrimitiveType::PrimitiveKind::INT, 0);

    right = Read::build(Cast::build(right, left_type_ptr), left_type, zero);
  }

  LessEq_ptr le = LessEq::build(left, right);

  save_result(le);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitUgt(const klee::UgtExpr &e) {
  assert(e.getNumKids() == 2);

  Expr_ptr left = transpile(ast, e.getKid(0), pointer_to_int);
  assert(left);

  Expr_ptr right = transpile(ast, e.getKid(1), pointer_to_int);
  assert(right);

  Greater_ptr gt = Greater::build(left, right);

  save_result(gt);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitUge(const klee::UgeExpr &e) {
  assert(e.getNumKids() == 2);

  Expr_ptr left = transpile(ast, e.getKid(0), pointer_to_int);
  assert(left);

  Expr_ptr right = transpile(ast, e.getKid(1), pointer_to_int);
  assert(right);

  GreaterEq_ptr ge = GreaterEq::build(left, right);

  save_result(ge);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitSlt(const klee::SltExpr &e) {
  assert(e.getNumKids() == 2);

  Expr_ptr left = transpile(ast, e.getKid(0), pointer_to_int);
  assert(left);

  Expr_ptr right = transpile(ast, e.getKid(1), pointer_to_int);
  assert(right);

  Cast_ptr lc = Cast::build(left, true);
  Cast_ptr rc = Cast::build(right, true);

  Less_ptr lt = Less::build(lc, rc);

  save_result(lt);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitSle(const klee::SleExpr &e) {
  assert(e.getNumKids() == 2);

  Expr_ptr left = transpile(ast, e.getKid(0), pointer_to_int);
  assert(left);

  Expr_ptr right = transpile(ast, e.getKid(1), pointer_to_int);
  assert(right);

  Cast_ptr lc = Cast::build(left, true);
  Cast_ptr rc = Cast::build(right, true);

  LessEq_ptr le = LessEq::build(lc, rc);

  save_result(le);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitSgt(const klee::SgtExpr &e) {
  assert(e.getNumKids() == 2);

  Expr_ptr left = transpile(ast, e.getKid(0), pointer_to_int);
  assert(left);

  Expr_ptr right = transpile(ast, e.getKid(1), pointer_to_int);
  assert(right);

  Cast_ptr lc = Cast::build(left, true);
  Cast_ptr rc = Cast::build(right, true);

  Greater_ptr gt = Greater::build(lc, rc);

  save_result(gt);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
KleeExprToASTNodeConverter::visitSge(const klee::SgeExpr &e) {
  assert(e.getNumKids() == 2);

  Expr_ptr left = transpile(ast, e.getKid(0), pointer_to_int);
  assert(left);

  Expr_ptr right = transpile(ast, e.getKid(1), pointer_to_int);
  assert(right);

  Cast_ptr lc = Cast::build(left, true);
  Cast_ptr rc = Cast::build(right, true);

  GreaterEq_ptr ge = GreaterEq::build(lc, rc);

  save_result(ge);

  return klee::ExprVisitor::Action::skipChildren();
}
