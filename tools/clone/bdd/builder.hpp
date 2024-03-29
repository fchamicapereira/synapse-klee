#pragma once

#include <memory>
#include <string>
#include <vector>
#include <deque>
#include <unordered_set>

#include "bdd/nodes/node.h"
#include "call-paths-to-bdd.h"

#include "../models/nf.hpp"

namespace Clone {
	using std::string;
	using std::deque;
	using std::unique_ptr;
	using std::shared_ptr;
	using std::unordered_set;
	using BDD::Node_ptr;

	typedef deque<Node_ptr> Tails;

	class Builder { 
	private:
		const unique_ptr<BDD::BDD> bdd;
		unordered_set<string> merged_inits;

		Node_ptr init_tail = nullptr;
		Node_ptr init_root = nullptr;
		Node_ptr process_root = nullptr;

		uint64_t counter = 1;
		Builder(unique_ptr<BDD::BDD> bdd);

		Tails clone_node(Node_ptr node, uint32_t input_port);
		void trim_node(Node_ptr curr, Node_ptr next);
	public:
		~Builder();

		static shared_ptr<Builder> create();

		bool is_init_empty() const;
		bool is_process_empty() const;

		void replace_with_drop(Node_ptr node);

		void add_root_branch(Node_ptr &root, unsigned port);
		void add_init_branch(unsigned port);
		void add_process_branch(unsigned port);

		void initialise_init(const shared_ptr<const BDD::BDD> &bdd);
		void initialise_process(const shared_ptr<const BDD::BDD> &bdd);

		void join_init(const NFPtr &nf);
		Tails join_process(const NFPtr &nf, unsigned input_port, const Node_ptr &tail);

		Node_ptr get_process_root() const;
		
		const std::unique_ptr<BDD::BDD>& get_bdd() const;
		void dump(std::string path);
	};
}
