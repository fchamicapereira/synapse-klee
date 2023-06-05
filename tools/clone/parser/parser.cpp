#include "parser.h"
#include "../pch.h"

#include "../models/device.h"
#include "../models/nf.h"
#include "../models/link.h"
#include "../models/port.h"
#include "../network/network.h"
#include "infrastructure.h"


namespace Clone {
	ifstream open_file(const string &path) {
		ifstream fstream;
		fstream.open(path);

		if(!fstream.is_open()) {
			danger("Could not open input file ",  path);
		}

		return fstream;
	}

	DevicePtr parse_device(const vector<string> &words) {
		if(words.size() != LENGTH_DEVICE_INPUT2 && words.size() != LENGTH_DEVICE_INPUT3) {
			danger("Invalid device at line ");
			exit(1);
		}

		if(words.size() == LENGTH_DEVICE_INPUT2) {
			const string id = words[1];
			return DevicePtr(new Device(id));
		}
		else if(words.size() == LENGTH_DEVICE_INPUT3) {
			const string type = words[1];
			const string id = words[2];

			return DevicePtr(new Device(id, type));
		}
		else {
			assert(false && "Shouldn't happen");
		}

		return nullptr;
	}

	NFPtr parse_nf(const vector<string> &words) {
		if(words.size() != LENGTH_NF_INPUT) {
			danger("Invalid network function at line ");
			exit(1);
		}

		const string id = words[1];
		const string path = words[2];

		return NFPtr(new NF(id, path));;
	}

	LinkPtr parse_link(const vector<string> &words) {
		if(words.size() != LENGTH_LINK_INPUT) {
			danger("Invalid link at line: ");
			exit(1);
		}

		const string node1 = words[1];
		const string sport1 = words[2];
		const unsigned port1 = stoul(sport1);
		const string node2 = words[3];
		const string sport2 = words[4];
		const unsigned port2 = stoul(sport2);

		return LinkPtr(new Link(node1, port1, node2, port2));
	}

	PortPtr parse_port(const vector<string> &words, DeviceMap &devices) {
		if(words.size() != LENGTH_PORT_INPUT) {
			danger("Invalid port at line: ");
			exit(1);
		}

		const unsigned global_port = stoul(words[1]);
		const string device_name = words[2];
		const unsigned device_port = stoul(words[3]);

		if (devices.find(device_name) == devices.end()) { 
			danger("Could not find device " + device_name + " at line: ");
			exit(1);
		}

		auto device = devices.at(device_name);
		device->add_port(device_port, global_port);

		return PortPtr(new Port(device, device_port, global_port));
	}

	unique_ptr<Network> parse_network(const string &network_file) {
		ifstream fstream = open_file(network_file);

		NFMap nfs;
		LinkList links;
		DeviceMap devices;
		PortMap ports;
		
		string line;
		
		while(getline(fstream, line)) {
			stringstream ss(line);
			vector<string> words;
			string word;

			while(ss >> word) {
				words.push_back(word);
			}

			if(words.size() == 0) {
				continue;
			}

			const string type = words[0];

			if(type == STRING_DEVICE) {
				auto device = parse_device(words);
				devices[device->get_id()] = move(device);
			} 
			else if(type == STRING_NF) {
				auto nf = parse_nf(words);	
				nfs[nf->get_id()] = move(nf);
			} 
			else if(type == STRING_LINK) {
				auto link = parse_link(words);
				links.push_back(move(link));
			}
			else if(type == STRING_PORT) {
				auto port = parse_port(words, devices);
				ports[port->get_global_port()] = move(port);
			}
			else {
				danger("Invalid line: ", line);
			}
		}

		return Network::create(move(devices), move(nfs), move(links), move(ports));
	}

	unique_ptr<Infrastructure> parse_infrastructure(const string &input_file) {
		ifstream fstream = open_file(input_file);

		LinkList links;
		DeviceMap devices;
		PortMap ports;
		
		string line;
		
		while(getline(fstream, line)) {
			stringstream ss(line);
			vector<string> words;
			string word;

			while(ss >> word) {
				words.push_back(word);
			}

			if(words.size() == 0) {
				continue;
			}

			const string type = words[0];

			if(type == STRING_DEVICE) {
				auto device = parse_device(words);
				devices[device->get_id()] = move(device);
			} 
			else if(type == STRING_LINK) {
				auto link = parse_link(words);
				links.push_back(move(link));
			}
			else if(type == STRING_PORT) {
				auto port = parse_port(words, devices);
				ports[port->get_global_port()] = move(port);
			}
			else {
				danger("Invalid line: ", line);
			}
		}

		return Infrastructure::create(move(devices), move(links), move(ports));
	}
}