#pragma once

#include <vector>
#include <string>
#include <memory>
#include <unordered_set>
#include <unordered_map>

#include "call-paths-to-bdd.h"

#include "../parser/infrastructure.hpp"
#include "../models/nf.hpp"
#include "../bdd/builder.hpp"
#include "node.hpp"

namespace BDD {
	class BDD;
}

namespace Clone {
	class Node;

	using std::string;
	using std::vector;
	using std::shared_ptr;
	using std::unique_ptr;
	using std::unordered_set;
	using std::unordered_map;
	using std::shared_ptr;
	using BDD::Node_ptr;
	using BDD::Branch;
	using BDD::ReturnProcess;
	using BDD::GraphvizGenerator;

	typedef unordered_map<string, shared_ptr<const BDD::BDD>> BDDs;

	struct NodeTransition {
		unsigned input_port;
		NodePtr node;
		Node_ptr tail;

		NodeTransition(unsigned input_port, NodePtr node, Node_ptr tail)
			: input_port(input_port), node(node), tail(tail) {}
	};

	typedef shared_ptr<NodeTransition> NodeTransitionPtr;

	class Network : public Infrastructure {
	private:
		const NFMap nfs;

		NodeMap nodes;
		NodePtr source;

		shared_ptr<Builder> builder;

		string name;

		Network(DeviceMap &&devices, NFMap &&nfs, LinkList &&links, PortMap &&ports);

		void build_graph();
		void traverse(unsigned global_port, NodePtr origin, unsigned nf_port);
		void print_graph() const;
	public:
		~Network();

		static unique_ptr<Network> create(DeviceMap &&devices, NFMap &&nfs, LinkList &&links, PortMap &&ports);

		inline void set_name(const string &name) {
			this->name = name;
		}

		void consolidate();
		void print() const;
	};
}
