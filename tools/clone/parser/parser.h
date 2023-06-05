#pragma once

#include <string>
#include <memory>

namespace Clone {
	using std::string;
	using std::unique_ptr;
	class Network;
	class Infrastructure;

	constexpr char STRING_DEVICE[] = "device";
	constexpr char STRING_NF[] =  "nf";
 	constexpr char STRING_LINK[] = "link";
	constexpr char STRING_PORT[] =  "port";

	constexpr size_t LENGTH_DEVICE_INPUT2 = 2;
	constexpr size_t LENGTH_DEVICE_INPUT3 = 3;
	constexpr size_t LENGTH_NF_INPUT = 3;
	constexpr size_t LENGTH_LINK_INPUT = 5;
	constexpr size_t LENGTH_PORT_INPUT = 4;

	/** 
	 * Parses a file containing the description of the network 
	 * 
	 * @param network_file the path to the file containing the network description
	 * @return a single Network object, representing the network described in the file
	*/
	unique_ptr<Network> parse_network(const string &input_file);
	unique_ptr<Infrastructure> parse_infrastructure(const string &input_file);
}