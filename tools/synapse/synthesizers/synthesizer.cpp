#include "synthesizer.h"

#include <assert.h>
#include <fstream>

namespace synapse {

const char INDENTATION_UNIT = ' ';
constexpr int INDENTATION_MULTIPLIER = 4;

const char *const MARKER_AFFIX = "/*@{";
const char *const MARKER_SUFFIX = "}@*/";

static std::filesystem::path get_template_path(const char *template_name) {
  std::filesystem::path src_file = __FILE__;
  return src_file.parent_path() / "templates" / template_name;
}

static std::unordered_map<marker_t, code_builder_t>
get_builders(const std::unordered_map<marker_t, indent_t> &markers) {
  std::unordered_map<marker_t, code_builder_t> builders;
  for (const auto &[marker, lvl] : markers) {
    builders[marker] = code_builder_t(lvl);
  }
  return builders;
}

static bool find_markers_in_template(
    const std::filesystem::path &template_file,
    const std::unordered_map<marker_t, code_builder_t> &builders) {
  std::ifstream file(template_file);

  std::stringstream buffer;
  buffer << file.rdbuf();

  code_t template_str = buffer.str();
  for (const auto &kv : builders) {
    const marker_t &marker = MARKER_AFFIX + kv.first + MARKER_SUFFIX;
    if (template_str.find(marker) == std::string::npos) {
      return false;
    }
  }

  return true;
}

Synthesizer::Synthesizer(const char *template_fname,
                         const std::unordered_map<marker_t, indent_t> &markers,
                         const std::filesystem::path &_out_file)
    : template_file(get_template_path(template_fname)), out_file(_out_file),
      builders(get_builders(markers)) {
  assert(std::filesystem::exists(template_file) && "Template file not found.");
  assert(find_markers_in_template(template_file, builders) &&
         "Marker not found in template.");
}

code_builder_t::code_builder_t(indent_t _lvl) : lvl(_lvl) {}

void code_builder_t::inc() { lvl++; }

void code_builder_t::dec() { lvl--; }

void code_builder_t::indent() {
  stream << code_t(lvl * INDENTATION_MULTIPLIER, INDENTATION_UNIT);
}

code_t code_builder_t::dump() const { return stream.str(); }

code_builder_t &code_builder_t::operator<<(const code_t &code) {
  stream << code;
  return *this;
}

code_builder_t &code_builder_t::operator<<(int n) {
  stream << n;
  return *this;
}

code_builder_t &Synthesizer::get(const std::string &marker) {
  auto it = builders.find(marker);
  assert(it != builders.end() && "Marker not found.");
  return it->second;
}

void Synthesizer::dump() const {
  std::ifstream file(template_file);

  std::stringstream buffer;
  buffer << file.rdbuf();

  code_t template_str = buffer.str();

  for (const auto &[marker_label, builder] : builders) {
    marker_t marker = MARKER_AFFIX + marker_label + MARKER_SUFFIX;
    code_t code = builder.stream.str();

    std::cerr << "Marker: " << marker << std::endl;
    std::cerr << "Code:\n" << code << std::endl;

    size_t pos = template_str.find(marker);
    assert(pos != std::string::npos);

    template_str.replace(pos, marker.size(), code);
  }

  std::ofstream out(out_file);
  out << template_str;
}

void Synthesizer::dbg() const {
  for (const auto &[marker_label, builder] : builders) {
    marker_t marker = MARKER_AFFIX + marker_label + MARKER_SUFFIX;
    code_t code = builder.stream.str();

    std::cerr << "\n====================\n";
    std::cerr << "Marker: " << marker << std::endl;
    std::cerr << "Code:\n" << code << std::endl;
    std::cerr << "====================\n";
  }
}

} // namespace synapse