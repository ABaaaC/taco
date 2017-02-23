#ifndef TACO_LOWER_ITERATORS_H
#define TACO_LOWER_ITERATORS_H

#include "iteration_schedule.h"
#include "tensor_path.h"
#include "var.h"
#include "storage/iterator.h"

#include <map>

namespace taco {

namespace internal {
class Tensor;
}

namespace ir {
class Expr;
}

namespace lower {

/// Tracks the per-edge iterators of the tensor paths of an iteration schedule.
class Iterators {
public:
  Iterators(const IterationSchedule& schedule,
            const std::map<internal::Tensor,ir::Expr>& tensorVariables);

  /// Returns the iterator for the step.
  const storage::Iterator& getIterator(const TensorPathStep&) const;

  /// Returns the iterator of the next step in the path. It is an error to call
  /// this function for the last step in a path.
  const storage::Iterator& getNextIterator(const TensorPathStep&) const;

  /// Returns the root iterator, which iterates over [0,1>.
  const storage::Iterator& getRootIterator() const;

private:
  storage::Iterator root;
  std::map<TensorPathStep, storage::Iterator> iterators;
};

}}
#endif
