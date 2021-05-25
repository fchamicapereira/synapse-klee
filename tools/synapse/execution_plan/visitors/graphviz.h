#pragma once

#include "visitor.h"
#include "../execution_plan.h"
#include "../../log.h"

#include <ctime>
#include <unistd.h>

#include <fstream>

namespace synapse {

class Graphviz : public ExecutionPlanVisitor {
private:
  std::ofstream ofs;
  std::string   fpath;

  std::map<Target, std::string> node_colors;

  constexpr static int         fname_len = 15;
  constexpr static const char* prefix    = "/tmp/";
  constexpr static const char* alphanum  = "0123456789"
                                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                           "abcdefghijklmnopqrstuvwxyz";

  std::string get_rand_fname() {
    std::stringstream ss;
    static unsigned   counter = 1;

    ss << prefix;

    srand((unsigned) std::time(NULL) * getpid() + counter);

    for (int i = 0; i < fname_len; i++) {
      ss << alphanum[rand() % (strlen(alphanum) - 1)];
    }

    ss << ".gv";

    counter++;
    return ss.str();
  }

  void open() {
    std::string file_path = __FILE__;
    std::string dir_path  = file_path.substr(0, file_path.rfind("/"));
    std::string script    = "open_graph.sh";
    std::string cmd       = dir_path + "/" + script + " " + fpath;
    
    system(cmd.c_str());
  }

public:
  Graphviz(const std::string& path) : fpath(path) {
    node_colors = std::map<Target, std::string> {
      { Target::x86,       "cornflowerblue" },
      { Target::Tofino,    "darkolivegreen2" },
      { Target::Netronome, "gold" },
      { Target::FPGA,      "coral1" }
    };

    ofs.open(fpath);
    assert(ofs);
  }

private:
  Graphviz() : Graphviz(get_rand_fname()) {}

  void function_call(Target target, std::string label) {
    ofs << "[label=\"" << label << "\", ";
    ofs << "color=" << node_colors[target] << "];";
    ofs << "\n";
  }

public:
  static void visualize(const ExecutionPlan& ep) {
    if (ep.get_root()) {
      Graphviz gv;
      ep.visit(gv);
      gv.open();
    }
  }

  ~Graphviz() {
    ofs.close();
  }

  void visit(ExecutionPlan ep) override {
    ofs << "digraph mygraph {"                   << "\n";
    ofs << "  compound=true;"                    << "\n";
    ofs << "  node [shape=record,style=filled];" << "\n";

    ExecutionPlanVisitor::visit(ep);

    ofs << "}\n";
    ofs.flush();
  }

  void visit(const ExecutionPlanNode* ep_node) override {
    auto mod  = ep_node->get_module();
    auto next = ep_node->get_next();
    auto id   = ep_node->get_id();

    ofs << "  " << id << " ";
    ExecutionPlanVisitor::visit(ep_node);

    for (auto branch : next) {
      ofs << "  " << id << " -> " << branch->get_id() << ";" << "\n";
    }
  }

  /********************************************
   * 
   *                  x86
   * 
   ********************************************/

  void visit(const targets::x86::MapGet* node) override {
    function_call(node->get_target(), "map_get");
  }

  void visit(const targets::x86::CurrentTime* node) override {
    function_call(node->get_target(), "current_time");
  }

  void visit(const targets::x86::PacketBorrowNextChunk* node) override {
    function_call(node->get_target(), "packet_borrow_next_chunk");
  }

  void visit(const targets::x86::PacketReturnChunk* node) override {
    function_call(node->get_target(), "packet_return_chunk");
  }

  void visit(const targets::x86::IfThen* node) override {
    function_call(node->get_target(), "if");
  }

  void visit(const targets::x86::Else* node) override {
    function_call(node->get_target(), "else");
  }

  void visit(const targets::x86::Forward* node) override {
    function_call(node->get_target(), "forward");
  }

  void visit(const targets::x86::Broadcast* node) override {
    function_call(node->get_target(), "drop");
  }

  void visit(const targets::x86::Drop* node) override {
    function_call(node->get_target(), "broadcast");
  }

  /********************************************
   * 
   *                  Tofino
   * 
   ********************************************/

  void visit(const targets::tofino::A* node) override {
    function_call(node->get_target(), "Tables");
  }

  void visit(const targets::tofino::B* node) override {
    function_call(node->get_target(), "Registers");
  }
};

}
