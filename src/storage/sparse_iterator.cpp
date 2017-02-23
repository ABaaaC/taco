#include "sparse_iterator.h"

#include "util/strings.h"

using namespace taco::ir;

namespace taco {
namespace storage {

SparseIterator::SparseIterator(std::string name, const Expr& tensor, int level,
                               Iterator previous) : IteratorImpl(previous) {
  this->tensor = tensor;
  this->level = level;

  std::string idxVarName = name + util::toString(tensor);
  ptrVar = Var::make(util::toString(tensor) + std::to_string(level+1)+"_ptr",
                     typeOf<int>(), false);
  idxVar = Var::make(idxVarName, typeOf<int>(), false);
}

bool SparseIterator::isRandomAccess() const {
  return false;
}

bool SparseIterator::isSequentialAccess() const {
  return true;
}

Expr SparseIterator::getPtrVar() const {
  return ptrVar;
}

Expr SparseIterator::getIdxVar() const {
  return idxVar;
}

Expr SparseIterator::getIteratorVar() const {
  return ptrVar;
}

Expr SparseIterator::begin() const {
  return Load::make(getPtrArr(), getParent().getPtrVar());
}

Expr SparseIterator::end() const {
  return Load::make(getPtrArr(), Add::make(getParent().getPtrVar(), 1));
}

Stmt SparseIterator::initDerivedVars() const {
  return VarAssign::make(getIdxVar(), Load::make(getIdxArr(), getPtrVar()));
}

ir::Stmt SparseIterator::storePtr() const {
  return Store::make(getPtrArr(),
                     Add::make(getParent().getPtrVar(), 1), getPtrVar());
}

ir::Stmt SparseIterator::storeIdx(ir::Expr idx) const {
  return Store::make(getIdxArr(), getPtrVar(), idx);
}

ir::Expr SparseIterator::getPtrArr() const {
  return GetProperty::make(tensor, TensorProperty::Pointer, level);
}

ir::Expr SparseIterator::getIdxArr() const {
  return GetProperty::make(tensor, TensorProperty::Index, level);
}

ir::Stmt SparseIterator::resizePtrStorage(ir::Expr size) const {
  return Allocate::make(getPtrArr(), size, true);
}

ir::Stmt SparseIterator::resizeIdxStorage(ir::Expr size) const {
  return Allocate::make(getIdxArr(), size, true);
}

}}
