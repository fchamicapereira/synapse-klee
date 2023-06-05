#include "llvm/Support/CommandLine.h"

#include "pch.h"

#include "parser/parser.h"
#include "network/network.h"

namespace {
	llvm::cl::OptionCategory CloNe("CloNe specific options");

	llvm::cl::opt<std::string>
		InputNetworkFile("in", llvm::cl::desc("Input file for Network deserialization."),
					llvm::cl::cat(CloNe));

	llvm::cl::opt<std::string>
   		Out("out", llvm::cl::desc("Name for the output BDD."),
        	llvm::cl::cat(CloNe));

}
void usage() {
	cout << "Usage: clone [input-file]" << endl;
}

int main(int argc, char **argv) {
	llvm::cl::ParseCommandLineOptions(argc, argv);

	assert(InputNetworkFile.size() != 0 && "Please provide an input file");

	auto network = Clone::parse_network(InputNetworkFile);

	string name = Out;
	if(Out.size() == 0) {
		name = "network";
	}
	network->set_name(name);

	network->consolidate();

	return EXIT_SUCCESS;
}
