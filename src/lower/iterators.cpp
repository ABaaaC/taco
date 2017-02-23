#include "iterators.h"

#include <iostream>

#include "var.h"
#include "format.h"
#include "internal_tensor.h"
#include "error.h"
#include "ir/ir.h"
#include "util/collections.h"

using namespace std;
using namespace taco::ir;
using taco::internal::Tensor;

namespace taco {
namespace lower {

// class Iterators
Iterators::Iterators(const IterationSchedule& schedule,
                     const map<internal::Tensor,ir::Expr>& tensorVariables) {
  root = storage::Iterator::makeRoot();

  // Create an iterator for each path step
  for (auto& path : schedule.getTensorPaths()) {
    Tensor tensor = path.getTensor();
    ir::Expr tensorVar = tensorVariables.at(tensor);
    Format format = path.getTensor().getFormat();

    storage::Iterator parent = root;
    for (int i=0; i < (int)path.getSize(); ++i) {
      Level levelFormat = format.getLevels()[i];
      string name = path.getVariables()[i].getName();

      storage::Iterator iterator =
          storage::Iterator::make(name, tensorVar, i, levelFormat, parent,
                                  tensor);
      iassert(path.getStep(i).getStep() == i);
      iterators.insert({path.getStep(i), iterator});
      parent = iterator;
    }
  }

  // Create an iterator for the result path
  TensorPath resultPath = schedule.getResultTensorPath();
  if (resultPath.defined()) {
    Tensor tensor = resultPath.getTensor();
    ir::Expr tensorVar = tensorVariables.at(tensor);
    Format format = tensor.getFormat();

    storage::Iterator parent = root;
    for (int i=0; i < (int)format.getLevels().size(); ++i) {
      taco::Var var = tensor.getIndexVars()[i];
      Level levelFormat = format.getLevels()[i];
      string name = var.getName();
      storage::Iterator iterator =
      storage::Iterator::make(name, tensorVar, i, levelFormat, parent, tensor);
      iassert(resultPath.getStep(i).getStep() == i);
      iterators.insert({resultPath.getStep(i), iterator});
      parent = iterator;
    }
  }
}

const storage::Iterator&
Iterators::getIterator(const TensorPathStep& step) const {
  iassert(util::contains(iterators, step)) << "No iterator for step: " << step;
  return iterators.at(step);
}

const storage::Iterator&
Iterators::getNextIterator(const TensorPathStep& step) const {
  iassert(step.getStep() >= 0);
  iassert((size_t)step.getStep() < step.getPath().getSize());
  iassert(((size_t)step.getStep()+1) < step.getPath().getSize())
      << "The path " << step.getPath() << " has no next step after " << step;
  TensorPathStep nextStep = step.getPath().getStep(step.getStep()+1);
  iassert(util::contains(iterators, nextStep));
  return iterators.at(nextStep);
}

const storage::Iterator& Iterators::getRootIterator() const {
  return root;
}

}}
