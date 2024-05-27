#include "graphviz.h"

#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>

#include <assert.h>
#include <unistd.h>

constexpr const char *OPEN_GRAPH_SCRIPT = "open_graph.sh";

rgb_t::rgb_t(uint8_t _r, uint8_t _g, uint8_t _b, uint8_t _o)
    : r(_r), g(_g), b(_b), o(_o) {}

rgb_t::rgb_t(uint8_t _r, uint8_t _g, uint8_t _b) : rgb_t(_r, _g, _b, 0xff) {}

std::string rgb_t::to_gv_repr() const {
  std::stringstream ss;

  ss << "#";
  ss << std::hex;

  ss << std::setw(2);
  ss << std::setfill('0');
  ss << (int)r;

  ss << std::setw(2);
  ss << std::setfill('0');
  ss << (int)g;

  ss << std::setw(2);
  ss << std::setfill('0');
  ss << (int)b;

  ss << std::setw(2);
  ss << std::setfill('0');
  ss << (int)o;

  ss << std::dec;

  return ss.str();
}

static std::string random_dot() {
  char filename[] = "/tmp/XXXXXX";
  int fd = mkstemp(filename);
  assert(fd != -1 && "Failed to create temporary file");
  std::string fpath(filename);
  fpath += ".dot";
  return fpath;
}

Graphviz::Graphviz(const std::string &_fpath)
    : fpath(_fpath.empty() ? random_dot() : _fpath) {}

Graphviz::Graphviz() : fpath(random_dot()) {}

void Graphviz::write() const {
  std::ofstream file(fpath);
  assert(file.is_open() && "Unable to open file");
  file << ss.str();
  file.close();
}

void Graphviz::show(bool interrupt) const {
  // Flush first.
  write();

  std::stringstream cmd_builder;

  std::string current_dir = __FILE__;
  current_dir = current_dir.substr(0, current_dir.rfind("/"));

  cmd_builder << current_dir;
  cmd_builder << "/";
  cmd_builder << OPEN_GRAPH_SCRIPT;
  cmd_builder << " ";
  cmd_builder << fpath;

  int status = system(cmd_builder.str().c_str());
  if (status < 0) {
    std::cout << "Error: " << std::strerror(errno) << "\n";
    assert(false && "Failed to open graph.");
  }

  if (interrupt) {
    std::cout << "Press Enter to continue ";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
  }
}

static void find_and_replace(
    std::string &str,
    const std::vector<std::pair<std::string, std::string>> &replacements) {
  for (const std::pair<std::string, std::string> &replacement : replacements) {
    const std::string &before = replacement.first;
    const std::string &after = replacement.second;

    std::string::size_type n = 0;
    while ((n = str.find(before, n)) != std::string::npos) {
      str.replace(n, before.size(), after);
      n += after.size();
    }
  }
}

void Graphviz::sanitize_html_label(std::string &label) {
  find_and_replace(
      label, {
                 {"&", "&amp;"}, // Careful, this needs to be the first
                                 // one, otherwise we mess everything up. Notice
                                 // that all the others use ampersands.
                 {"{", "&#123;"},
                 {"}", "&#125;"},
                 {"[", "&#91;"},
                 {"]", "&#93;"},
                 {"<", "&lt;"},
                 {">", "&gt;"},
                 {"\n", "<br/>"},
             });
}
