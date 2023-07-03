#pragma once

#include "klee/util/ExprHashMap.h"
#include <cassert>
#include <string>
#include <map>
#include <memory>

namespace Clone {
	using std::string;
	using std::shared_ptr;
	using std::map;
	using std::pair;
	using std::make_pair;
	using std::vector;
	
	class Device {
	private:
		const string id;
		const string type;

    // local port, global port
		map<unsigned, unsigned> ports;
    
    // device id, outgoing port, incoming port
		map<string, vector<pair<unsigned, unsigned>>> neighbours;
	public:
		Device(string id, string type = "");
		~Device();

		inline const string& get_id() const {
			return id;
		}

		inline const string& get_type() const {
			return type;
		}

		inline unsigned get_port(const unsigned local_port) const {
			assert(ports.find(local_port) != ports.end());
			return ports.at(local_port);
		}

		inline void add_port(const unsigned local_port, const unsigned global_port) {
			ports[local_port] = global_port;
		}

		inline void add_neighbour(string device, unsigned outgoing, unsigned incoming) {
			if(neighbours.count(device) == 0) {
				neighbours[device] = vector<pair<unsigned, unsigned>>();
			}
			neighbours[device].push_back(make_pair(outgoing, incoming));
		}
		
		// outgoing port, incoming port
		inline const pair<unsigned, unsigned>& get_neighbour(std::string neighbour) {
			assert(neighbours.count(neighbour) > 0);
			assert(neighbours[neighbour].size() > 0);
			return neighbours[neighbour].front();
		}

		inline const map<string, vector<pair<unsigned, unsigned>>>& get_neighbours() const {
			return neighbours;
		}

		void print() const;
	};
	
	typedef shared_ptr<Device> DevicePtr;
	typedef map<string, DevicePtr> DeviceMap;
}
