#include "network.h"
#include "../pch.h"

#include "bdd/nodes/branch.h"
#include "bdd/nodes/node.h"
#include "bdd/utils.h"
#include "call-paths-to-bdd.h"

#include "klee/util/ArrayExprHash.h"
#include "node.h"
#include <cmath>

namespace Clone {
	/* Constructors and destructors */
	Network::Network(DeviceMap &&devices, NFMap &&nfs, LinkList &&links, PortMap &&ports):
		Infrastructure(move(devices), move(links), move(ports)), nfs(move(nfs)) {}

	Network::~Network() = default;

	/* Private methods */
	void Network::build_graph() {
		for(const auto &link: links) {
			const string &node1_str = link->get_node1();
			const string &node2_str = link->get_node2();

			const NodeType node1_type = nfs.find(node1_str) != nfs.end() ? NodeType::NF : NodeType::GLOBAL_PORT;
			const NodeType node2_type = nfs.find(node2_str) != nfs.end() ? NodeType::NF : NodeType::GLOBAL_PORT;

			const unsigned port1 = link->get_port1();
			const unsigned port2 = link->get_port2();

			if(nodes.find(node1_str) == nodes.end()) {
				nodes.emplace(node1_str, NodePtr(new Node(node1_str, node1_type)));
			}

			if(nodes.find(node2_str) == nodes.end()) {
				nodes.emplace(node2_str, NodePtr(new Node(node2_str, node2_type)));
			}

			const NodePtr node1 = nodes.at(node1_str);
			const NodePtr node2 = nodes.at(node2_str);

			node1->add_child(port1, port2, node2);
			
			if(node1_type == NodeType::GLOBAL_PORT && source == nullptr) {
				source =  node1;
			}
		}

		if(source == nullptr) {
			danger("Null source");
		}
	}

	void Network::traverse(unsigned global_port, NodePtr source, unsigned nf_port) {
		/* Input port | Node */
		assert(nfs.find(source->get_name()) != nfs.end());
		const NFPtr nf = nfs.at(source->get_name());
		
		//builder->add_init_branch(global_port);
		builder->add_process_branch(global_port);

		deque<NodeTransitionPtr> q_transitions;
		const Node_ptr root = builder->get_process_root();
		const Branch* branch = static_cast<Branch*>(root.get());
		const NodeTransitionPtr transition = make_shared<NodeTransition>(nf_port, source, branch->get_on_true());
		q_transitions.push_front(transition);

		while(!q_transitions.empty()) {
			const NodeTransitionPtr transition = q_transitions.front();
			const unsigned port = transition->input_port;
			const NodePtr node = transition->node;
			const Node_ptr tail = transition->tail;
			const NFPtr nf = nfs.at(node->get_name());
			assert(nf != nullptr);

			q_transitions.pop_front();
			builder->join_init(nf);
			Tails tails = builder->join_process(nf, port, tail);

			while(!tails.empty()) {
				const Node_ptr tail = tails.front();
				tails.pop_front();
				ReturnProcess* return_process = static_cast<ReturnProcess*>(tail.get());
				unsigned port_next = return_process->get_return_value();

				if(node->has_child(port_next)) {
					const pair<unsigned, NodePtr> &child = node->get_child(port_next);
					const unsigned port = child.first;
					const NodePtr node = child.second;

					if(node->get_node_type() == NodeType::NF) {
						q_transitions.push_front(make_shared<NodeTransition>(port, node, tail));
					} 
					else {
						return_process->set_return_value(port);
					}
				} 
				else {
					builder->replace_with_drop(tail);
				}
			}
		}
	}

	void Network::print_graph() const {
		for(auto &node: nodes) {
			node.second->print();
		}
	}

	/* Static methods */
 	unique_ptr<Network> Network::create(DeviceMap &&devices, NFMap &&nfs, LinkList &&links, PortMap &&ports) {
		if(devices.size() == 0) danger("No devices found");
		if(nfs.size() == 0)	danger("No nfs found");
		if(links.size() == 0) danger("No links found");

		BDDs bdds;

		for (auto it = nfs.begin(); it != nfs.end(); ++it) {
			auto &nf { it->second };

			const string &path { nf->get_path() };

			if(bdds.find(path) == bdds.end()) {
				bdds.emplace(path, shared_ptr<BDD::BDD>(new BDD::BDD(path)));
			}

			nf->set_bdd(bdds.at(path));
		}
		
		return unique_ptr<Network>(new Network(move(devices), move(nfs), move(links), move(ports)));
	}

	unordered_map<string, vector<BDD::Node_ptr>> Network::get_device_mapping(BDD::Node_ptr node) const {
		unordered_map<string, vector<BDD::Node_ptr>> m;
		while(node->get_type() == BDD::Node::NodeType::BRANCH) {
			auto casted = BDD::cast_node<BDD::Branch>(node);
			unsigned port_value = extract_port(casted);

			auto port = ports.at(port_value);
			const string& name = port->get_device()->get_id();
			if(m.find(name) == m.end()) {
				m.emplace(name, vector<BDD::Node_ptr>());
			}
			m.at(name).push_back(node);

			node = casted->get_on_false();
		}

		return m;
	}

	void Network::reorder_roots() {
		auto m = get_device_mapping(builder->get_bdd()->get_process());
		BDD::Node_ptr drop = builder->get_metadrop();
		BDD::Node_ptr node = nullptr;
		BDD::Node_ptr prev = nullptr;
		BDD::Node_ptr on_true = nullptr;
		for(auto p: m) {
			for(auto n: p.second) {
				if(node == nullptr) {
					builder->set_root(n);
				}
				Branch* branch = static_cast<Branch*>(n.get());
				prev = node;
				node = n;
				on_true = branch->get_on_true();

				node->disconnect();

				if(prev != nullptr) {
					node->add_prev(prev);
					Branch* casted = static_cast<Branch*>(prev.get());
					casted->add_on_false(node);
				}

				node->add_next(on_true);
			}
		}
		
		assert(node != nullptr);
		Branch* branch = static_cast<Branch*>(node.get());
		branch->add_on_false(drop);
		drop->disconnect();
		drop->add_prev(node);
	}

	void Network::consolidate() {
		build_graph();

		builder = Builder::create();

		assert(source != nullptr);
		for(const auto &child: source->get_children()) {
			const unsigned global_port = child.first;
			info("Traversing from global port ", global_port, " to ", child.second.second->get_name(), " port ", child.second.first);
			traverse(global_port, child.second.second, child.second.first);
		}

		reorder_roots();

		GraphvizGenerator::visualize(*builder->get_bdd(), true, false);
		builder->dump(name + ".bdd");
	}

	void Network::print() const {
		debug("Printing Network");

		for (auto it = devices.begin(); it != devices.end(); ++it) {
			it->second->print();
		}

		for (auto it = nfs.begin(); it != nfs.end(); ++it) {
			it->second->print();
		}

		for (const auto& link : links) {
			link->print();
		}
	}
}
