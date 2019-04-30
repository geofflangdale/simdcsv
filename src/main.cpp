#include <unistd.h> // for getopt

#include <iostream>
#include "common_defs.h"
#include "csv_defs.h"
#include "io_util.h"
#include "timing.h"

using namespace std;

int main(int argc, char * argv[]) {
  int c; 
  bool verbose = false;
  bool dump = false;
  int iterations = 10;
  bool squash_counters = false;

  while ((c = getopt(argc, argv, "vdi:s")) != -1){
    switch (c) {
    case 'v':
      verbose = true;
      break;
    case 'd':
      dump = true;
      break;
    case 'i':
      iterations = atoi(optarg);
      break;
    case 's':
      squash_counters = true;
      break;
    }
  }
  if (optind >= argc) {
    cerr << "Usage: " << argv[0] << " <csvfile>" << endl;
    exit(1);
  }

  const char *filename = argv[optind];
  if (optind + 1 < argc) {
    cerr << "warning: ignoring everything after " << argv[optind + 1] << endl;
  }

  if (verbose) {
    cout << "[verbose] loading " << filename << endl;
  }
  std::string_view p;
  try {
    p = get_corpus(filename, CSV_PADDING);
  } catch (const std::exception &e) { // caught by reference to base
    std::cout << "Could not load the file " << filename << std::endl;
    return EXIT_FAILURE;
  }
  if (verbose) {
    cout << "[verbose] loaded " << filename << " (" << p.size() << " bytes)" << endl;
  }

  vector<int> evts;
  evts.push_back(PERF_COUNT_HW_CPU_CYCLES);
  evts.push_back(PERF_COUNT_HW_INSTRUCTIONS);
  evts.push_back(PERF_COUNT_HW_BRANCH_MISSES);
  evts.push_back(PERF_COUNT_HW_CACHE_REFERENCES);
  evts.push_back(PERF_COUNT_HW_CACHE_MISSES);

  TimingAccumulator ta(2, evts);
  for (int i = 0; i < iterations; i++) {
    {
      TimingPhase p1(ta, 0);
    }
    {
      TimingPhase p2(ta, 1);
    }
  }
  ta.dump();

  if (verbose) {
    cout << "[verbose] done " << endl;
  }
  return 0;
}

