#include "builder.h"
#include "../pch.h"

#include "call-paths-to-bdd.h"
#include "../parser/infrastructure.h"
#include "concretize_symbols.h"
#include "exprs.h"
#include "load-call-paths.h"
#include "retrieve_symbols.h"
#include "solver_toolbox.h"


using namespace BDD;
using klee::ConstraintManager;
using kutil::solver_toolbox;
using kutil::get_symbols;


namespace Clone {

	/* Constructors and destructors */

	Builder::Builder(unique_ptr<BDD::BDD> bdd): bdd(move(bdd)) {
	}

	Builder::~Builder() = default;

	/* Private methods */

	void Builder::add_vigor_device_constraint(BDD::Call* node, unsigned input_port) {
		ConstraintManager cm = node->get_node_constraints();
		auto vigor_device = solver_toolbox.create_new_symbol("VIGOR_DEVICE", 32);
		auto extract = solver_toolbox.exprBuilder->Extract(vigor_device, 0, klee::Expr::Int32);
		auto port = solver_toolbox.exprBuilder->Constant(input_port, vigor_device->getWidth());
		auto eq = solver_toolbox.exprBuilder->Eq(extract, port);
		cm.addConstraint(eq);
		node->set_node_constraints(cm);
	}

	void Builder::add_vigor_device_constraint(BDD::Branch* node, unsigned input_port) {
		ConstraintManager cm = node->get_node_constraints();
		auto vigor_device = solver_toolbox.create_new_symbol("VIGOR_DEVICE", 32);
		auto port = solver_toolbox.exprBuilder->Constant(input_port, vigor_device->getWidth());
		auto eq = solver_toolbox.exprBuilder->Eq(vigor_device, port);
		cm.addConstraint(eq);
		node->set_node_constraints(cm);
	}

	void Builder::concretize_vigor_device(BDD::Call* node, unsigned input_port) {
		ConstraintManager cm = node->get_node_constraints();
		auto call = node->get_call();

		for(auto& arg: call.args) {
			auto expr = arg.second.expr;
			auto symbols = kutil::get_symbols(expr);
			if(symbols.count("VIGOR_DEVICE")) {
				arg.second.expr = kutil::ConcretizeSymbols("VIGOR_DEVICE", input_port).visit(expr);
				add_vigor_device_constraint(node, input_port);
			}

			auto in = arg.second.in;
			symbols = kutil::get_symbols(in);
			if(symbols.count("VIGOR_DEVICE")) {
				arg.second.in = kutil::ConcretizeSymbols("VIGOR_DEVICE", input_port).visit(in);
				add_vigor_device_constraint(node, input_port);
			}
		}

		node->set_call(call);
	}

	bool Builder::concretize_vigor_device(BDD::Branch* node, unsigned input_port) {
		auto condition = node->get_condition();
		auto symbols = kutil::get_symbols(condition);

		if(symbols.count("VIGOR_DEVICE")) {
			add_vigor_device_constraint(node, input_port);
			return true;
		}
		
		return false;
	}


	Tails Builder::clone_node(Node_ptr root, uint32_t input_port) {
		assert(root);

		stack<Node_ptr> s;

		Tails tails;
		s.push(root);

		while(!s.empty()) {
			auto curr = s.top();
			s.pop();

			switch(curr->get_type()) {
				case Node::NodeType::BRANCH: {
					Branch* branch { static_cast<Branch*>(curr.get()) };
					assert(branch->get_on_true());
					assert(branch->get_on_false());

					Node_ptr prev = branch->get_prev();
					Node_ptr next_true = branch->get_on_true()->clone();
					Node_ptr next_false = branch->get_on_false()->clone();
					branch->disconnect();
					branch->add_prev(prev);

					const auto &condition_symbols = get_symbols(branch->get_condition());
					concretize_vigor_device(branch, input_port);

					auto condition = branch->get_condition();
					auto constraints = branch->get_node_constraints();

					bool maybe_false = solver_toolbox.is_expr_maybe_false(constraints, condition, true);
					bool maybe_true = solver_toolbox.is_expr_maybe_true(constraints, condition, true);

					if(!maybe_true) {
						Infrastructure::trim_node(curr, next_false);
						s.push(next_false);
					}
					else if(!maybe_false) {
						Infrastructure::trim_node(curr, next_true);
						s.push(next_true);
					}
					else {
						branch->add_on_true(next_true);
						next_true->replace_prev(curr);
						s.push(next_true);

						branch->add_on_false(next_false);
						next_false->replace_prev(curr);
						s.push(next_false);

						auto condition = branch->get_condition();
						auto symbols = kutil::get_symbols(condition);

						if(symbols.count("VIGOR_DEVICE")) {
							kutil::ConcretizeSymbols concretizer("VIGOR_DEVICE", input_port);
							auto new_condition = concretizer.visit(branch->get_condition());
							branch->set_condition(new_condition);
						} 
					}
					break;
				}
				case Node::NodeType::CALL: {
					Call* call { static_cast<Call*>(curr.get()) };
					assert(call->get_next());

					concretize_vigor_device(call, input_port);

					Node_ptr next = call->get_next()->clone();
					call->replace_next(next);
					next->replace_prev(curr);
					s.push(next);

					break;
				}
				case Node::NodeType::RETURN_INIT: {
					ReturnInit* ret { static_cast<ReturnInit*>(curr.get()) };

					if(ret->get_return_value() == ReturnInit::ReturnType::SUCCESS) {
						init_tail = curr;
					}
					break;
				}
				case Node::NodeType::RETURN_PROCESS: {
					ReturnProcess* ret = static_cast<ReturnProcess*>(curr.get()) ;

					if(ret->get_return_operation() == ReturnProcess::Operation::FWD) {
						tails.push_back(curr);
					}

					break;
				}
				case Node::NodeType::RETURN_RAW: {
					break;
				}
			}
		}

		return tails;
	}

	/* Static methods */

	shared_ptr<Builder> Builder::create() {
		auto builder = new Builder(unique_ptr<BDD::BDD>(new BDD::BDD()));
		return shared_ptr<Builder>(move(builder));
	}

	/* Public methods */
	bool Builder::is_init_empty() const {
		return bdd->get_init() == nullptr;
	}

	bool Builder::is_process_empty() const {
		return bdd->get_process() == nullptr;
	}

	void Builder::replace_with_drop(Node_ptr node) {
		Node_ptr return_drop = Node_ptr(new ReturnProcess(
			counter++, 
			node, 
			{}, 
			0, 
			ReturnProcess::Operation::DROP));
		Infrastructure::trim_node(node, return_drop);
	}

	void Builder::add_root_branch(Node_ptr &root, unsigned input_port) {
		Node_ptr node = Infrastructure::get_if_vigor_device(input_port, counter);

		if(root != nullptr) {
			Branch* root_casted = static_cast<Branch*>(root.get());
			root_casted->replace_on_false(node);
			node->add_prev(root);
		}

		root = node;
		assert(root != nullptr);
	}

	//void Builder::add_init_branch(unsigned port) {
	//	add_root_branch(init_root, port);
	//	merged_inits.clear();
	//	Branch* branch = static_cast<Branch*>(init_root.get());
	//	init_tail = branch->get_on_true();
	//}
	
	void Builder::add_process_branch(unsigned port) {
		add_root_branch(process_root, port);
		if(is_process_empty()) {
			bdd->set_process(process_root);
		}
	}

	void Builder::join_init(const NFPtr &nf) {
		if(merged_inits.find(nf->get_id()) != merged_inits.end()) {
			return;
		}

		Node_ptr init_new = nf->get_bdd()->get_init()->clone(true);
		init_new->recursive_update_ids(counter);

		if(bdd->get_init() == nullptr) {
			if(init_root == nullptr) {
				init_root = init_new;
			}
			bdd->set_init(init_root);
		}

		if(init_tail != nullptr) {
			Infrastructure::trim_node(init_tail, init_new);
		}

 		clone_node(init_new, 0);
		merged_inits.insert(nf->get_id());
	}

	Tails Builder::join_process(const NFPtr &nf, unsigned port, const Node_ptr &tail) {
		assert(process_root != nullptr);

		auto root = nf->get_bdd()->get_process()->clone(true);
		root->recursive_update_ids(counter);
		Infrastructure::trim_node(tail, root);

		return clone_node(root, port);
	}

	void Builder::set_root(Node_ptr root) {
		assert(root != nullptr);
		bdd->set_process(root);
	}

	const unique_ptr<BDD::BDD>& Builder::get_bdd() const {
		return this->bdd;
	}

	Node_ptr Builder::get_metadrop() const {
		Node_ptr node = bdd->get_process();

		while(node->get_type() == BDD::Node::NodeType::BRANCH) {
			Branch* casted = static_cast<Branch*>(node.get());
			node = casted->get_on_false();
		}

		return node;
	}

	Node_ptr Builder::get_process_root() const {
		return this->process_root;
	}

	void Builder::dump(string path) {
		info("Dumping BDD to file \"", path, "\"");
		this->bdd->serialize(path);
	}
}
