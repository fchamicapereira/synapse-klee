#pragma once

#include <functional>
#include <random>
#include <memory>
#include <assert.h>

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

  RandomEngine(unsigned _rand_seed, int _min, int _max)
      : rand_seed(_rand_seed), gen(rand_seed), random_dist(_min, _max),
        generator(std::bind(random_dist, gen)) {}

  RandomEngine(unsigned _rand_seed)
      : rand_seed(_rand_seed), gen(rand_seed), random_dist(0, INT32_MAX),
        generator(std::bind(random_dist, gen)) {}

  RandomEngine(const RandomEngine &) = delete;
  RandomEngine(RandomEngine &&) = delete;

  RandomEngine &operator=(const RandomEngine &) = delete;

  int generate_number() { return generator(); }
  unsigned get_seed() const { return rand_seed; }

private:
  static std::unique_ptr<RandomEngine> engine;

public:
  static void seed(unsigned rand_seed) {
    engine = std::unique_ptr<RandomEngine>(new RandomEngine(rand_seed));
  }

  static int generate() {
    assert(engine);
    return engine->generate_number();
  }
};

} // namespace synapse