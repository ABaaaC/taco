#ifndef TACO_IR_TAGS_H
#define TACO_IR_TAGS_H

namespace taco {
enum class PARALLEL_UNIT {
  NOT_PARALLEL, DEFAULT_UNIT, GPU_BLOCK, GPU_WARP, GPU_THREAD, CPU_THREAD, CPU_VECTOR, CPU_THREAD_GROUP_REDUCTION, GPU_BLOCK_REDUCTION, GPU_WARP_REDUCTION
};
extern const char *PARALLEL_UNIT_NAMES[];

enum class OUTPUT_RACE_STRATEGY {
  IGNORE_RACES, NO_RACES, ATOMICS, TEMPORARY, PARALLEL_REDUCTION
};
extern const char *OUTPUT_RACE_STRATEGY_NAMES[];
}

#endif //TACO_IR_TAGS_H
