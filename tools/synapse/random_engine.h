#pragma once

#include <functional>
#include <random>

namespace synapse {

class RandomEngine {
private:
  // What a mouth full...
  // This is the product of the bind expression below.
  typedef std::_Bind<std::uniform_int_distribution<int>(std::mt19937)>
      Generator;

  unsigned rand_seed;
  std::mt19937 gen;
  std::uniform_int_distribution<int> random_dist;
  Generator generator;

public:
  RandomEngine(unsigned _rand_seed, int _min, int _max)
      : rand_seed(_rand_seed), gen(rand_seed), random_dist(_min, _max),
        generator(std::bind(random_dist, gen)) {}

  RandomEngine(const RandomEngine &) = delete;
  RandomEngine(RandomEngine &&) = delete;

  RandomEngine &operator=(const RandomEngine &) = delete;

  int generate() { return generator(); }
  unsigned get_seed() const { return rand_seed; }
};

} // namespace synapse