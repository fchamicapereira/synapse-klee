#pragma once

#include <string>
#include <vector>
#include <memory>

namespace Clone {
	using std::string;
	using std::unique_ptr;
	using std::vector;

	class Link {
	private:
		static size_t id_counter;

		const size_t id;
		const string node1;
		const unsigned port1;
		const string node2;
		const unsigned port2;

	public:
		Link(const string &node1, const unsigned port1, const string &node2, const unsigned port2);
		~Link();

		size_t get_id() const;
		string get_node1() const;
		string get_node2() const;
		unsigned get_port1() const;
		unsigned get_port2() const;

		void print() const;
	};

	typedef vector<unique_ptr<const Link>> LinkList;
	typedef unique_ptr<const Link> LinkPtr;
}
