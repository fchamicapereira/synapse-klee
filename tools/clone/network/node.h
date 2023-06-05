#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <cassert>


namespace Clone {
	using std::string;
	using std::unordered_map;
	using std::unordered_set;
	using std::shared_ptr;
	using std::pair;
	using std::make_pair;

	enum class NodeType {
		GLOBAL_PORT,
		NF
	};

	class Node {
	private:
		const string name;
		const NodeType node_type;

		unordered_map<unsigned, pair<unsigned, shared_ptr<Node>>> children; // Port -> (Port, Node)

	public:
		Node(const string &name, NodeType node_type);
		~Node();

		inline const std::string &get_name() const {
			return name;
		}

		inline NodeType get_node_type() const {
			return node_type;
		}

		inline const unordered_map<unsigned, pair<unsigned, shared_ptr<Node>>> &get_children() const {
			return children;
		}

		inline bool has_child(unsigned port) const {
			return children.find(port) != children.end();
		}

		inline const pair<unsigned, shared_ptr<Node>> &get_child(unsigned port) const {
			assert(children.find(port) != children.end());
			return children.at(port);
		}

		inline void add_child(unsigned port_from, unsigned port_to, const std::shared_ptr<Node> &node) {
			children[port_from] = make_pair(port_to, node);
		}

		void print() const;
	};

	typedef shared_ptr<Node> NodePtr;
	typedef unordered_set<NodePtr> NodeSet;
	typedef unordered_map<string, NodePtr> NodeMap;
}
