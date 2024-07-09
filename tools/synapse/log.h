#pragma once

#include <iostream>
#include <sstream>

#define DEBUG_PAUSE                                                            \
  {                                                                            \
    std::cout << "Press Enter to continue ";                                   \
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');        \
  }

namespace synapse {

namespace Colors {
typedef std::string Color;

const Color RESET = "\033[0m";
const Color BLACK = "\033[30m";
const Color RED = "\033[31m";
const Color RED_BRIGHT = "\u001b[31;1m";
const Color GREEN = "\033[32m";
const Color YELLOW = "\033[33m";
const Color BLUE = "\033[34m";
const Color MAGENTA = "\033[35m";
const Color CYAN = "\033[36m";
const Color WHITE = "";
const Color BOLD = "\033[1m";
} // namespace Colors

class Log {

public:
  enum Level { DEBUG = 0, LOG = 1, WARNING = 2, ERROR = 3 };

  std::ostream stream;

  static Level MINIMUM_LOG_LEVEL;
  static bool is_debug_active() {
    return Log::MINIMUM_LOG_LEVEL >= Level::DEBUG;
  }

  static Log log();
  static Log dbg();
  static Log wrn();
  static Log err();

private:
  Colors::Color color;
  Level level;

private:
  Log(const Level &_level) : stream(nullptr), level(_level) {
    switch (_level) {
    case Level::LOG:
      stream.rdbuf(std::cout.rdbuf());
      color = Colors::WHITE;
      break;
    case Level::DEBUG:
      stream.rdbuf(std::cerr.rdbuf());
      color = Colors::CYAN;
      break;
    case Level::WARNING:
      stream.rdbuf(std::cerr.rdbuf());
      color = Colors::RED;
      break;
    case Level::ERROR:
      stream.rdbuf(std::cerr.rdbuf());
      color = Colors::RED_BRIGHT;
      break;
    default:
      stream.rdbuf(std::cerr.rdbuf());
      color = Colors::WHITE;
    }
  }

  Log(const Log &log) : Log(log.level) {}

  template <typename T> friend Log &operator<<(Log &log, T &&t);
  template <typename T> friend Log &operator<<(Log &&log, T &&t);
};

template <typename T> Log &operator<<(Log &log, T &&t) {
  if (log.level < Log::MINIMUM_LOG_LEVEL)
    return log;

  log.stream << log.color;
  log.stream << std::forward<T>(t);
  log.stream << Colors::RESET;

  return log;
}

template <typename T> Log &operator<<(Log &&log, T &&t) {
  return log << std::forward<T>(t);
}
} // namespace synapse
