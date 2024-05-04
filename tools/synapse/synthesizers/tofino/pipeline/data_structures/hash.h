#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../../../code_builder.h"
#include "../../constants.h"
#include "../../util.h"
#include "../domain/variable.h"

namespace synapse {
namespace synthesizer {
namespace tofino {

struct hash_t {
  std::string label;
  bits_t size;
  std::string algorithm;

  hash_t(bits_t _size, int i) : size(_size) {
    label = "hash" + std::to_string(i);

    switch (size) {
    case 8: {
      algorithm = "HashAlgorithm_t.CRC8";
    } break;
    case 16: {
      algorithm = "HashAlgorithm_t.CRC16";
    } break;
    case 32: {
      algorithm = "HashAlgorithm_t.CRC32";
    } break;
    case 64: {
      algorithm = "HashAlgorithm_t.CRC64";
    } break;
    default:
      assert(false && "Missing custom hashing algorithm");
      Log::err() << "Missing custom hashing algorithm for hash size " << size
                 << "\n";
      exit(1);
    }
  }

  std::string get_label() const { return label; }

  std::string get_value_label() const { return label + "_value"; }

  void synthesize_declaration(CodeBuilder &builder) const {
    builder.append_new_line();

    builder.indent();
    builder.append("Hash<");

    builder.append("bit<");
    builder.append(size);
    builder.append(">");

    builder.append(">(");
    builder.append(algorithm);
    builder.append(") ");
    builder.append(label);
    builder.append(";");
    builder.append_new_line();
  }

  void synthesize_apply(CodeBuilder &builder,
                        const std::vector<std::string> &inputs,
                        Variable out) const {
    builder.indent();

    builder.append(out.get_type());
    builder.append(" ");
    builder.append(out.get_label());
    builder.append(" = ");

    builder.append(label);
    builder.append(".get({");

    for (auto i = 0u; i < inputs.size(); i++) {
      if (i != 0)
        builder.append(", ");
      builder.append(inputs[i]);
    }

    builder.append("});");
    builder.append_new_line();
  }

  bool operator==(const hash_t &other) const { return label == other.label; }
  bool operator==(const std::string &other_label) const {
    return label == other_label;
  }

  struct HashHashing {
    size_t operator()(const hash_t &hash) const {
      return std::hash<std::string>()(hash.label);
    }
  };
};

typedef std::unordered_set<hash_t, hash_t::HashHashing> hashes_t;

} // namespace tofino
} // namespace synthesizer
} // namespace synapse