#pragma once

#include <string>
#include <memory>

namespace Clone {
	using std::string;
	class Network;

	constexpr char STRING_DEVICE[] = "device";
	constexpr char STRING_NF[] =  "nf";
 	constexpr char STRING_LINK[] = "link";
	constexpr char STRING_PORT[] =  "port";

	constexpr size_t LENGTH_DEVICE_INPUT = 2;
	constexpr size_t LENGTH_NF_INPUT = 3;
	constexpr size_t LENGTH_LINK_INPUT = 5;
	constexpr size_t LENGTH_PORT_INPUT = 4;

	/** 
	 * Parses a file containing the description of the network 
	 * 
	 * @param network_file the path to the file containing the network description
	 * @return a single Network object, representing the network described in the file
	*/
	std::unique_ptr<Network> parse(const std::string &input_file);
}
