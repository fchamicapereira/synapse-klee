#include "log.h"

namespace synapse {

Log Log::log() { return Log(Log::Level::LOG); }
Log Log::dbg() { return Log(Log::Level::DEBUG); }
Log Log::wrn() { return Log(Log::Level::WARNING) << "[WARNING] "; }
Log Log::err() { return Log(Log::Level::ERROR) << "[ERROR] "; }

Log::Level Log::MINIMUM_LOG_LEVEL = Log::Level::WARNING;

} // namespace synapse
