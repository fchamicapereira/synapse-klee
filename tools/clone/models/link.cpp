#include "link.h"

#include "../pch.h"


namespace Clone {
	size_t Link::id_counter = 0;

	Link::Link(const string &node1, const unsigned port1, const string &node2, const unsigned port2): 
		id(id_counter++), node1(node1), port1(port1), node2(node2), port2(port2) {}

	Link::~Link() {}

	size_t Link::get_id() const {
		return this->id;
	}

	string Link::get_node1() const {
		return this->node1;
	}

	string Link::get_node2() const {
		return this->node2;
	}

	unsigned Link::get_port1() const {
		return this->port1;
	}

	unsigned Link::get_port2() const {
		return this->port2;
	}

	void Link::print() const {
		debug("Link ", this->id, " ", this->node1, ":", this->port1, " <--> ", this->node2, ":", this->port2);
	}
}

