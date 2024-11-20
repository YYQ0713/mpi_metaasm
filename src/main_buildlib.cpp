#include "sequence/io/sequence_lib.h"
#include "utils/utils.h"
#include "common/common.hpp"


int main_build_lib(GlobalOptions& opt) {
  AutoMaxRssRecorder recorder;

  SequenceLibCollection::Build(opt.read_lib_path(), opt.read_lib_path());

  return 0;
}