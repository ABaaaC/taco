#include "taco/index_notation/index_notation.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>
#include <utility>
#include <set>
#include <taco/ir/simplify.h>
#include "lower/mode_access.h"

#include "error/error_checks.h"
#include "taco/error/error_messages.h"
#include "taco/type.h"
#include "taco/format.h"

#include "taco/index_notation/intrinsic.h"
#include "taco/index_notation/schedule.h"
#include "taco/index_notation/transformations.h"
#include "taco/index_notation/index_notation_nodes.h"
#include "taco/index_notation/index_notation_rewriter.h"
#include "taco/index_notation/index_notation_printer.h"
#include "taco/ir/ir.h"
#include "taco/lower/lower.h"
#include "taco/codegen/module.h"

#include "taco/util/name_generator.h"
#include "taco/util/scopedmap.h"
#include "taco/util/strings.h"
#include "taco/util/collections.h"

using namespace std;

namespace taco {

// class IndexExpr
IndexExpr::IndexExpr(TensorVar var) : IndexExpr(new AccessNode(var,{})) {
}

IndexExpr::IndexExpr(char val) : IndexExpr(new LiteralNode(val)) {
}

IndexExpr::IndexExpr(int8_t val) : IndexExpr(new LiteralNode(val)) {
}

IndexExpr::IndexExpr(int16_t val) : IndexExpr(new LiteralNode(val)) {
}

IndexExpr::IndexExpr(int32_t val) : IndexExpr(new LiteralNode(val)) {
}

IndexExpr::IndexExpr(int64_t val) : IndexExpr(new LiteralNode(val)) {
}

IndexExpr::IndexExpr(uint8_t val) : IndexExpr(new LiteralNode(val)) {
}

IndexExpr::IndexExpr(uint16_t val) : IndexExpr(new LiteralNode(val)) {
}

IndexExpr::IndexExpr(uint32_t val) : IndexExpr(new LiteralNode(val)) {
}

IndexExpr::IndexExpr(uint64_t val) : IndexExpr(new LiteralNode(val)) {
}

IndexExpr::IndexExpr(float val) : IndexExpr(new LiteralNode(val)) {
}

IndexExpr::IndexExpr(double val) : IndexExpr(new LiteralNode(val)) {
}

IndexExpr::IndexExpr(std::complex<float> val) :IndexExpr(new LiteralNode(val)){
}

IndexExpr::IndexExpr(std::complex<double> val) :IndexExpr(new LiteralNode(val)){
}

Datatype IndexExpr::getDataType() const {
  return const_cast<IndexExprNode*>(this->ptr)->getDataType();
}

void IndexExpr::workspace(IndexVar i, IndexVar iw, std::string name) {
//  const_cast<IndexExprNode*>(this->ptr)->splitOperator(i, i, iw);
}

void IndexExpr::workspace(IndexVar i, IndexVar iw, Format format, string name) {
//  const_cast<IndexExprNode*>(this->ptr)->splitOperator(i, i, iw);
}

void IndexExpr::workspace(IndexVar i, IndexVar iw, TensorVar workspace) {
//  const_cast<IndexExprNode*>(this->ptr)->splitOperator(i, i, iw);
//  const_cast<IndexExprNode*>(this->ptr)->workspace(i, iw, workspace);
  this->ptr->setWorkspace(i, iw, workspace);
}

void IndexExpr::accept(IndexExprVisitorStrict *v) const {
  ptr->accept(v);
}

std::ostream& operator<<(std::ostream& os, const IndexExpr& expr) {
  if (!expr.defined()) return os << "IndexExpr()";
  IndexNotationPrinter printer(os);
  printer.print(expr);
  return os;
}

struct Equals : public IndexNotationVisitorStrict {
  bool eq = false;
  IndexExpr bExpr;
  IndexStmt bStmt;

  bool check(IndexExpr a, IndexExpr b) {
    this->bExpr = b;
    a.accept(this);
    return eq;
  }

  bool check(IndexStmt a, IndexStmt b) {
    this->bStmt = b;
    a.accept(this);
    return eq;
  }

  using IndexNotationVisitorStrict::visit;

  void visit(const AccessNode* anode) {
    if (!isa<AccessNode>(bExpr.ptr)) {
      eq = false;
      return;
    }
    auto bnode = to<AccessNode>(bExpr.ptr);
    if (anode->tensorVar != bnode->tensorVar) {
      eq = false;
      return;
    }
    if (anode->indexVars.size() != anode->indexVars.size()) {
      eq = false;
      return;
    }
    for (size_t i = 0; i < anode->indexVars.size(); i++) {
      if (anode->indexVars[i] != bnode->indexVars[i]) {
        eq = false;
        return;
      }
    }
    eq = true;
  }

  void visit(const LiteralNode* anode) {
    if (!isa<LiteralNode>(bExpr.ptr)) {
      eq = false;
      return;
    }
    auto bnode = to<LiteralNode>(bExpr.ptr);
    if (anode->getDataType() != bnode->getDataType()) {
      eq = false;
      return;
    }
    if (memcmp(anode->val,bnode->val,anode->getDataType().getNumBytes()) != 0) {
      eq = false;
      return;
    }
    eq = true;
  }

  template <class T>
  bool unaryEquals(const T* anode, IndexExpr b) {
    if (!isa<T>(b.ptr)) {
      return false;
    }
    auto bnode = to<T>(b.ptr);
    if (!equals(anode->a, bnode->a)) {
      return false;
    }
    return true;
  }

  void visit(const NegNode* anode) {
    eq = unaryEquals(anode, bExpr);
  }

  void visit(const SqrtNode* anode) {
    eq = unaryEquals(anode, bExpr);
  }

  template <class T>
  bool binaryEquals(const T* anode, IndexExpr b) {
    if (!isa<T>(b.ptr)) {
      return false;
    }
    auto bnode = to<T>(b.ptr);
    if (!equals(anode->a, bnode->a) || !equals(anode->b, bnode->b)) {
      return false;
    }
    return true;
  }

  void visit(const AddNode* anode) {
    eq = binaryEquals(anode, bExpr);
  }

  void visit(const SubNode* anode) {
    eq = binaryEquals(anode, bExpr);
  }

  void visit(const MulNode* anode) {
    eq = binaryEquals(anode, bExpr);
  }

  void visit(const DivNode* anode) {
    eq = binaryEquals(anode, bExpr);
  }

  void visit(const CastNode* anode) {
    if (!isa<CastNode>(bExpr.ptr)) {
      eq = false;
      return;
    }
    auto bnode = to<CastNode>(bExpr.ptr);
    if (anode->getDataType() != bnode->getDataType() ||
        !equals(anode->a, bnode->a)) {
      eq = false;
      return;
    }
    eq = true;
  }

  void visit(const CallIntrinsicNode* anode) {
    if (!isa<CallIntrinsicNode>(bExpr.ptr)) {
      eq = false;
      return;
    }
    auto bnode = to<CallIntrinsicNode>(bExpr.ptr);
    if (anode->func->getName() != bnode->func->getName() || 
        anode->args.size() != bnode->args.size()) {
      eq = false;
      return;
    }
    for (size_t i = 0; i < anode->args.size(); ++i) {
      if (!equals(anode->args[i], bnode->args[i])) {
        eq = false;
        return;
      }
    }
    eq = true;
  }

  void visit(const ReductionNode* anode) {
    if (!isa<ReductionNode>(bExpr.ptr)) {
      eq = false;
      return;
    }
    auto bnode = to<ReductionNode>(bExpr.ptr);
    if (!(equals(anode->op, bnode->op) && equals(anode->a, bnode->a))) {
      eq = false;
      return;
    }
    eq = true;
  }

  void visit(const AssignmentNode* anode) {
    if (!isa<AssignmentNode>(bStmt.ptr)) {
      eq = false;
      return;
    }
    auto bnode = to<AssignmentNode>(bStmt.ptr);
    if (!equals(anode->lhs, bnode->lhs) || !equals(anode->rhs, bnode->rhs) ||
        !equals(anode->op, bnode->op)) {
      eq = false;
      return;
    }
    eq = true;
  }

  void visit(const YieldNode* anode) {
    if (!isa<YieldNode>(bStmt.ptr)) {
      eq = false;
      return;
    }
    auto bnode = to<YieldNode>(bStmt.ptr);
    if (anode->indexVars.size() != anode->indexVars.size()) {
      eq = false;
      return;
    }
    for (size_t i = 0; i < anode->indexVars.size(); i++) {
      if (anode->indexVars[i] != bnode->indexVars[i]) {
        eq = false;
        return;
      }
    }
    if (!equals(anode->expr, bnode->expr)) { 
      eq = false;
      return;
    }
    eq = true;
  }

  void visit(const ForallNode* anode) {
    if (!isa<ForallNode>(bStmt.ptr)) {
      eq = false;
      return;
    }
    auto bnode = to<ForallNode>(bStmt.ptr);
    if (anode->indexVar != bnode->indexVar ||
        !equals(anode->stmt, bnode->stmt) ||
        anode->parallel_unit != bnode->parallel_unit ||
        anode->output_race_strategy != bnode->output_race_strategy) {
      eq = false;
      return;
    }
    eq = true;
  }

  void visit(const WhereNode* anode) {
    if (!isa<WhereNode>(bStmt.ptr)) {
      eq = false;
      return;
    }
    auto bnode = to<WhereNode>(bStmt.ptr);
    if (!equals(anode->consumer, bnode->consumer) ||
        !equals(anode->producer, bnode->producer)) {
      eq = false;
      return;
    }
    eq = true;
  }

  void visit(const SequenceNode* anode) {
    if (!isa<SequenceNode>(bStmt.ptr)) {
      eq = false;
      return;
    }
    auto bnode = to<SequenceNode>(bStmt.ptr);
    if (!equals(anode->definition, bnode->definition) ||
        !equals(anode->mutation, bnode->mutation)) {
      eq = false;
      return;
    }
    eq = true;
  }

  void visit(const MultiNode* anode) {
    if (!isa<MultiNode>(bStmt.ptr)) {
      eq = false;
      return;
    }
    auto bnode = to<MultiNode>(bStmt.ptr);
    if (!equals(anode->stmt1, bnode->stmt1) ||
        !equals(anode->stmt2, bnode->stmt2)) {
      eq = false;
      return;
    }
    eq = true;
  }

  void visit(const SuchThatNode* anode) {
    if (!isa<SuchThatNode>(bStmt.ptr)) {
      eq = false;
      return;
    }
    auto bnode = to<SuchThatNode>(bStmt.ptr);
    if (anode->predicate != bnode->predicate ||
        !equals(anode->stmt, bnode->stmt)) {
      eq = false;
      return;
    }
    eq = true;
  }
};

bool equals(IndexExpr a, IndexExpr b) {
  if (!a.defined() && !b.defined()) {
    return true;
  }
  if ((a.defined() && !b.defined()) || (!a.defined() && b.defined())) {
    return false;
  }
  return Equals().check(a,b);
}

IndexExpr operator-(const IndexExpr& expr) {
  return new NegNode(expr.ptr);
}

IndexExpr operator+(const IndexExpr& lhs, const IndexExpr& rhs) {
  return new AddNode(lhs, rhs);
}

IndexExpr operator-(const IndexExpr& lhs, const IndexExpr& rhs) {
  return new SubNode(lhs, rhs);
}

IndexExpr operator*(const IndexExpr& lhs, const IndexExpr& rhs) {
  return new MulNode(lhs, rhs);
}

IndexExpr operator/(const IndexExpr& lhs, const IndexExpr& rhs) {
  return new DivNode(lhs, rhs);
}


// class Access
Access::Access(const AccessNode* n) : IndexExpr(n) {
}

Access::Access(const TensorVar& tensor, const std::vector<IndexVar>& indices)
    : Access(new AccessNode(tensor, indices)) {
}

const TensorVar& Access::getTensorVar() const {
  return getNode(*this)->tensorVar;
}

const std::vector<IndexVar>& Access::getIndexVars() const {
  return getNode(*this)->indexVars;
}

static void check(Assignment assignment) {
  auto tensorVar = assignment.getLhs().getTensorVar();
  auto freeVars = assignment.getLhs().getIndexVars();
  auto indexExpr = assignment.getRhs();
  auto shape = tensorVar.getType().getShape();
  taco_uassert(error::dimensionsTypecheck(freeVars, indexExpr, shape))
      << error::expr_dimension_mismatch << " "
      << error::dimensionTypecheckErrors(freeVars, indexExpr, shape);
}

Assignment Access::operator=(const IndexExpr& expr) {
  TensorVar result = getTensorVar();
  Assignment assignment = Assignment(*this, expr);
  check(assignment);
  const_cast<AccessNode*>(getNode(*this))->setAssignment(assignment);
  return assignment;
}

Assignment Access::operator=(const Access& expr) {
  return operator=(static_cast<IndexExpr>(expr));
}

Assignment Access::operator=(const TensorVar& var) {
  return operator=(Access(var));
}

Assignment Access::operator+=(const IndexExpr& expr) {
  TensorVar result = getTensorVar();
  Assignment assignment = Assignment(result, getIndexVars(), expr, Add());
  check(assignment);
  const_cast<AccessNode*>(getNode(*this))->setAssignment(assignment);
  return assignment;
}

template <> bool isa<Access>(IndexExpr e) {
  return isa<AccessNode>(e.ptr);
}

template <> Access to<Access>(IndexExpr e) {
  taco_iassert(isa<Access>(e));
  return Access(to<AccessNode>(e.ptr));
}


// class Literal
Literal::Literal(const LiteralNode* n) : IndexExpr(n) {
}

Literal::Literal(bool val) : Literal(new LiteralNode(val)) {
}

Literal::Literal(unsigned char val) : Literal(new LiteralNode(val)) {
}

Literal::Literal(unsigned short val) : Literal(new LiteralNode(val)) {
}

Literal::Literal(unsigned int val) : Literal(new LiteralNode(val)) {
}

Literal::Literal(unsigned long val) : Literal(new LiteralNode(val)) {
}

Literal::Literal(unsigned long long val) : Literal(new LiteralNode(val)) {
}

Literal::Literal(char val) : Literal(new LiteralNode(val)) {
}

Literal::Literal(short val) : Literal(new LiteralNode(val)) {
}

Literal::Literal(int val) : Literal(new LiteralNode(val)) {
}

Literal::Literal(long val) : Literal(new LiteralNode(val)) {
}

Literal::Literal(long long val) : Literal(new LiteralNode(val)) {
}

Literal::Literal(int8_t val) : Literal(new LiteralNode(val)) {
}

Literal::Literal(float val) : Literal(new LiteralNode(val)) {
}

Literal::Literal(double val) : Literal(new LiteralNode(val)) {
}

Literal::Literal(std::complex<float> val) : Literal(new LiteralNode(val)) {
}

Literal::Literal(std::complex<double> val) : Literal(new LiteralNode(val)) {
}

IndexExpr Literal::zero(Datatype type) {
  switch (type.getKind()) {
    case Datatype::Bool:        return Literal(false);
    case Datatype::UInt8:       return Literal(uint8_t(0));
    case Datatype::UInt16:      return Literal(uint16_t(0));
    case Datatype::UInt32:      return Literal(uint32_t(0));
    case Datatype::UInt64:      return Literal(uint64_t(0));
    case Datatype::Int8:        return Literal(int8_t(0));
    case Datatype::Int16:       return Literal(int16_t(0));
    case Datatype::Int32:       return Literal(int32_t(0));
    case Datatype::Int64:       return Literal(int64_t(0));
    case Datatype::Float32:     return Literal(float(0.0));
    case Datatype::Float64:     return Literal(double(0.0));
    case Datatype::Complex64:   return Literal(std::complex<float>());
    case Datatype::Complex128:  return Literal(std::complex<double>());
    default:                    taco_ierror << "unsupported type";
  };

  return IndexExpr();
}

template <typename T> T Literal::getVal() const {
  return getNode(*this)->getVal<T>();
}
template bool Literal::getVal() const;
template unsigned char Literal::getVal() const;
template unsigned short Literal::getVal() const;
template unsigned int Literal::getVal() const;
template unsigned long Literal::getVal() const;
template unsigned long long Literal::getVal() const;
template char Literal::getVal() const;
template short Literal::getVal() const;
template int Literal::getVal() const;
template long Literal::getVal() const;
template long long Literal::getVal() const;
template int8_t Literal::getVal() const;
template float Literal::getVal() const;
template double Literal::getVal() const;
template std::complex<float> Literal::getVal() const;
template std::complex<double> Literal::getVal() const;

template <> bool isa<Literal>(IndexExpr e) {
  return isa<LiteralNode>(e.ptr);
}

template <> Literal to<Literal>(IndexExpr e) {
  taco_iassert(isa<Literal>(e));
  return Literal(to<LiteralNode>(e.ptr));
}


// class Neg
Neg::Neg(const NegNode* n) : IndexExpr(n) {
}

Neg::Neg(IndexExpr a) : Neg(new NegNode(a)) {
}

IndexExpr Neg::getA() const {
  return getNode(*this)->a;
}

template <> bool isa<Neg>(IndexExpr e) {
  return isa<NegNode>(e.ptr);
}

template <> Neg to<Neg>(IndexExpr e) {
  taco_iassert(isa<Neg>(e));
  return Neg(to<NegNode>(e.ptr));
}


// class Add
Add::Add() : Add(new AddNode) {
}

Add::Add(const AddNode* n) : IndexExpr(n) {
}

Add::Add(IndexExpr a, IndexExpr b) : Add(new AddNode(a, b)) {
}

IndexExpr Add::getA() const {
  return getNode(*this)->a;
}

IndexExpr Add::getB() const {
  return getNode(*this)->b;
}

template <> bool isa<Add>(IndexExpr e) {
  return isa<AddNode>(e.ptr);
}

template <> Add to<Add>(IndexExpr e) {
  taco_iassert(isa<Add>(e));
  return Add(to<AddNode>(e.ptr));
}


// class Sub
Sub::Sub() : Sub(new SubNode) {
}

Sub::Sub(const SubNode* n) : IndexExpr(n) {
}

Sub::Sub(IndexExpr a, IndexExpr b) : Sub(new SubNode(a, b)) {
}

IndexExpr Sub::getA() const {
  return getNode(*this)->a;
}

IndexExpr Sub::getB() const {
  return getNode(*this)->b;
}

template <> bool isa<Sub>(IndexExpr e) {
  return isa<SubNode>(e.ptr);
}

template <> Sub to<Sub>(IndexExpr e) {
  taco_iassert(isa<Sub>(e));
  return Sub(to<SubNode>(e.ptr));
}


// class Mul
Mul::Mul() : Mul(new MulNode) {
}

Mul::Mul(const MulNode* n) : IndexExpr(n) {
}

Mul::Mul(IndexExpr a, IndexExpr b) : Mul(new MulNode(a, b)) {
}

IndexExpr Mul::getA() const {
  return getNode(*this)->a;
}

IndexExpr Mul::getB() const {
  return getNode(*this)->b;
}

template <> bool isa<Mul>(IndexExpr e) {
  return isa<MulNode>(e.ptr);
}

template <> Mul to<Mul>(IndexExpr e) {
  taco_iassert(isa<Mul>(e));
  return Mul(to<MulNode>(e.ptr));
}


// class Div
Div::Div() : Div(new DivNode) {
}

Div::Div(const DivNode* n) : IndexExpr(n) {
}

Div::Div(IndexExpr a, IndexExpr b) : Div(new DivNode(a, b)) {
}

IndexExpr Div::getA() const {
  return getNode(*this)->a;
}

IndexExpr Div::getB() const {
  return getNode(*this)->b;
}

template <> bool isa<Div>(IndexExpr e) {
  return isa<DivNode>(e.ptr);
}

template <> Div to<Div>(IndexExpr e) {
  taco_iassert(isa<Div>(e));
  return Div(to<DivNode>(e.ptr));
}


// class Sqrt
Sqrt::Sqrt(const SqrtNode* n) : IndexExpr(n) {
}

Sqrt::Sqrt(IndexExpr a) : Sqrt(new SqrtNode(a)) {
}

IndexExpr Sqrt::getA() const {
  return getNode(*this)->a;
}

template <> bool isa<Sqrt>(IndexExpr e) {
  return isa<SqrtNode>(e.ptr);
}

template <> Sqrt to<Sqrt>(IndexExpr e) {
  taco_iassert(isa<Sqrt>(e));
  return Sqrt(to<SqrtNode>(e.ptr));
}


// class Cast
Cast::Cast(const CastNode* n) : IndexExpr(n) {
}

Cast::Cast(IndexExpr a, Datatype newType) : Cast(new CastNode(a, newType)) {
}

IndexExpr Cast::getA() const {
  return getNode(*this)->a;
}

template <> bool isa<Cast>(IndexExpr e) {
  return isa<CastNode>(e.ptr);
}

template <> Cast to<Cast>(IndexExpr e) {
  taco_iassert(isa<Cast>(e));
  return Cast(to<CastNode>(e.ptr));
}


// class CallIntrinsic
CallIntrinsic::CallIntrinsic(const CallIntrinsicNode* n) : IndexExpr(n) {
}

CallIntrinsic::CallIntrinsic(const std::shared_ptr<Intrinsic>& func,  
                             const std::vector<IndexExpr>& args) 
    : CallIntrinsic(new CallIntrinsicNode(func, args)) {
}

const Intrinsic& CallIntrinsic::getFunc() const {
  return *(getNode(*this)->func);
}

const std::vector<IndexExpr>& CallIntrinsic::getArgs() const {
  return getNode(*this)->args;
}

template <> bool isa<CallIntrinsic>(IndexExpr e) {
  return isa<CallIntrinsicNode>(e.ptr);
}

template <> CallIntrinsic to<CallIntrinsic>(IndexExpr e) {
  taco_iassert(isa<CallIntrinsic>(e));
  return CallIntrinsic(to<CallIntrinsicNode>(e.ptr));
}

IndexExpr mod(IndexExpr a, IndexExpr b) {
  return CallIntrinsic(std::make_shared<ModIntrinsic>(), {a, b});
}

IndexExpr abs(IndexExpr a) {
  return CallIntrinsic(std::make_shared<AbsIntrinsic>(), {a});
}

IndexExpr pow(IndexExpr a, IndexExpr b) {
  return CallIntrinsic(std::make_shared<PowIntrinsic>(), {a, b});
}

IndexExpr square(IndexExpr a) {
  return CallIntrinsic(std::make_shared<SquareIntrinsic>(), {a});
}

IndexExpr cube(IndexExpr a) {
  return CallIntrinsic(std::make_shared<CubeIntrinsic>(), {a});
}

IndexExpr sqrt(IndexExpr a) {
  return CallIntrinsic(std::make_shared<SqrtIntrinsic>(), {a});
}

IndexExpr cbrt(IndexExpr a) {
  return CallIntrinsic(std::make_shared<CbrtIntrinsic>(), {a});
}

IndexExpr exp(IndexExpr a) {
  return CallIntrinsic(std::make_shared<ExpIntrinsic>(), {a});
}

IndexExpr log(IndexExpr a) {
  return CallIntrinsic(std::make_shared<LogIntrinsic>(), {a});
}

IndexExpr log10(IndexExpr a) {
  return CallIntrinsic(std::make_shared<Log10Intrinsic>(), {a});
}

IndexExpr sin(IndexExpr a) {
  return CallIntrinsic(std::make_shared<SinIntrinsic>(), {a});
}

IndexExpr cos(IndexExpr a) {
  return CallIntrinsic(std::make_shared<CosIntrinsic>(), {a});
}

IndexExpr tan(IndexExpr a) {
  return CallIntrinsic(std::make_shared<TanIntrinsic>(), {a});
}

IndexExpr asin(IndexExpr a) {
  return CallIntrinsic(std::make_shared<AsinIntrinsic>(), {a});
}

IndexExpr acos(IndexExpr a) {
  return CallIntrinsic(std::make_shared<AcosIntrinsic>(), {a});
}

IndexExpr atan(IndexExpr a) {
  return CallIntrinsic(std::make_shared<AtanIntrinsic>(), {a});
}

IndexExpr atan2(IndexExpr a, IndexExpr b) {
  return CallIntrinsic(std::make_shared<Atan2Intrinsic>(), {a, b});
}

IndexExpr sinh(IndexExpr a) {
  return CallIntrinsic(std::make_shared<SinhIntrinsic>(), {a});
}

IndexExpr cosh(IndexExpr a) {
  return CallIntrinsic(std::make_shared<CoshIntrinsic>(), {a});
}

IndexExpr tanh(IndexExpr a) {
  return CallIntrinsic(std::make_shared<TanhIntrinsic>(), {a});
}

IndexExpr asinh(IndexExpr a) {
  return CallIntrinsic(std::make_shared<AsinhIntrinsic>(), {a});
}

IndexExpr acosh(IndexExpr a) {
  return CallIntrinsic(std::make_shared<AcoshIntrinsic>(), {a});
}

IndexExpr atanh(IndexExpr a) {
  return CallIntrinsic(std::make_shared<AtanhIntrinsic>(), {a});
}

IndexExpr gt(IndexExpr a, IndexExpr b) {
  return CallIntrinsic(std::make_shared<GtIntrinsic>(), {a, b});
}

IndexExpr lt(IndexExpr a, IndexExpr b) {
  return CallIntrinsic(std::make_shared<LtIntrinsic>(), {a, b});
}

IndexExpr gte(IndexExpr a, IndexExpr b) {
  return CallIntrinsic(std::make_shared<GteIntrinsic>(), {a, b});
}

IndexExpr lte(IndexExpr a, IndexExpr b) {
  return CallIntrinsic(std::make_shared<LteIntrinsic>(), {a, b});
}

IndexExpr eq(IndexExpr a, IndexExpr b) {
  return CallIntrinsic(std::make_shared<EqIntrinsic>(), {a, b});
}

IndexExpr neq(IndexExpr a, IndexExpr b) {
  return CallIntrinsic(std::make_shared<NeqIntrinsic>(), {a, b});
}

IndexExpr max(IndexExpr a, IndexExpr b) {
  return CallIntrinsic(std::make_shared<MaxIntrinsic>(), {a, b});
}

IndexExpr min(IndexExpr a, IndexExpr b) {
  return CallIntrinsic(std::make_shared<MinIntrinsic>(), {a, b});
}

IndexExpr heaviside(IndexExpr a, IndexExpr b) {
  if (!b.defined()) {
    b = Literal::zero(a.getDataType());
  }
  return CallIntrinsic(std::make_shared<HeavisideIntrinsic>(), {a, b});
}

IndexExpr Not(IndexExpr a) {
  return CallIntrinsic(std::make_shared<NotIntrinsic>(), {a});
}


// class Reduction
Reduction::Reduction(const ReductionNode* n) : IndexExpr(n) {
}

Reduction::Reduction(IndexExpr op, IndexVar var, IndexExpr expr)
    : Reduction(new ReductionNode(op, var, expr)) {
}

IndexExpr Reduction::getOp() const {
  return getNode(*this)->op;
}

IndexVar Reduction::getVar() const {
  return getNode(*this)->var;
}

IndexExpr Reduction::getExpr() const {
  return getNode(*this)->a;
}

Reduction sum(IndexVar i, IndexExpr expr) {
  return Reduction(Add(), i, expr);
}

template <> bool isa<Reduction>(IndexExpr s) {
  return isa<ReductionNode>(s.ptr);
}

template <> Reduction to<Reduction>(IndexExpr s) {
  taco_iassert(isa<Reduction>(s));
  return Reduction(to<ReductionNode>(s.ptr));
}


// class IndexStmt
IndexStmt::IndexStmt() : util::IntrusivePtr<const IndexStmtNode>(nullptr) {
}

IndexStmt::IndexStmt(const IndexStmtNode* n)
    : util::IntrusivePtr<const IndexStmtNode>(n) {
}

void IndexStmt::accept(IndexStmtVisitorStrict *v) const {
  ptr->accept(v);
}

std::vector<IndexVar> IndexStmt::getIndexVars() const {
  vector<IndexVar> vars;;
  set<IndexVar> seen;
  match(*this,
    std::function<void(const AssignmentNode*,Matcher*)>([&](
        const AssignmentNode* op, Matcher* ctx) {
      for (auto& var : op->lhs.getIndexVars()) {
        if (!util::contains(seen, var)) {
          vars.push_back(var);
          seen.insert(var);
        }
      }
      ctx->match(op->rhs);
    }),
    std::function<void(const AccessNode*)>([&](const AccessNode* op) {
      for (auto& var : op->indexVars) {
        if (!util::contains(seen, var)) {
          vars.push_back(var);
          seen.insert(var);
        }
      }
    })
  );
  return vars;
}

map<IndexVar,Dimension> IndexStmt::getIndexVarDomains() const {
  map<IndexVar, Dimension> indexVarDomains;
  match(*this,
    std::function<void(const AssignmentNode*,Matcher*)>([](
        const AssignmentNode* op, Matcher* ctx) {
      ctx->match(op->lhs);
      ctx->match(op->rhs);
    }),
    function<void(const AccessNode*)>([&indexVarDomains](const AccessNode* op) {
      auto& type = op->tensorVar.getType();
      auto& vars = op->indexVars;
      for (size_t i = 0; i < vars.size(); i++) {
        if (!util::contains(indexVarDomains, vars[i])) {
          indexVarDomains.insert({vars[i], type.getShape().getDimension(i)});
        }
        else {
          taco_iassert(indexVarDomains.at(vars[i]) ==
                       type.getShape().getDimension(i))
              << "Index variable used to index incompatible dimensions";
        }
      }
    })
  );

  return indexVarDomains;
}

IndexStmt IndexStmt::concretize() const {
  IndexStmt stmt = *this;
  if (isEinsumNotation(stmt)) {
    stmt = makeReductionNotation(stmt);
  }
  if (isReductionNotation(stmt)) {
    stmt = makeConcreteNotation(stmt);
  }
  return stmt;
}

IndexStmt IndexStmt::split(IndexVar i, IndexVar i1, IndexVar i2, size_t splitFactor) const {
  IndexVarRel rel = IndexVarRel(new SplitRelNode(i, i1, i2, splitFactor));
  string reason;

  // Add predicate to concrete index notation
  IndexStmt transformed = Transformation(AddSuchThatPredicates({rel})).apply(*this, &reason);
  if (!transformed.defined()) {
    taco_uerror << reason;
  }

  // Replace all occurrences of i with nested i1, i2
  transformed = Transformation(ForAllReplace({i}, {i1, i2})).apply(transformed, &reason);
  if (!transformed.defined()) {
    taco_uerror << reason;
  }

  return transformed;
}

IndexStmt IndexStmt::reorder(taco::IndexVar i, taco::IndexVar j) const {
  string reason;
  IndexStmt transformed = Reorder(i, j).apply(*this, &reason);
  if (!transformed.defined()) {
    taco_uerror << reason;
  }
  return transformed;
}

IndexStmt IndexStmt::reorder(std::vector<IndexVar> reorderedvars) const {
  string reason;
  IndexStmt transformed = Reorder(reorderedvars).apply(*this, &reason);
  if (!transformed.defined()) {
    taco_uerror << reason;
  }
  return transformed;
}

IndexStmt IndexStmt::parallelize(IndexVar i, PARALLEL_UNIT parallel_unit, OUTPUT_RACE_STRATEGY output_race_strategy) const {
  string reason;
  IndexStmt transformed = Parallelize(i, parallel_unit, output_race_strategy).apply(*this, &reason);
  if (!transformed.defined()) {
    taco_uerror << reason;
  }
  return transformed;
}

IndexStmt IndexStmt::pos(IndexVar i, IndexVar ipos, Access access) const {
  IndexVarRel rel = IndexVarRel(new PosRelNode(i, ipos, access));
  string reason;

  // Add predicate to concrete index notation
  IndexStmt transformed = Transformation(AddSuchThatPredicates({rel})).apply(*this, &reason);
  if (!transformed.defined()) {
    taco_uerror << reason;
  }

  // Replace all occurrences of i with ipos
  transformed = Transformation(ForAllReplace({i}, {ipos})).apply(transformed, &reason);
  if (!transformed.defined()) {
    taco_uerror << reason;
  }

  return transformed;
}

IndexStmt IndexStmt::fuse(IndexVar i, IndexVar j, IndexVar f) const {
  IndexVarRel rel = IndexVarRel(new FuseRelNode(i, j, f));
  string reason;

  // Add predicate to concrete index notation
  IndexStmt transformed = Transformation(AddSuchThatPredicates({rel})).apply(*this, &reason);
  if (!transformed.defined()) {
    taco_uerror << reason;
  }

  // Replace all occurrences of i, j with f
  transformed = Transformation(ForAllReplace({i,j}, {f})).apply(transformed, &reason);
  if (!transformed.defined()) {
    taco_uerror << reason;
  }

  return transformed;
}

bool equals(IndexStmt a, IndexStmt b) {
  if (!a.defined() && !b.defined()) {
    return true;
  }
  if ((a.defined() && !b.defined()) || (!a.defined() && b.defined())) {
    return false;
  }
  return Equals().check(a,b);
}

std::ostream& operator<<(std::ostream& os, const IndexStmt& expr) {
  if (!expr.defined()) return os << "IndexStmt()";
  IndexNotationPrinter printer(os);
  printer.print(expr);
  return os;
}

// class Assignment
Assignment::Assignment(const AssignmentNode* n) : IndexStmt(n) {
}

Assignment::Assignment(Access lhs, IndexExpr rhs, IndexExpr op)
    : Assignment(new AssignmentNode(lhs, rhs, op)) {
}

Assignment::Assignment(TensorVar tensor, vector<IndexVar> indices,
                       IndexExpr rhs, IndexExpr op)
    : Assignment(Access(tensor, indices), rhs, op) {
}

Access Assignment::getLhs() const {
  return getNode(*this)->lhs;
}

IndexExpr Assignment::getRhs() const {
  return getNode(*this)->rhs;
}

IndexExpr Assignment::getOperator() const {
  return getNode(*this)->op;
}

const std::vector<IndexVar>& Assignment::getFreeVars() const {
  return getLhs().getIndexVars();
}

std::vector<IndexVar> Assignment::getReductionVars() const {
  vector<IndexVar> freeVars = getLhs().getIndexVars();
  set<IndexVar> seen(freeVars.begin(), freeVars.end());
  vector<IndexVar> reductionVars;
  match(getRhs(),
    std::function<void(const AccessNode*)>([&](const AccessNode* op) {
    for (auto& var : op->indexVars) {
      if (!util::contains(seen, var)) {
        reductionVars.push_back(var);
        seen.insert(var);
      }
    }
    })
  );
  return reductionVars;
}

template <> bool isa<Assignment>(IndexStmt s) {
  return isa<AssignmentNode>(s.ptr);
}

template <> Assignment to<Assignment>(IndexStmt s) {
  taco_iassert(isa<Assignment>(s));
  return Assignment(to<AssignmentNode>(s.ptr));
}


// class Yield
Yield::Yield(const YieldNode* n) : IndexStmt(n) {
}

Yield::Yield(const std::vector<IndexVar>& indexVars, IndexExpr expr) 
    : Yield(new YieldNode(indexVars, expr)) {
}

const std::vector<IndexVar>& Yield::getIndexVars() const {
  return getNode(*this)->indexVars;
}

IndexExpr Yield::getExpr() const {
  return getNode(*this)->expr;
}


// class Forall
Forall::Forall(const ForallNode* n) : IndexStmt(n) {
}

Forall::Forall(IndexVar indexVar, IndexStmt stmt)
    : Forall(indexVar, stmt, PARALLEL_UNIT::NOT_PARALLEL, OUTPUT_RACE_STRATEGY::IGNORE_RACES) {
}

Forall::Forall(IndexVar indexVar, IndexStmt stmt, PARALLEL_UNIT parallel_unit, OUTPUT_RACE_STRATEGY output_race_strategy)
        : Forall(new ForallNode(indexVar, stmt, parallel_unit, output_race_strategy)) {
}

IndexVar Forall::getIndexVar() const {
  return getNode(*this)->indexVar;
}

IndexStmt Forall::getStmt() const {
  return getNode(*this)->stmt;
}

PARALLEL_UNIT Forall::getParallelUnit() const {
  return getNode(*this)->parallel_unit;
}

OUTPUT_RACE_STRATEGY Forall::getOutputRaceStrategy() const {
  return getNode(*this)->output_race_strategy;
}

Forall forall(IndexVar i, IndexStmt stmt) {
  return Forall(i, stmt);
}

Forall forall(IndexVar i, IndexStmt stmt, PARALLEL_UNIT parallel_unit, OUTPUT_RACE_STRATEGY output_race_strategy) {
  return Forall(i, stmt, parallel_unit, output_race_strategy);
}

template <> bool isa<Forall>(IndexStmt s) {
  return isa<ForallNode>(s.ptr);
}

template <> Forall to<Forall>(IndexStmt s) {
  taco_iassert(isa<Forall>(s));
  return Forall(to<ForallNode>(s.ptr));
}


// class Where
Where::Where(const WhereNode* n) : IndexStmt(n) {
}

Where::Where(IndexStmt consumer, IndexStmt producer)
    : Where(new WhereNode(consumer, producer)) {
}

IndexStmt Where::getConsumer() {
  return getNode(*this)->consumer;
}

IndexStmt Where::getProducer() {
  return getNode(*this)->producer;
}

TensorVar Where::getResult() {
  return getResultAccesses(getConsumer()).first[0].getTensorVar();
}

TensorVar Where::getTemporary() {
  return getResultAccesses(getProducer()).first[0].getTensorVar();
}

Where where(IndexStmt consumer, IndexStmt producer) {
  return Where(consumer, producer);
}

template <> bool isa<Where>(IndexStmt s) {
  return isa<WhereNode>(s.ptr);
}

template <> Where to<Where>(IndexStmt s) {
  taco_iassert(isa<Where>(s));
  return Where(to<WhereNode>(s.ptr));
}


// class Sequence
Sequence::Sequence(const SequenceNode* n) :IndexStmt(n) {
}

Sequence::Sequence(IndexStmt definition, IndexStmt mutation)
    : Sequence(new SequenceNode(definition, mutation)) {
}

IndexStmt Sequence::getDefinition() const {
  return getNode(*this)->definition;
}

IndexStmt Sequence::getMutation() const {
  return getNode(*this)->mutation;
}

Sequence sequence(IndexStmt definition, IndexStmt mutation) {
  return Sequence(definition, mutation);
}

template <> bool isa<Sequence>(IndexStmt s) {
  return isa<SequenceNode>(s.ptr);
}

template <> Sequence to<Sequence>(IndexStmt s) {
  taco_iassert(isa<Sequence>(s));
  return Sequence(to<SequenceNode>(s.ptr));
}


// class Multi
Multi::Multi(const MultiNode* n) : IndexStmt(n) {
}

Multi::Multi(IndexStmt stmt1, IndexStmt stmt2)
    : Multi(new MultiNode(stmt1, stmt2)) {
}

IndexStmt Multi::getStmt1() const {
  return getNode(*this)->stmt1;
}

IndexStmt Multi::getStmt2() const {
  return getNode(*this)->stmt2;
}

Multi multi(IndexStmt stmt1, IndexStmt stmt2) {
  return Multi(stmt1, stmt2);
}

template <> bool isa<Multi>(IndexStmt s) {
  return isa<MultiNode>(s.ptr);
}

template <> Multi to<Multi>(IndexStmt s) {
  taco_iassert(isa<Multi>(s));
  return Multi(to<MultiNode>(s.ptr));
}

// class SuchThat
SuchThat::SuchThat(const SuchThatNode* n) : IndexStmt(n) {
}

SuchThat::SuchThat(IndexStmt stmt, std::vector<IndexVarRel> predicate)
        : SuchThat(new SuchThatNode(stmt, predicate)) {
}

IndexStmt SuchThat::getStmt() const {
  return getNode(*this)->stmt;
}

std::vector<IndexVarRel> SuchThat::getPredicate() const {
  return getNode(*this)->predicate;
}

SuchThat suchthat(IndexStmt stmt, std::vector<IndexVarRel> predicate) {
  return SuchThat(stmt, predicate);
}

template <> bool isa<SuchThat>(IndexStmt s) {
  return isa<SuchThatNode>(s.ptr);
}

template <> SuchThat to<SuchThat>(IndexStmt s) {
  taco_iassert(isa<SuchThat>(s));
  return SuchThat(to<SuchThatNode>(s.ptr));
}




// class IndexVar
IndexVar::IndexVar() : IndexVar(util::uniqueName('i')) {}

IndexVar::IndexVar(const std::string& name) : content(new Content) {
  content->name = name;
}

std::string IndexVar::getName() const {
  return content->name;
}

bool operator==(const IndexVar& a, const IndexVar& b) {
  return a.content == b.content;
}

bool operator<(const IndexVar& a, const IndexVar& b) {
  return a.content < b.content;
}

std::ostream& operator<<(std::ostream& os, const IndexVar& var) {
  return os << var.getName();
}

void IndexVarRel::print(std::ostream& stream) const {
  if (ptr == nullptr) {
    stream << "undefined";
  }
  else {
    switch(getRelType()) {
      case SPLIT:
        getNode<SplitRelNode>()->print(stream);
        break;
      case POS:
        getNode<PosRelNode>()->print(stream);
        break;
      case FUSE:
        getNode<FuseRelNode>()->print(stream);
        break;
      default:
        taco_ierror;
    }
  }
}

bool IndexVarRel::equals(const IndexVarRel &rel) const {
  if (getRelType() != rel.getRelType()) {
    return false;
  }

  switch(getRelType()) {
    case SPLIT:
      return getNode<SplitRelNode>()->equals(*rel.getNode<SplitRelNode>());
    case POS:
      return getNode<PosRelNode>()->equals(*rel.getNode<PosRelNode>());
      break;
    case FUSE:
      return getNode<FuseRelNode>()->equals(*rel.getNode<FuseRelNode>());
      break;
    case UNDEFINED:
      return true;
    default:
      taco_ierror;
      return false;
  }
}

bool operator==(const IndexVarRel& a, const IndexVarRel& b) {
  return a.equals(b);
}

std::ostream& operator<<(std::ostream& stream, const IndexVarRel& rel) {
  rel.print(stream);
  return stream;
}

IndexVarRelType IndexVarRel::getRelType() const {
  if (ptr == NULL) return UNDEFINED;
  return getNode()->relType;
}

std::vector<ir::Expr> IndexVarRelNode::computeRelativeBound(std::set<IndexVar> definedVars, std::map<IndexVar, std::vector<ir::Expr>> computedBounds, std::map<IndexVar, ir::Expr> variableExprs, Iterators iterators, IndexVarRelGraph relGraph) const {
  taco_ierror;
  return {};
}

std::vector<ir::Expr> IndexVarRelNode::deriveIterBounds(IndexVar indexVar, std::map<IndexVar, std::vector<ir::Expr>> parentIterBounds,
                                                        std::map<IndexVar, std::vector<ir::Expr>> parentCoordBounds,
                                       std::map<taco::IndexVar, taco::ir::Expr> variableNames,
                                       Iterators iterators, IndexVarRelGraph relGraph) const {
  taco_ierror;
  return {};
}

ir::Expr IndexVarRelNode::recoverVariable(IndexVar indexVar, std::map<IndexVar, ir::Expr> variableNames, Iterators iterators, std::map<IndexVar, std::vector<ir::Expr>> parentCoordBounds, IndexVarRelGraph relGraph) const {
  taco_ierror;
  return {};
}

ir::Stmt IndexVarRelNode::recoverChild(IndexVar indexVar, std::map<IndexVar, ir::Expr> variableNames, bool emitVarDecl, Iterators iterators, IndexVarRelGraph relGraph) const {
  taco_ierror;
  return {};
}


void SplitRelNode::print(std::ostream &stream) const {
  stream << "split(" << parentVar << ", " << outerVar << ", " << innerVar << ", " << splitFactor << ")";
}

bool SplitRelNode::equals(const SplitRelNode &rel) const {
  return parentVar == rel.parentVar && outerVar == rel.outerVar
        && innerVar == rel.innerVar && splitFactor == rel.splitFactor;
}

std::vector<IndexVar> SplitRelNode::getParents() const {
  return {parentVar};
}

std::vector<IndexVar> SplitRelNode::getChildren() const {
  return {outerVar, innerVar};
}

std::vector<IndexVar> SplitRelNode::getIrregulars() const {
  return {outerVar};
}

std::vector<ir::Expr> SplitRelNode::computeRelativeBound(std::set<IndexVar> definedVars, std::map<IndexVar, std::vector<ir::Expr>> computedBounds, std::map<IndexVar, ir::Expr> variableExprs, Iterators iterators, IndexVarRelGraph relGraph) const {
  taco_iassert(computedBounds.count(parentVar) == 1);
  std::vector<ir::Expr> parentBound = computedBounds.at(parentVar);
  bool outerVarDefined = definedVars.count(outerVar);
  bool innerVarDefined = definedVars.count(innerVar);

  if (relGraph.isPosVariable(parentVar)) {
    return parentBound; // splitting pos space does not change coordinate bounds
  }

  ir::Expr splitFactorLiteral = ir::Literal::make(splitFactor, variableExprs[parentVar].type());

  if (!outerVarDefined && !innerVarDefined) {
    return parentBound;
  }
  else if(outerVarDefined && !innerVarDefined) {
    // outerVar constrains space to a length splitFactor strip starting at outerVar * splitFactor
    ir::Expr minBound = parentBound[0];
    minBound = ir::Add::make(minBound, ir::Mul::make(variableExprs[outerVar], splitFactorLiteral));
    ir::Expr maxBound = ir::Min::make(parentBound[1], ir::Add::make(minBound, splitFactorLiteral));
    return {minBound, maxBound};
  }
  else if(!outerVarDefined && innerVarDefined) {
    // when innerVar is defined first does not limit coordinate space
    return parentBound;
  }
  else {
    taco_iassert(outerVarDefined && innerVarDefined);
    // outerVar and innervar constrains space to a length 1 strip starting at outerVar * splitFactor + innerVar
    ir::Expr minBound = parentBound[0];
    minBound = ir::Add::make(minBound, ir::Add::make(ir::Mul::make(variableExprs[outerVar], splitFactorLiteral), variableExprs[innerVar]));
    ir::Expr maxBound = ir::Min::make(parentBound[1], ir::Add::make(minBound, ir::Literal::make(1, variableExprs[parentVar].type())));
    return {minBound, maxBound};
  }
}

std::vector<ir::Expr> SplitRelNode::deriveIterBounds(taco::IndexVar indexVar,
                                                     std::map<IndexVar, std::vector<ir::Expr>> parentIterBounds,
                                                     std::map<IndexVar, std::vector<ir::Expr>> parentCoordBounds,
                                                     std::map<taco::IndexVar, taco::ir::Expr> variableNames,
                                                     Iterators iterators, IndexVarRelGraph relGraph) const {
  taco_iassert(indexVar == outerVar || indexVar == innerVar);
  taco_iassert(parentIterBounds.size() == 1);
  taco_iassert(parentIterBounds.count(parentVar) == 1);

  std::vector<ir::Expr> parentBound = parentIterBounds.at(parentVar);
  Datatype splitFactorType = parentBound[0].type();
  if (indexVar == outerVar) {
    ir::Expr minBound = ir::Div::make(parentBound[0], ir::Literal::make(splitFactor, splitFactorType));
    ir::Expr maxBound = ir::Div::make(ir::Add::make(parentBound[1], ir::Literal::make(splitFactor-1, splitFactorType)), ir::Literal::make(splitFactor, splitFactorType));
    return {minBound, maxBound};
  }
  else if (indexVar == innerVar) {
    ir::Expr minBound = 0;
    ir::Expr maxBound = ir::Literal::make(splitFactor, splitFactorType);
    return {minBound, maxBound};
  }
  taco_ierror;
  return {};
}

ir::Expr SplitRelNode::recoverVariable(taco::IndexVar indexVar,
                                       std::map<taco::IndexVar, taco::ir::Expr> variableNames,
                                       Iterators iterators, std::map<IndexVar, std::vector<ir::Expr>> parentCoordBounds, IndexVarRelGraph relGraph) const {
  taco_iassert(indexVar == parentVar);
  taco_iassert(variableNames.count(parentVar) && variableNames.count(outerVar) && variableNames.count(innerVar));
  Datatype splitFactorType = variableNames[parentVar].type();
  return ir::Add::make(ir::Mul::make(variableNames[outerVar], ir::Literal::make(splitFactor, splitFactorType)), variableNames[innerVar]);
}

ir::Stmt SplitRelNode::recoverChild(taco::IndexVar indexVar,
                                       std::map<taco::IndexVar, taco::ir::Expr> variableNames, bool emitVarDecl, Iterators iterators, IndexVarRelGraph relGraph) const {
  taco_iassert(indexVar == outerVar || indexVar == innerVar);
  taco_iassert(variableNames.count(parentVar) && variableNames.count(outerVar) && variableNames.count(innerVar));
  Datatype splitFactorType = variableNames[parentVar].type();
  if (indexVar == outerVar) {
    // outerVar = parentVar - innerVar
    ir::Expr subStmt = ir::Sub::make(variableNames[parentVar], variableNames[innerVar]);
    if (emitVarDecl) {
      return ir::Stmt(ir::VarDecl::make(variableNames[outerVar], subStmt));
    }
    else {
      return ir::Stmt(ir::Assign::make(variableNames[outerVar], subStmt));
    }
  }
  else {
    // innerVar = parentVar - outerVar * splitFactor
    ir::Expr subStmt = ir::Sub::make(variableNames[parentVar],
                                     ir::Mul::make(variableNames[outerVar], ir::Literal::make(splitFactor, splitFactorType)));
    if (emitVarDecl) {
      return ir::Stmt(ir::VarDecl::make(variableNames[innerVar], subStmt));
    }
    else {
      return ir::Stmt(ir::Assign::make(variableNames[innerVar], subStmt));
    }
  }
}

bool operator==(const SplitRelNode& a, const SplitRelNode& b) {
  return a.equals(b);
}

void PosRelNode::print(std::ostream &stream) const {
  stream << "pos(" << parentVar << ", " << posVar << ", " << access << ")";
}

bool PosRelNode::equals(const PosRelNode &rel) const {
  return parentVar == rel.parentVar && posVar == rel.posVar
         && access == rel.access;
}

std::vector<IndexVar> PosRelNode::getParents() const {
  return {parentVar};
}

std::vector<IndexVar> PosRelNode::getChildren() const {
  return {posVar};
}

std::vector<IndexVar> PosRelNode::getIrregulars() const {
  return {posVar};
}

std::vector<ir::Expr> PosRelNode::computeRelativeBound(std::set<IndexVar> definedVars, std::map<IndexVar, std::vector<ir::Expr>> computedBounds, std::map<IndexVar, ir::Expr> variableExprs, Iterators iterators, IndexVarRelGraph relGraph) const {
  // new coordinate bounds are projection of segment bounds -> no actually remain unchanged
  taco_iassert(computedBounds.count(parentVar) == 1);
  std::vector<ir::Expr> parentCoordBound = computedBounds.at(parentVar);
  return parentCoordBound;
}

std::vector<ir::Expr> PosRelNode::deriveIterBounds(taco::IndexVar indexVar,
                                                     std::map<IndexVar, std::vector<ir::Expr>> parentIterBounds,
                                                     std::map<IndexVar, std::vector<ir::Expr>> parentCoordBounds,
                                                     std::map<taco::IndexVar, taco::ir::Expr> variableNames,
                                                     Iterators iterators,
                                                     IndexVarRelGraph relGraph) const {
  taco_iassert(indexVar == posVar);
  taco_iassert(parentCoordBounds.count(parentVar) == 1);
  std::vector<ir::Expr> parentCoordBound = parentCoordBounds.at(parentVar);

  if (relGraph.getUnderivedAncestors(indexVar).size() > 1) {
    // has fused take iterbounds instead
    // TODO: need to search segments by multiple underived for now just assume complete iteration
    Iterator accessIterator = getAccessIterator(iterators, relGraph);
    Iterator rootIterator = accessIterator;
    while(!rootIterator.isRoot()) {
      rootIterator = rootIterator.getParent();
    }
    ir::Expr parentSize = 1; // to find size of segment walk down sizes of iterator chain
    while (rootIterator != accessIterator) {
      rootIterator = rootIterator.getChild();
      if (rootIterator.hasAppend()) {
        parentSize = rootIterator.getSize(parentSize);
      } else if (rootIterator.hasInsert()) {
        parentSize = ir::Mul::make(parentSize, rootIterator.getWidth());
      }
    }
    return {ir::Literal::make(0), parentSize};
  }

  // locate position var for segment based on coordinate parentVar
  ir::Expr posVarExpr = variableNames[posVar];
  return locateBounds(parentCoordBound, posVarExpr.type(), iterators, relGraph);
}

std::vector<ir::Expr> PosRelNode::locateBounds(std::vector<ir::Expr> coordBounds,
                                                   Datatype boundType,
                                                   Iterators iterators,
                                                   IndexVarRelGraph relGraph) const {
  Iterator accessIterator = getAccessIterator(iterators, relGraph);
  ir::Expr parentPos = accessIterator.getParent().getPosVar();
  ModeFunction segment_bounds = accessIterator.posBounds(parentPos);
  vector<ir::Expr> binarySearchArgsStart = {
          getAccessCoordArray(iterators, relGraph),
          segment_bounds[0], // arrayStart
          segment_bounds[1], // arrayEnd
          coordBounds[0]
  };

  vector<ir::Expr> binarySearchArgsEnd = {
          getAccessCoordArray(iterators, relGraph),
          segment_bounds[0], // arrayStart
          segment_bounds[1], // arrayEnd
          coordBounds[1]
  };

  ir::Expr start = ir::Call::make("taco_binarySearchAfter", binarySearchArgsStart, boundType);
  // simplify start when this is 0
  ir::Expr simplifiedParentBound = ir::simplify(coordBounds[0]);
  if (isa<ir::Literal>(simplifiedParentBound) && to<ir::Literal>(simplifiedParentBound)->equalsScalar(0)) {
    start = segment_bounds[0];
  }
  ir::Expr end = ir::Call::make("taco_binarySearchAfter", binarySearchArgsEnd, boundType);
  // simplify end -> A1_pos[1] when parentBound[1] is max coord dimension
  simplifiedParentBound = ir::simplify(coordBounds[1]);
  if (isa<ir::GetProperty>(simplifiedParentBound) && to<ir::GetProperty>(simplifiedParentBound)->property == ir::TensorProperty::Dimension) {
    end = segment_bounds[1];
  }
  return {start, end};
}

Iterator PosRelNode::getAccessIterator(Iterators iterators, IndexVarRelGraph relGraph) const {
  size_t mode_index = 0; // which of the access index vars match?

  // when multiple underived ancestors, match iterator with max mode (iterate bottom level)
  vector<IndexVar> underivedParentAncestors = relGraph.getUnderivedAncestors(parentVar);
  int max_mode = 0;
  for (IndexVar underivedParent : underivedParentAncestors) {
    for (auto var : access.getIndexVars()) {
      if (var == underivedParent) {
        break;
      }
      mode_index++;
    }
    taco_iassert(mode_index < access.getIndexVars().size());
    int mode = access.getTensorVar().getFormat().getModeOrdering()[mode_index];
    if (mode > max_mode) {
      max_mode = mode;
    }
  }

  // can't use default level iterator access function because mapping contents rather than pointer which is default to allow repeated operands
  std::map<ModeAccess, Iterator> levelIterators = iterators.levelIterators();
  ModeAccess modeAccess = ModeAccess(access, max_mode+1);
  for (auto levelIterator : levelIterators) {
    if (::taco::equals(levelIterator.first.getAccess(), modeAccess.getAccess()) && levelIterator.first.getModePos() == modeAccess.getModePos()) {
      return levelIterator.second;
    }
  }
  taco_ierror;
  return Iterator();
}

ir::Expr PosRelNode::getAccessCoordArray(Iterators iterators, IndexVarRelGraph relGraph) const {
  return getAccessIterator(iterators, relGraph).getMode().getModePack().getArray(1);
}


ir::Expr PosRelNode::recoverVariable(taco::IndexVar indexVar,
                                       std::map<taco::IndexVar, taco::ir::Expr> variableNames,
                                       Iterators iterators,
                                       std::map<IndexVar, std::vector<ir::Expr>> parentCoordBounds,
                                       IndexVarRelGraph relGraph) const {
  taco_iassert(indexVar == parentVar);
  taco_iassert(variableNames.count(parentVar) == 1 && variableNames.count(posVar) == 1);
  taco_iassert(parentCoordBounds.count(parentVar) == 1);

  ir::Expr coord_array = getAccessCoordArray(iterators, relGraph);

  Iterator accessIterator = getAccessIterator(iterators, relGraph);
  ir::Expr parentPos = accessIterator.getParent().getPosVar();
  ModeFunction segment_bounds = accessIterator.posBounds(parentPos);

  // positions should be with respect to entire array not just segment so don't need to offset variable when projecting.
  ir::Expr project_result = ir::Load::make(coord_array, variableNames.at(posVar));

  // but need to subtract parentvars start corodbound
  ir::Expr parent_value = ir::Sub::make(project_result, parentCoordBounds[parentVar][0]);

  return parent_value;
}

ir::Stmt PosRelNode::recoverChild(taco::IndexVar indexVar,
                                    std::map<taco::IndexVar, taco::ir::Expr> variableNames, bool emitVarDecl, Iterators iterators, IndexVarRelGraph relGraph) const {
  taco_iassert(indexVar == posVar);
  taco_iassert(variableNames.count(parentVar) && variableNames.count(posVar));
  // locate position var for segment based on coordinate parentVar
  ir::Expr posVarExpr = variableNames[posVar];

  Iterator accessIterator = getAccessIterator(iterators, relGraph);
  ir::Expr parentPos = accessIterator.getParent().getPosVar();
  ModeFunction segment_bounds = accessIterator.posBounds(parentPos);
  vector<ir::Expr> binarySearchArgs = {
          getAccessCoordArray(iterators, relGraph),
          segment_bounds[0], // arrayStart
          segment_bounds[1], // arrayEnd
          variableNames[parentVar]
  };
  return ir::VarDecl::make(posVarExpr, ir::Call::make("taco_binarySearchAfter", binarySearchArgs, posVarExpr.type()));
}

bool operator==(const PosRelNode& a, const PosRelNode& b) {
  return a.equals(b);
}

void FuseRelNode::print(std::ostream &stream) const {
  stream << "fuse(" << outerParentVar << ", " << innerParentVar << ", " << fusedVar << ")";
}

bool FuseRelNode::equals(const FuseRelNode &rel) const {
  return outerParentVar == rel.outerParentVar && innerParentVar == rel.innerParentVar
         && fusedVar == rel.fusedVar;
}

std::vector<IndexVar> FuseRelNode::getParents() const {
  return {outerParentVar, innerParentVar};
}

std::vector<IndexVar> FuseRelNode::getChildren() const {
  return {fusedVar};
}

std::vector<IndexVar> FuseRelNode::getIrregulars() const {
  return {fusedVar};
}

std::vector<ir::Expr> FuseRelNode::computeRelativeBound(std::set<IndexVar> definedVars, std::map<IndexVar, std::vector<ir::Expr>> computedBounds, std::map<IndexVar, ir::Expr> variableExprs, Iterators iterators, IndexVarRelGraph relGraph) const {
  taco_iassert(computedBounds.count(outerParentVar) && computedBounds.count(innerParentVar));
  return combineParentBounds(computedBounds[outerParentVar], computedBounds[innerParentVar]);
}

std::vector<ir::Expr> FuseRelNode::deriveIterBounds(taco::IndexVar indexVar,
                                                     std::map<IndexVar, std::vector<ir::Expr>> parentIterBounds,
                                                     std::map<IndexVar, std::vector<ir::Expr>> parentCoordBounds,
                                                     std::map<taco::IndexVar, taco::ir::Expr> variableNames,
                                                     Iterators iterators, IndexVarRelGraph relGraph) const {
  taco_iassert(indexVar == fusedVar);
  taco_iassert(parentIterBounds.count(outerParentVar) && parentIterBounds.count(innerParentVar));
  return combineParentBounds(parentIterBounds[outerParentVar], parentIterBounds[innerParentVar]);
}

ir::Expr FuseRelNode::recoverVariable(taco::IndexVar indexVar,
                                       std::map<taco::IndexVar, taco::ir::Expr> variableNames,
                                       Iterators iterators, std::map<IndexVar, std::vector<ir::Expr>> parentCoordBounds, IndexVarRelGraph relGraph) const {
  taco_iassert(variableNames.count(indexVar));
  taco_iassert(parentCoordBounds.count(innerParentVar));
  ir::Expr innerSize = ir::Sub::make(parentCoordBounds[innerParentVar][1], parentCoordBounds[innerParentVar][0]);

  if (indexVar == outerParentVar) {
    // outerVar = fusedVar / innerSize
    return ir::Div::make(variableNames[fusedVar], innerSize);
  }
  else if (indexVar == innerParentVar) {
    // innerVar = fusedVar % innerSize
    return ir::Rem::make(variableNames[fusedVar], innerSize);
  }
  else {
    taco_unreachable;
    return ir::Expr();
  }
}

ir::Stmt FuseRelNode::recoverChild(taco::IndexVar indexVar,
                                    std::map<taco::IndexVar, taco::ir::Expr> variableNames, bool emitVarDecl, Iterators iterators, IndexVarRelGraph relGraph) const {
//  taco_iassert(indexVar == fusedVar);
//  taco_iassert(variableNames.count(indexVar) && variableNames.count(outerParentVar) && variableNames.count(innerParentVar));
//  taco_iassert(parentCoordBounds.count(innerParentVar));
//  ir::Expr innerSize = ir::Sub::make(parentCoordBounds[innerParentVar][1], parentCoordBounds[innerParentVar][0]);
//  return ir::Add::make(ir::Mul::make(variableNames[outerParentVar], innerSize), variableNames[innerParentVar]);
  taco_not_supported_yet;
  return ir::Stmt();
}

std::vector<ir::Expr> FuseRelNode::combineParentBounds(std::vector<ir::Expr> outerParentBound, std::vector<ir::Expr> innerParentBound) const {
  ir::Expr innerSize = ir::Sub::make(innerParentBound[1], innerParentBound[0]);
  ir::Expr minBound = ir::Add::make(ir::Mul::make(outerParentBound[0], innerSize), innerParentBound[0]);
  ir::Expr maxBound = ir::Add::make(ir::Mul::make(outerParentBound[1], innerSize), innerParentBound[1]);
  return {minBound, maxBound};
}

bool operator==(const FuseRelNode& a, const FuseRelNode& b) {
  return a.equals(b);
}

// class IndexVarRelGraph
IndexVarRelGraph::IndexVarRelGraph(IndexStmt concreteStmt) {
  // Get SuchThat node with relations
  if (!isa<SuchThat>(concreteStmt)) {
    // No relations defined so no variables in relation
    return;
  }
  SuchThat suchThat = to<SuchThat>(concreteStmt);
  vector<IndexVarRel> relations = suchThat.getPredicate();

  for (IndexVarRel rel : relations) {
    std::vector<IndexVar> parents = rel.getNode()->getParents();
    std::vector<IndexVar> children = rel.getNode()->getChildren();
    for (IndexVar parent : parents) {
      childRelMap[parent] = rel;
      childrenMap[parent] = children;
    }

    for (IndexVar child : children) {
      parentRelMap[child] = rel;
      parentsMap[child] = parents;
    }
  }
}

std::vector<IndexVar> IndexVarRelGraph::getChildren(IndexVar indexVar) const {
  if (childrenMap.count(indexVar)) {
    return childrenMap.at(indexVar);
  }
  return {};
}

std::vector<IndexVar> IndexVarRelGraph::getParents(IndexVar indexVar) const {
  if (parentsMap.count(indexVar)) {
    return parentsMap.at(indexVar);
  }
  return {};
}

std::vector<IndexVar> IndexVarRelGraph::getFullyDerivedDescendants(IndexVar indexVar) const {
  // DFS to find all underived parents
  std::vector<IndexVar> children = getChildren(indexVar);
  if (children.empty()) {
    return {indexVar};
  }

  std::vector<IndexVar> fullyDerivedChildren;
  for (IndexVar child : children) {
    std::vector<IndexVar> childFullyDerived = getFullyDerivedDescendants(child);
    fullyDerivedChildren.insert(fullyDerivedChildren.end(), childFullyDerived.begin(), childFullyDerived.end());
  }
  return fullyDerivedChildren;
}

std::vector<IndexVar> IndexVarRelGraph::getUnderivedAncestors(IndexVar indexVar) const {
  // DFS to find all underived parents
  std::vector<IndexVar> parents = getParents(indexVar);
  if (parents.empty()) {
    return {indexVar};
  }

  std::vector<IndexVar> underivedParents;
  for (IndexVar parent : parents) {
    std::vector<IndexVar> parentUnderived = getUnderivedAncestors(parent);
    underivedParents.insert(underivedParents.end(), parentUnderived.begin(), parentUnderived.end());
  }
  return underivedParents;
}

bool IndexVarRelGraph::getIrregularDescendant(IndexVar indexVar, IndexVar *irregularChild) const {
  if (isFullyDerived(indexVar) && isIrregular(indexVar)) {
    *irregularChild = indexVar;
    return true;
  }
  for (IndexVar child : getChildren(indexVar)) {
    if (getIrregularDescendant(child, irregularChild)) {
      return true;
    }
  }
  return false;
}

// A pos Iterator Descendant is first innermost variable that is pos
bool IndexVarRelGraph::getPosIteratorDescendant(IndexVar indexVar, IndexVar *irregularChild) const {
  if (isPosVariable(indexVar)) {
    *irregularChild = indexVar;
    return true;
  }

  if (getChildren(indexVar).size() == 1) {
    return getPosIteratorDescendant(getChildren(indexVar)[0], irregularChild);
  }
  for (IndexVar child : getChildren(indexVar)) {
    if (!isIrregular(child)) {
      return getPosIteratorDescendant(child, irregularChild);
    }
  }
  return false;
}

bool IndexVarRelGraph::isIrregular(IndexVar indexVar) const {
  if (isUnderived(indexVar)) {
    return true;
  }

  IndexVarRel rel = parentRelMap.at(indexVar);
  std::vector<IndexVar> irregulars = rel.getNode()->getIrregulars();
  auto it = std::find (irregulars.begin(), irregulars.end(), indexVar);
  if (it == irregulars.end()) {
    // variable does not maintain irregular status through relationship
    return false;
  }

  for (const IndexVar& parent : getParents(indexVar)) {
    if (isIrregular(parent)) {
      return true;
    }
  }
  return false;
}

bool IndexVarRelGraph::isUnderived(taco::IndexVar indexVar) const {
  return getParents(indexVar).empty();
}

bool IndexVarRelGraph::isDerivedFrom(taco::IndexVar indexVar, taco::IndexVar ancestor) const {
  for (IndexVar parent : getParents(indexVar)) {
    if (parent == ancestor) {
      return true;
    }
    if(isDerivedFrom(parent, ancestor)) {
      return true;
    }
  }
  return false;
}

bool IndexVarRelGraph::isFullyDerived(taco::IndexVar indexVar) const {
  return getChildren(indexVar).empty();
}

bool IndexVarRelGraph::isAvailable(IndexVar indexVar, std::set<IndexVar> defined) const {
  for (const IndexVar& parent : getParents(indexVar)) {
    if (!defined.count(parent)) {
      return false;
    }
  }
  return true;
}

bool IndexVarRelGraph::isRecoverable(taco::IndexVar indexVar, std::set<taco::IndexVar> defined) const {
  // all children are either defined or recoverable from their children
  for (const IndexVar& child : getChildren(indexVar)) {
    if (!defined.count(child) && (isFullyDerived(child) || !isRecoverable(child, defined))) {
      return false;
    }
  }
  return true;
}

bool IndexVarRelGraph::isChildRecoverable(taco::IndexVar indexVar, std::set<taco::IndexVar> defined) const {
  // at most 1 unknown in relation
  int count_unknown = 0;
  for (const IndexVar& parent : getParents(indexVar)) {
    if (!defined.count(parent)) {
      count_unknown++;
    }
    for (const IndexVar& sibling : getChildren(parent)) {
      if (!defined.count(sibling)) {
        count_unknown++;
      }
    }
  }
  cout << indexVar << ": " << count_unknown << endl; // TODO:
  return count_unknown <= 1;
}

// in terms of joined spaces
void IndexVarRelGraph::addRelativeBoundsToMap(IndexVar indexVar, std::set<IndexVar> alreadyDefined, std::map<IndexVar, std::vector<ir::Expr>> &bounds, std::map<IndexVar, ir::Expr> variableExprs, Iterators iterators) const {
  // derive bounds of parents and use to construct bounds
  if (isUnderived(indexVar)) {
    taco_iassert(bounds.count(indexVar));
    return; // underived bound should already be in bounds
  }

  for (IndexVar parent : getParents(indexVar)) {
    addRelativeBoundsToMap(parent, alreadyDefined, bounds, variableExprs, iterators);
  }

  IndexVarRel rel = parentRelMap.at(indexVar);
  bounds[indexVar] = rel.getNode()->computeRelativeBound(alreadyDefined, bounds, variableExprs, iterators, *this);
}

void IndexVarRelGraph::computeBoundsForUnderivedAncestors(IndexVar indexVar, std::map<IndexVar, std::vector<ir::Expr>> relativeBounds, std::map<IndexVar, std::vector<ir::Expr>> &computedBounds) const {
  std::vector<IndexVar> underivedAncestors = getUnderivedAncestors(indexVar);
  // taco_iassert(underivedAncestors.size() == 1); // TODO: fuse

  computedBounds[underivedAncestors[0]] = relativeBounds[indexVar];
}

std::map<IndexVar, std::vector<ir::Expr>> IndexVarRelGraph::deriveCoordBounds(std::vector<IndexVar> derivedVarOrder, std::map<IndexVar, std::vector<ir::Expr>> underivedBounds, std::map<IndexVar, ir::Expr> variableExprs, Iterators iterators) const {
  std::map<IndexVar, std::vector<ir::Expr>> computedCoordbounds = underivedBounds;
  std::set<IndexVar> defined;
  for (IndexVar indexVar : derivedVarOrder) {
    if (indexVar != derivedVarOrder.back()) {
      for (auto recoverable : newlyRecoverableParents(indexVar, defined)) {
        defined.insert(recoverable);
      }
      defined.insert(indexVar);
    }
    if (isUnderived(indexVar)) {
      continue; // underived indexvar can't constrain bounds
    }

    // add all relative coord bounds of nodes along derivation path to map.
    std::map<IndexVar, std::vector<ir::Expr>> relativeBounds = underivedBounds;
    addRelativeBoundsToMap(indexVar, defined, relativeBounds, variableExprs, iterators);

    // modify bounds for affected underived
    computeBoundsForUnderivedAncestors(indexVar, relativeBounds, computedCoordbounds);
  }
  return computedCoordbounds;
}

std::vector<ir::Expr> IndexVarRelGraph::deriveIterBounds(IndexVar indexVar, std::vector<IndexVar> derivedVarOrder, std::map<IndexVar, std::vector<ir::Expr>> underivedBounds,
                                                  std::map<taco::IndexVar, taco::ir::Expr> variableNames, Iterators iterators) const {
  // strategy is to start with underived variable bounds and propagate through each step on return call.
  // Define in IndexVarRel a function that takes in an Expr and produces an Expr for bound
  // for split: outer: Div(expr, splitfactor), Div(expr, splitfactor), inner: 0, splitfactor
  // what about for reordered split: same loop bounds just reordered loops (this might change for different tail strategies)

  if (isUnderived(indexVar)) {
    taco_iassert(underivedBounds.count(indexVar) == 1);
    return underivedBounds[indexVar];
  }

  std::vector<IndexVar> derivedVarOrderExceptLast = derivedVarOrder;
  if (!derivedVarOrderExceptLast.empty()) {
    derivedVarOrderExceptLast.pop_back();
  }
  taco_iassert(std::find(derivedVarOrderExceptLast.begin(), derivedVarOrderExceptLast.end(), indexVar) == derivedVarOrderExceptLast.end());

  std::map<IndexVar, std::vector<ir::Expr>> parentIterBounds;
  std::map<IndexVar, std::vector<ir::Expr>> parentCoordBounds;
  for (const IndexVar parent : getParents(indexVar)) {
    parentIterBounds[parent] = deriveIterBounds(parent, derivedVarOrder, underivedBounds, variableNames, iterators);
    vector<IndexVar> underivedParentAncestors = getUnderivedAncestors(parent);
    // TODO: this is okay for now because we don't need parentCoordBounds for fused taco_iassert(underivedParentAncestors.size() == 1);
    IndexVar underivedParent = underivedParentAncestors[0];
    parentCoordBounds[parent] = deriveCoordBounds(derivedVarOrderExceptLast, underivedBounds, variableNames, iterators)[underivedParent];
  }

  IndexVarRel rel = parentRelMap.at(indexVar);
  return rel.getNode()->deriveIterBounds(indexVar, parentIterBounds, parentCoordBounds, variableNames, iterators, *this);
}

bool IndexVarRelGraph::hasCoordBounds(IndexVar indexVar) const {
  return !isUnderived(indexVar) && isCoordVariable(indexVar);
}

// position variable if any pos relationship parent
bool IndexVarRelGraph::isPosVariable(taco::IndexVar indexVar) const {
  if (isUnderived(indexVar)) return false;
  if (parentRelMap.at(indexVar).getRelType() == POS) return true;
  for (const IndexVar parent : getParents(indexVar)) {
    if (isPosVariable(parent)) {
      return true;
    }
  }
  return false;
}

bool IndexVarRelGraph::isPosOfAccess(IndexVar indexVar, Access access) const {
  if (isUnderived(indexVar)) return false;
  if (parentRelMap.at(indexVar).getRelType() == POS) {
    return equals(parentRelMap.at(indexVar).getNode<PosRelNode>()->access, access);
  }
  for (const IndexVar parent : getParents(indexVar)) {
    if (isPosOfAccess(parent, access)) {
      return true;
    }
  }
  return false;
}

bool IndexVarRelGraph::hasPosDescendant(taco::IndexVar indexVar) const {
  if (isPosVariable(indexVar)) return true;
  if (isFullyDerived(indexVar)) return false;
  for (auto child : getChildren(indexVar)) {
    if (hasPosDescendant(child)) return true;
  }
  return false;
}

bool IndexVarRelGraph::isCoordVariable(taco::IndexVar indexVar) const {
  return !isPosVariable(indexVar);
}

std::vector<IndexVar> IndexVarRelGraph::newlyRecoverableParents(taco::IndexVar indexVar,
                                                   std::set<taco::IndexVar> previouslyDefined) const {
  // for each parent is it not recoverable with previouslyDefined, but yes with previouslyDefined+indexVar
  if (isUnderived(indexVar)) {
    return {};
  }

  std::set<taco::IndexVar> defined = previouslyDefined;
  defined.insert(indexVar);

  std::vector<IndexVar> newlyRecoverable;

  for (const IndexVar& parent : getParents(indexVar)) {
    if (parentRelMap.at(indexVar).getRelType() == FUSE) {
      IndexVar irregularDescendant;
      taco_iassert(getIrregularDescendant(indexVar, &irregularDescendant));
      if (isPosVariable(irregularDescendant)) { // Fused Pos case needs to be tracked with special while loop
        if (parent == getParents(indexVar)[0]) {
          continue;
        }
      }
    }

    if (!isRecoverable(parent, previouslyDefined) && isRecoverable(parent, defined)) {
      newlyRecoverable.push_back(parent);
      std::vector<IndexVar> parentRecoverable = newlyRecoverableParents(parent, previouslyDefined);
      newlyRecoverable.insert(newlyRecoverable.end(), parentRecoverable.begin(), parentRecoverable.end());
    }
  }
  return newlyRecoverable;
}

std::vector<IndexVar> IndexVarRelGraph::derivationPath(taco::IndexVar ancestor, taco::IndexVar indexVar) const {
  if (ancestor == indexVar) {
    return {indexVar};
  }

  for (IndexVar child : getChildren(ancestor)) {
    std::vector<IndexVar> childResult = derivationPath(child, indexVar);
    if (!childResult.empty()) {
      childResult.insert(childResult.begin(), ancestor);
      return childResult;
    }
  }
  // wrong path taken
  return {};
}

ir::Expr IndexVarRelGraph::recoverVariable(taco::IndexVar indexVar,
                                           std::vector<IndexVar> definedVarOrder,
                                           std::map<IndexVar, std::vector<ir::Expr>> underivedBounds,
                                           std::map<taco::IndexVar, taco::ir::Expr> childVariables,
                                           Iterators iterators) const {
  if (isFullyDerived(indexVar)) {
    return ir::Expr();
  }

  IndexVarRel rel = childRelMap.at(indexVar);

  std::map<IndexVar, std::vector<ir::Expr>> parentCoordBounds;
  for (IndexVar parent : rel.getNode()->getParents()) {
    vector<IndexVar> underivedParentAncestors = getUnderivedAncestors(parent);
    //TODO: taco_iassert(underivedParentAncestors.size() == 1);
    IndexVar underivedParent = underivedParentAncestors[0];

    parentCoordBounds[parent] = deriveCoordBounds(definedVarOrder, underivedBounds, childVariables, iterators)[underivedParent];
  }

  return rel.getNode()->recoverVariable(indexVar, childVariables, iterators, parentCoordBounds, *this);
}

ir::Stmt IndexVarRelGraph::recoverChild(taco::IndexVar indexVar,
                                        std::map<taco::IndexVar, taco::ir::Expr> relVariables, bool emitVarDecl, Iterators iterators) const {
  if (isUnderived(indexVar)) {
    return ir::Stmt();
  }

  IndexVarRel rel = parentRelMap.at(indexVar);
  return rel.getNode()->recoverChild(indexVar, relVariables, emitVarDecl, iterators, *this);
}

std::set<IndexVar> IndexVarRelGraph::getAllIndexVars() const {
  std::set<IndexVar> results;
  for (auto rel : parentRelMap) {
    results.insert(rel.first);
  }
  
  for (auto rel : childRelMap) {
    results.insert(rel.first);
  }
  return results;
}

// class TensorVar
struct TensorVar::Content {
  string name;
  Type type;
  Format format;
  Schedule schedule;
};

TensorVar::TensorVar() : content(nullptr) {
}

static Format createDenseFormat(const Type& type) {
  return Format(vector<ModeFormatPack>(type.getOrder(), ModeFormat(Dense)));
}

TensorVar::TensorVar(const Type& type)
: TensorVar(type, createDenseFormat(type)) {
}

TensorVar::TensorVar(const std::string& name, const Type& type)
: TensorVar(name, type, createDenseFormat(type)) {
}

TensorVar::TensorVar(const Type& type, const Format& format)
    : TensorVar(util::uniqueName('A'), type, format) {
}

TensorVar::TensorVar(const string& name, const Type& type, const Format& format)
    : content(new Content) {
  content->name = name;
  content->type = type;
  content->format = format;
}

std::string TensorVar::getName() const {
  return content->name;
}

int TensorVar::getOrder() const {
  return content->type.getShape().getOrder();
}

const Type& TensorVar::getType() const {
  return content->type;
}

const Format& TensorVar::getFormat() const {
  return content->format;
}

const Schedule& TensorVar::getSchedule() const {
  struct GetSchedule : public IndexNotationVisitor {
    using IndexNotationVisitor::visit;
    Schedule schedule;
    void visit(const BinaryExprNode* expr) {
      auto workspace = expr->getWorkspace();
      if (workspace.defined()) {
        schedule.addPrecompute(workspace);
      }
    }
  };
  GetSchedule getSchedule;
  content->schedule.clearPrecomputes();
  getSchedule.schedule = content->schedule;
  return content->schedule;
}

void TensorVar::setName(std::string name) {
  content->name = name;
}

bool TensorVar::defined() const {
  return content != nullptr;
}

const Access TensorVar::operator()(const std::vector<IndexVar>& indices) const {
  taco_uassert((int)indices.size() == getOrder()) <<
      "A tensor of order " << getOrder() << " must be indexed with " <<
      getOrder() << " variables, but is indexed with:  " << util::join(indices);
  return Access(new AccessNode(*this, indices));
}

Access TensorVar::operator()(const std::vector<IndexVar>& indices) {
  taco_uassert((int)indices.size() == getOrder()) <<
      "A tensor of order " << getOrder() << " must be indexed with " <<
      getOrder() << " variables, but is indexed with:  " << util::join(indices);
  return Access(new AccessNode(*this, indices));
}

Assignment TensorVar::operator=(IndexExpr expr) {
  taco_uassert(getOrder() == 0)
      << "Must use index variable on the left-hand-side when assigning an "
      << "expression to a non-scalar tensor.";
  Assignment assignment = Assignment(*this, {}, expr);
  check(assignment);
  return assignment;
}

Assignment TensorVar::operator+=(IndexExpr expr) {
  taco_uassert(getOrder() == 0)
      << "Must use index variable on the left-hand-side when assigning an "
      << "expression to a non-scalar tensor.";
  Assignment assignment = Assignment(*this, {}, expr, new AddNode);
  check(assignment);
  return assignment;
}

bool operator==(const TensorVar& a, const TensorVar& b) {
  return a.content == b.content;
}

bool operator<(const TensorVar& a, const TensorVar& b) {
  return a.content < b.content;
}

std::ostream& operator<<(std::ostream& os, const TensorVar& var) {
  return os << var.getName() << " : " << var.getType();
}


static bool isValid(Assignment assignment, string* reason) {
  INIT_REASON(reason);
  auto rhs = assignment.getRhs();
  auto lhs = assignment.getLhs();
  auto result = lhs.getTensorVar();
  auto freeVars = lhs.getIndexVars();
  auto shape = result.getType().getShape();
  if(!error::dimensionsTypecheck(freeVars, rhs, shape)) {
    *reason = error::expr_dimension_mismatch + " " +
              error::dimensionTypecheckErrors(freeVars, rhs, shape);
    return false;
  }
  return true;
}

// functions
bool isEinsumNotation(IndexStmt stmt, std::string* reason) {
  INIT_REASON(reason);

  if (!isa<Assignment>(stmt)) {
    *reason = "Einsum notation statements must be assignments.";
    return false;
  }

  if (!isValid(to<Assignment>(stmt), reason)) {
    return false;
  }

  // Einsum notation until proved otherwise
  bool isEinsum = true;

  // Additions are not allowed under the first multiplication
  bool mulnodeVisited = false;

  match(stmt,
    std::function<void(const AddNode*,Matcher*)>([&](const AddNode* op,
                                                     Matcher* ctx) {
      if (mulnodeVisited) {
        *reason = "additions in einsum notation must not be nested under "
                  "multiplications";
        isEinsum = false;
      }
      else {
        ctx->match(op->a);
        ctx->match(op->b);
      }
    }),
    std::function<void(const SubNode*,Matcher*)>([&](const SubNode* op,
                                                     Matcher* ctx) {
      if (mulnodeVisited) {
        *reason = "subtractions in einsum notation must not be nested under "
                  "multiplications";
        isEinsum = false;
      }
      else {
        ctx->match(op->a);
        ctx->match(op->b);
      }
    }),
    std::function<void(const MulNode*,Matcher*)>([&](const MulNode* op,
                                                     Matcher* ctx) {
      bool topMulNode = !mulnodeVisited;
      mulnodeVisited = true;
      ctx->match(op->a);
      ctx->match(op->b);
      if (topMulNode) {
        mulnodeVisited = false;
      }
    }),
    std::function<void(const BinaryExprNode*)>([&](const BinaryExprNode* op) {
      *reason = "einsum notation may not contain " + op->getOperatorString() +
                " operations";
      isEinsum = false;
    }),
    std::function<void(const ReductionNode*)>([&](const ReductionNode* op) {
      *reason = "einsum notation may not contain reductions";
      isEinsum = false;
    })
  );
  return isEinsum;
}

bool isReductionNotation(IndexStmt stmt, std::string* reason) {
  INIT_REASON(reason);

  if (!isa<Assignment>(stmt)) {
    *reason = "reduction notation statements must be assignments";
    return false;
  }

  if (!isValid(to<Assignment>(stmt), reason)) {
    return false;
  }

  // Reduction notation until proved otherwise
  bool isReduction = true;

  util::ScopedMap<IndexVar,int> boundVars;  // (int) value not used
  for (auto& var : to<Assignment>(stmt).getFreeVars()) {
    boundVars.insert({var,0});
  }

  match(stmt,
    std::function<void(const ReductionNode*,Matcher*)>([&](
        const ReductionNode* op, Matcher* ctx) {
      boundVars.scope();
      boundVars.insert({op->var,0});
      ctx->match(op->a);
      boundVars.unscope();
    }),
    std::function<void(const AccessNode*)>([&](const AccessNode* op) {
      for (auto& var : op->indexVars) {
        if (!boundVars.contains(var)) {
          *reason = "all reduction variables in reduction notation must be "
                    "bound by a reduction expression";
          isReduction = false;
        }
      }
    })
  );
  return isReduction;
}

bool isConcreteNotation(IndexStmt stmt, std::string* reason) {
  taco_iassert(stmt.defined()) << "the index statement is undefined";
  INIT_REASON(reason);

  // Concrete notation until proved otherwise
  bool isConcrete = true;

  util::ScopedMap<IndexVar,int> boundVars;  // (int) value not used
  std::set<IndexVar> definedVars; // used to check if all variables recoverable TODO: need to actually use scope like above

  IndexVarRelGraph relGraph = IndexVarRelGraph(stmt);

  match(stmt,
    std::function<void(const ForallNode*,Matcher*)>([&](const ForallNode* op,
                                                        Matcher* ctx) {
      boundVars.scope();
      boundVars.insert({op->indexVar,0});
      definedVars.insert(op->indexVar);
      ctx->match(op->stmt);
      boundVars.unscope();
    }),
    std::function<void(const AccessNode*)>([&](const AccessNode* op) {
      for (auto& var : op->indexVars) {
        if (!boundVars.contains(var) && (relGraph.isFullyDerived(var) || !relGraph.isRecoverable(var, definedVars))) {
          *reason = "all variables in concrete notation must be bound by a "
                    "forall statement";
          isConcrete = false;
        }
      }
    }),
    std::function<void(const AssignmentNode*,Matcher*)>([&](
        const AssignmentNode* op, Matcher* ctx) {
      if(!isValid(Assignment(op), reason)) {
        isConcrete = false;
        return;
      }

      if (Assignment(op).getReductionVars().size() > 0 &&
          op->op == IndexExpr()) {
        *reason = "reduction variables in concrete notation must be dominated "
                  "by compound assignments (such as +=)";
        isConcrete = false;
        return;
      }

      ctx->match(op->lhs);
      ctx->match(op->rhs);
    }),
    std::function<void(const ReductionNode*)>([&](const ReductionNode* op) {
      *reason = "concrete notation cannot contain reduction nodes";
      isConcrete = false;
    }),
    std::function<void(const SuchThatNode*)>([&](const SuchThatNode* op) {
      const string failed_reason = "concrete notation cannot contain nested SuchThat nodes";
      if (!isa<SuchThat>(stmt)) {
        *reason = failed_reason;
        isConcrete = false;
        return;
      }
      SuchThat firstSuchThat = to<SuchThat>(stmt);
      if (firstSuchThat != op) {
        *reason = failed_reason;
        isConcrete = false;
        return;
      }
    })
  );
  return isConcrete;
}

Assignment makeReductionNotation(Assignment assignment) {
  IndexExpr expr = assignment.getRhs();
  std::vector<IndexVar> free = assignment.getLhs().getIndexVars();
  if (!isEinsumNotation(assignment)) {
    return assignment;
  }

  struct MakeReductionNotation : IndexNotationRewriter {
    MakeReductionNotation(const std::vector<IndexVar>& free)
        : free(free.begin(), free.end()){}

    std::set<IndexVar> free;
    bool onlyOneTerm;

    IndexExpr addReductions(IndexExpr expr) {
      auto vars = getIndexVars(expr);
      for (auto& var : util::reverse(vars)) {
        if (!util::contains(free, var)) {
          expr = sum(var,expr);
        }
      }
      return expr;
    }

    IndexExpr einsum(const IndexExpr& expr) {
      onlyOneTerm = true;
      IndexExpr einsumexpr = rewrite(expr);

      if (onlyOneTerm) {
        einsumexpr = addReductions(einsumexpr);
      }

      return einsumexpr;
    }

    using IndexNotationRewriter::visit;

    void visit(const AddNode* op) {
      // Sum every reduction variables over each term
      onlyOneTerm = false;

      IndexExpr a = addReductions(op->a);
      IndexExpr b = addReductions(op->b);
      if (a == op->a && b == op->b) {
        expr = op;
      }
      else {
        expr = new AddNode(a, b);
      }
    }

    void visit(const SubNode* op) {
      // Sum every reduction variables over each term
      onlyOneTerm = false;

      IndexExpr a = addReductions(op->a);
      IndexExpr b = addReductions(op->b);
      if (a == op->a && b == op->b) {
        expr = op;
      }
      else {
        expr = new SubNode(a, b);
      }
    }
  };
  return Assignment(assignment.getLhs(),
                    MakeReductionNotation(free).einsum(expr),
                    assignment.getOperator());
}

IndexStmt makeReductionNotation(IndexStmt stmt) {
  taco_iassert(isEinsumNotation(stmt));
  return makeReductionNotation(to<Assignment>(stmt));
}

IndexStmt makeConcreteNotation(IndexStmt stmt) {
  std::string reason;
  taco_iassert(isReductionNotation(stmt, &reason))
      << "Not reduction notation: " << stmt << std::endl << reason;
  taco_iassert(isa<Assignment>(stmt));

  // Free variables and reductions covering the whole rhs become top level loops
  vector<IndexVar> freeVars = to<Assignment>(stmt).getFreeVars();

  struct RemoveTopLevelReductions : IndexNotationRewriter {
    using IndexNotationRewriter::visit;

    void visit(const AssignmentNode* node) {
      // Easiest to just walk down the reduction node until we find something
      // that's not a reduction
      vector<IndexVar> topLevelReductions;
      IndexExpr rhs = node->rhs;
      while (isa<Reduction>(rhs)) {
        Reduction reduction = to<Reduction>(rhs);
        topLevelReductions.push_back(reduction.getVar());
        rhs = reduction.getExpr();
      }

      if (rhs != node->rhs) {
        stmt = Assignment(node->lhs, rhs, Add());
        for (auto& i : util::reverse(topLevelReductions)) {
          stmt = forall(i, stmt);
        }
      }
      else {
        stmt = node;
      }
    }
  };
  stmt = RemoveTopLevelReductions().rewrite(stmt);

  for (auto& i : util::reverse(freeVars)) {
    stmt = forall(i, stmt);
  }

  // Replace other reductions with where and forall statements
  struct ReplaceReductionsWithWheres : IndexNotationRewriter {
    using IndexNotationRewriter::visit;

    Reduction reduction;
    TensorVar t;

    void visit(const AssignmentNode* node) {
      reduction = Reduction();
      t = TensorVar();

      IndexExpr rhs = rewrite(node->rhs);

      // nothing was rewritten
      if (rhs == node->rhs) {
        stmt = node;
        return;
      }

      taco_iassert(t.defined() && reduction.defined());
      IndexStmt consumer = Assignment(node->lhs, rhs, node->op);
      IndexStmt producer = forall(reduction.getVar(),
                                  Assignment(t, reduction.getExpr(),
                                             reduction.getOp()));
      stmt = where(rewrite(consumer), rewrite(producer));
    }

    void visit(const ReductionNode* node) {
      // only rewrite one reduction at a time
      if (reduction.defined()) {
        expr = node;
        return;
      }

      reduction = node;
      t = TensorVar("t" + util::toString(node->var),
                    node->getDataType());
      expr = t;
    }
  };
  stmt = ReplaceReductionsWithWheres().rewrite(stmt);
  return stmt;
}


vector<TensorVar> getResults(IndexStmt stmt) {
  vector<TensorVar> result;
  set<TensorVar> collected;

  for (auto& access : getResultAccesses(stmt).first) {
    TensorVar tensor = access.getTensorVar();
    taco_iassert(!util::contains(collected, tensor));
    collected.insert(tensor);
    result.push_back(tensor);
  }

  return result;
}


vector<TensorVar> getArguments(IndexStmt stmt) {
  vector<TensorVar> result;
  set<TensorVar> collected;

  for (auto& access : getArgumentAccesses(stmt)) {
    TensorVar tensor = access.getTensorVar();
    if (!util::contains(collected, tensor)) {
      collected.insert(tensor);
      result.push_back(tensor);
    }
  }

  return result;
}

std::vector<TensorVar> getTemporaries(IndexStmt stmt) {
  vector<TensorVar> temporaries;
  bool firstAssignment = true;
  match(stmt,
    function<void(const AssignmentNode*)>([&](const AssignmentNode* op) {
      // Ignore the first assignment as its lhs is the result and not a temp.
      if (firstAssignment) {
        firstAssignment = false;
        return;
      }
      temporaries.push_back(op->lhs.getTensorVar());
    }),
    function<void(const SequenceNode*,Matcher*)>([&](const SequenceNode* op,
                                                     Matcher* ctx) {
      if (firstAssignment) {
        ctx->match(op->definition);
        firstAssignment = true;
        ctx->match(op->mutation);
      }
      else {
        ctx->match(op->definition);
        ctx->match(op->mutation);
      }
    }),
    function<void(const MultiNode*,Matcher*)>([&](const MultiNode* op,
                                                  Matcher* ctx) {
      if (firstAssignment) {
        ctx->match(op->stmt1);
        firstAssignment = true;
        ctx->match(op->stmt2);
      }
      else {
        ctx->match(op->stmt1);
        ctx->match(op->stmt2);
      }
    }),
    function<void(const WhereNode*,Matcher*)>([&](const WhereNode* op,
                                                  Matcher* ctx) {
      ctx->match(op->consumer);
      ctx->match(op->producer);
    })
  );
  return temporaries;
}


std::vector<TensorVar> getTensorVars(IndexStmt stmt) {
  vector<TensorVar> results = getResults(stmt);
  vector<TensorVar> arguments = getArguments(stmt);
  vector<TensorVar> temps = getTemporaries(stmt);
  return util::combine(results, util::combine(arguments, temps));
}


pair<vector<Access>,set<Access>> getResultAccesses(IndexStmt stmt)
{
  vector<Access> result;
  set<Access> reduced;

  match(stmt,
    function<void(const AssignmentNode*)>([&](const AssignmentNode* op) {
      taco_iassert(!util::contains(result, op->lhs));
      result.push_back(op->lhs);
      if (op->op.defined()) {
        reduced.insert(op->lhs);
      }
    }),
    function<void(const WhereNode*,Matcher*)>([&](const WhereNode* op,
                                                  Matcher* ctx) {
      ctx->match(op->consumer);
    }),
    function<void(const SequenceNode*,Matcher*)>([&](const SequenceNode* op,
                                                     Matcher* ctx) {
      ctx->match(op->definition);
    })
  );
  return {result, reduced};
}


std::vector<Access> getArgumentAccesses(IndexStmt stmt)
{
  vector<Access> result;
  set<TensorVar> temporaries = util::toSet(getTemporaries(stmt));

  match(stmt,
    function<void(const AccessNode*)>([&](const AccessNode* n) {
      if (util::contains(temporaries, n->tensorVar)) {
        return;
      }
      result.push_back(n);
    }),
    function<void(const AssignmentNode*,Matcher*)>([&](const AssignmentNode* n,
                                                       Matcher* ctx) {
      ctx->match(n->rhs);
    })
  );

  return result;
}

// Return corresponding underived indexvars
struct GetIndexVars : IndexNotationVisitor {
  GetIndexVars(IndexVarRelGraph relGraph) : relGraph(relGraph) {}
  vector<IndexVar> indexVars;
  set<IndexVar> seen;
  IndexVarRelGraph relGraph;

  using IndexNotationVisitor::visit;

  void add(const vector<IndexVar>& vars) {
    for (auto& var : vars) {
      std::vector<IndexVar> underivedAncestors = relGraph.getUnderivedAncestors(var);
      for (auto &underived : underivedAncestors) {
        if (!util::contains(seen, underived)) {
          seen.insert(underived);
          indexVars.push_back(underived);
        }
      }
    }
  }

  void visit(const ForallNode* node) {
    add({node->indexVar});
    IndexNotationVisitor::visit(node->stmt);
  }

  void visit(const AccessNode* node) {
    add(node->indexVars);
  }

  void visit(const AssignmentNode* node) {
    add(node->lhs.getIndexVars());
    IndexNotationVisitor::visit(node->rhs);
  }
};

vector<IndexVar> getIndexVars(IndexStmt stmt) {
  GetIndexVars visitor = GetIndexVars(IndexVarRelGraph(stmt));
  stmt.accept(&visitor);
  return visitor.indexVars;
}


vector<IndexVar> getIndexVars(IndexExpr expr) {
  GetIndexVars visitor = GetIndexVars(IndexVarRelGraph());
  expr.accept(&visitor);
  return visitor.indexVars;
}

std::vector<IndexVar> getReductionVars(IndexStmt stmt) {
  std::vector<IndexVar> reductionVars;
  match(stmt,
        function<void(const AssignmentNode*)>([&](const AssignmentNode* node) {
          util::append(reductionVars, Assignment(node).getReductionVars());
        })
  );
  return reductionVars;
}

vector<ir::Expr> createVars(const vector<TensorVar>& tensorVars,
                            map<TensorVar, ir::Expr>* vars) {
  taco_iassert(vars != nullptr);
  vector<ir::Expr> irVars;
  for (auto& var : tensorVars) {
    ir::Expr irVar = ir::Var::make(var.getName(), var.getType().getDataType(),
                                   true, true);
    irVars.push_back(irVar);
    vars->insert({var, irVar});
  }
  return irVars;
}

struct Zero : public IndexNotationRewriterStrict {
public:
  Zero(const set<Access>& zeroed) : zeroed(zeroed) {}

private:
  using IndexExprRewriterStrict::visit;

  set<Access> zeroed;

  /// Temporary variables whose assignment has become zero.  These are therefore
  /// zero at every access site.
  set<TensorVar> zeroedVars;

  void visit(const AccessNode* op) {
    if (util::contains(zeroed, op) ||
        util::contains(zeroedVars, op->tensorVar)) {
      expr = IndexExpr();
    }
    else {
      expr = op;
    }
  }

  void visit(const LiteralNode* op) {
    expr = op;
  }

  template <class T>
  IndexExpr visitUnaryOp(const T *op) {
    IndexExpr a = rewrite(op->a);
    if (!a.defined()) {
      return IndexExpr();
    }
    else if (a == op->a) {
      return op;
    }
    else {
      return new T(a);
    }
  }

  void visit(const NegNode* op) {
    expr = visitUnaryOp(op);
  }

  void visit(const SqrtNode* op) {
    expr = visitUnaryOp(op);
  }

  template <class T>
  IndexExpr visitDisjunctionOp(const T *op) {
    IndexExpr a = rewrite(op->a);
    IndexExpr b = rewrite(op->b);
    if (!a.defined() && !b.defined()) {
      return IndexExpr();
    }
    else if (!a.defined()) {
      return b;
    }
    else if (!b.defined()) {
      return a;
    }
    else if (a == op->a && b == op->b) {
      return op;
    }
    else {
      return new T(a, b);
    }
  }

  template <class T>
  IndexExpr visitConjunctionOp(const T *op) {
    IndexExpr a = rewrite(op->a);
    IndexExpr b = rewrite(op->b);
    if (!a.defined() || !b.defined()) {
      return IndexExpr();
    }
    else if (a == op->a && b == op->b) {
      return op;
    }
    else {
      return new T(a, b);
    }
  }

  void visit(const AddNode* op) {
    expr = visitDisjunctionOp(op);
  }

  void visit(const SubNode* op) {
    IndexExpr a = rewrite(op->a);
    IndexExpr b = rewrite(op->b);
    if (!a.defined() && !b.defined()) {
      expr = IndexExpr();
    }
    else if (!a.defined()) {
      expr = -b;
    }
    else if (!b.defined()) {
      expr = a;
    }
    else if (a == op->a && b == op->b) {
      expr = op;
    }
    else {
      expr = new SubNode(a, b);
    }
  }

  void visit(const MulNode* op) {
    expr = visitConjunctionOp(op);
  }

  void visit(const DivNode* op) {
    expr = visitConjunctionOp(op);
  }

  void visit(const CastNode* op) {
    IndexExpr a = rewrite(op->a);
    if (!a.defined()) {
      expr = IndexExpr();
    }
    else if (a == op->a) {
      expr = op;
    }
    else {
      expr = new CastNode(a, op->getDataType());
    }
  }

  void visit(const CallIntrinsicNode* op) {
    std::vector<IndexExpr> args;
    std::vector<size_t> zeroArgs;
    bool rewritten = false;
    for (size_t i = 0; i < op->args.size(); ++i) {
      IndexExpr arg = op->args[i];
      IndexExpr rewrittenArg = rewrite(arg);
      if (!rewrittenArg.defined()) {
        rewrittenArg = Literal::zero(arg.getDataType());
        zeroArgs.push_back(i);
      }
      args.push_back(rewrittenArg);
      if (arg != rewrittenArg) {
        rewritten = true;
      }
    }
    const auto zeroPreservingArgsSets = op->func->zeroPreservingArgs(args);
    for (const auto& zeroPreservingArgs : zeroPreservingArgsSets) {
      taco_iassert(!zeroPreservingArgs.empty());
      if (std::includes(zeroArgs.begin(), zeroArgs.end(),
                        zeroPreservingArgs.begin(), zeroPreservingArgs.end())) {
        expr = IndexExpr();
        return;
      }
    }
    if (rewritten) {
      expr = new CallIntrinsicNode(op->func, args);
    }
    else {
      expr = op;
    }
  }

  void visit(const ReductionNode* op) {
    IndexExpr a = rewrite(op->a);
    if (!a.defined()) {
      expr = IndexExpr();
    }
    else if (a == op->a) {
      expr = op;
    }
    else {
      expr = new ReductionNode(op->op, op->var, a);
    }
  }

  void visit(const AssignmentNode* op) {
    IndexExpr rhs = rewrite(op->rhs);
    if (!rhs.defined()) {
      stmt = IndexStmt();
      zeroedVars.insert(op->lhs.getTensorVar());
    }
    else if (rhs == op->rhs) {
      stmt = op;
    }
    else {
      stmt = new AssignmentNode(op->lhs, rhs, op->op);
    }
  }

  void visit(const YieldNode* op) {
    IndexExpr expr = rewrite(op->expr);
    if (expr == op->expr) {
      stmt = op;
    }
    else {
      stmt = new YieldNode(op->indexVars, expr);
    }
  }

  void visit(const ForallNode* op) {
    IndexStmt body = rewrite(op->stmt);
    if (!body.defined()) {
      stmt = IndexStmt();
    }
    else if (body == op->stmt) {
      stmt = op;
    }
    else {
      stmt = new ForallNode(op->indexVar, body, op->parallel_unit, op->output_race_strategy);
    }
  }

  void visit(const WhereNode* op) {
    IndexStmt producer = rewrite(op->producer);
    IndexStmt consumer = rewrite(op->consumer);
    if (!consumer.defined()) {
      stmt = IndexStmt();
    }
    else if (!producer.defined()) {
      stmt = consumer;
    }
    else if (producer == op->producer && consumer == op->consumer) {
      stmt = op;
    }
    else {
      stmt = new WhereNode(consumer, producer);
    }
  }

  void visit(const SequenceNode* op) {
    taco_not_supported_yet;
  }

  void visit(const MultiNode* op) {
    taco_not_supported_yet;
  }

  void visit(const SuchThatNode* op) {
    IndexStmt body = rewrite(op->stmt);
    if (!body.defined()) {
      stmt = IndexStmt();
    }
    else if (body == op->stmt) {
      stmt = op;
    }
    else {
      stmt = new SuchThatNode(body, op->predicate);
    }
  }
};

IndexExpr zero(IndexExpr expr, const set<Access>& zeroed) {
  return Zero(zeroed).rewrite(expr);
}

IndexStmt zero(IndexStmt stmt, const std::set<Access>& zeroed) {
  return Zero(zeroed).rewrite(stmt);
}

}
