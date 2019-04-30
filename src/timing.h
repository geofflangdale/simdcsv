#ifndef TIMING_H
#define TIMING_H

#include <asm/unistd.h>       // for __NR_perf_event_open
#include <linux/perf_event.h> // for perf event constants
#include <sys/ioctl.h>        // for ioctl
#include <unistd.h>           // for syscall

#include <cerrno>  // for errno
#include <cstring> // for memset
#include <stdexcept>
#include <vector>
#include <iostream>

class TimingAccumulator {
public:
  std::vector<uint64_t> results;
  std::vector<uint64_t> temp_result_vec; // reused rather than allocated in the middle of timing
  int num_phases;
  int num_events;
  int fd;
  bool working;

  explicit TimingAccumulator(int num_phases_in, std::vector<int> config_vec) 
    : num_phases(num_phases_in), fd(0), working(true) {
    perf_event_attr attribs;
    std::vector<uint64_t> ids;

    memset(&attribs, 0, sizeof(attribs));
    attribs.type = PERF_TYPE_HARDWARE; // for now
    attribs.size = sizeof(attribs);
    attribs.disabled = 1;
    attribs.exclude_kernel = 1;
    attribs.exclude_hv = 1;

    attribs.sample_period = 0;
    attribs.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
    const int pid = 0;  // the current process
    const int cpu = -1; // all CPUs
    const unsigned long flags = 0;

    int group = -1; // no group
    num_events = config_vec.size();
    uint32_t i = 0;
    for (auto config : config_vec) {
      attribs.config = config;
      fd = syscall(__NR_perf_event_open, &attribs, pid, cpu, group, flags);
      if (fd == -1) {
        report_error("perf_event_open");
      }
      ioctl(fd, PERF_EVENT_IOC_ID, &ids[i++]);
      if (group == -1) {
        group = fd;
      }
    }

    temp_result_vec.resize(num_events * 2 + 1);
    results.resize(num_phases*num_events, config_vec.size());
  }

  ~TimingAccumulator() {
    close(fd);
  }

  void report_error(const std::string &context) {
     if (working) {
       std::cerr << (context + ": " + std::string(strerror(errno))) << std::endl;
     }
     working = false;
  }

  void start(UNUSED int phase_number) {
    if (ioctl(fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP) == -1) {
      report_error("ioctl(PERF_EVENT_IOC_RESET)");
    }
    if (ioctl(fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP) == -1) {
      report_error("ioctl(PERF_EVENT_IOC_ENABLE)");
    }
  }

  void stop(int phase_number) {
    if (ioctl(fd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP) == -1) {
      report_error("ioctl(PERF_EVENT_IOC_DISABLE)");
    }

    if (read(fd, &temp_result_vec[0], temp_result_vec.size() * 8) == -1) {
      report_error("read");
    }
    // our actual results are in slots 1,3,5, ... of this structure
    // we really should be checking our ids obtained earlier to be safe
    for (uint32_t i = 1; i < temp_result_vec.size(); i += 2) {
      results[phase_number * num_events + i/2] += temp_result_vec[i];
    }
  }

  void dump() {
    for (int i = 0; i < num_phases; i++) {
      for (int j = 0; j < num_events; j++) {
          std::cout << results[i*num_events + j] << " ";
      }
      std::cout << "\n";
    }
  }
};

// a that is designed to start counting on coming into scope and stop counting, putting its results into
// an Accumulator, when leaving scope. Optional - can just use explicit start/stop
class TimingPhase {
public:
  TimingAccumulator & acc;
  int phase_number;

  TimingPhase(TimingAccumulator & acc_in, int phase_number_in) 
     : acc(acc_in), phase_number(phase_number_in) {
    acc.start(phase_number);
  }

  ~TimingPhase() {
    acc.stop(phase_number);
  }
};

