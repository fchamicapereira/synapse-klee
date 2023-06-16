#include "internals.h"

#include <assert.h>
#include <termios.h>

#define KEY_ESC 0x1b
#define KEY_DEL 0x7f
#define KEY_BELL 0x07

#define VT100_ERASE "\33[2K\r"
#define VT100_UP_1_LINE "\33[1A"
#define VT100_UP_2_LINES "\33[2A"
#define VT100_UP_3_LINES "\33[3A"

#define REPORT_PACKET_PERIOD 1000

namespace BDD {
namespace emulation {

class Reporter {
private:
  const meta_t &meta;

  uint64_t num_packets;
  bool warmup_mode;
  uint64_t packet_counter;
  time_ns_t time;

  uint64_t next_report;

public:
  Reporter(const meta_t &_meta, bool _warmup_mode)
      : meta(_meta), num_packets(0), warmup_mode(_warmup_mode),
        packet_counter(0), time(0), next_report(0) {
    struct termios term;
    tcgetattr(fileno(stdin), &term);

    term.c_lflag &= ~ECHO;
    tcsetattr(fileno(stdin), 0, &term);
  }

  void set_num_packets(uint64_t _num_packets) {
    if (warmup_mode) {
      num_packets = 2 * _num_packets;
    } else {
      num_packets = _num_packets;
    }
  }

  void stop_warmup() { warmup_mode = false; }
  void inc_packet_counter() { packet_counter++; }
  void set_time(time_ns_t _time) { time = _time; }

  void show() {
    auto progress = (int)((100.0 * packet_counter) / num_packets);
    auto churn_fpm =
        (60.0 * meta.flows_expired) / (double)(meta.elapsed_time * 1e-9);
    auto percent_accepted = 100.0 * meta.accepted / meta.packet_counter;
    auto percent_rejected = 100.0 * meta.rejected / meta.packet_counter;

    if (packet_counter < next_report) {
      return;
    }

    if (next_report > 0) {
      for (auto i = 0; i < 13; i++)
        printf(VT100_UP_1_LINE);
    }

    printf(VT100_ERASE "Emulator data\n");
    printf(VT100_ERASE "  Progress      %d %%\n", progress);
    printf(VT100_ERASE "  Packets       %lu\n", packet_counter);
	printf(VT100_ERASE "  Total packets %lu\n", num_packets);
    printf(VT100_ERASE "  Time          %lu ns\n", time);
    printf(VT100_ERASE "  Warmup        %d\n", warmup_mode);

    printf(VT100_ERASE "Metadata\n");
    printf(VT100_ERASE "  Packets       %lu\n", meta.packet_counter);
    printf(VT100_ERASE "  Accepted      %lu (%.2f%%)\n", meta.accepted,
           percent_accepted);
    printf(VT100_ERASE "  Rejected      %lu (%.2f%%)\n", meta.rejected,
           percent_rejected);
    printf(VT100_ERASE "  Elapsed       %lu ns\n", meta.elapsed_time);
    printf(VT100_ERASE "  Expired       %lu (%f fpm)\n", meta.flows_expired,
           churn_fpm);
    printf(VT100_ERASE "  Dchain allocs %lu\n", meta.dchain_allocations);

    fflush(stdout);

    next_report = packet_counter + REPORT_PACKET_PERIOD;
  }
};

} // namespace emulation
} // namespace BDD