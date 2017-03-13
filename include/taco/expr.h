#ifndef TACO_EXPR_H
#define TACO_EXPR_H

#include <iostream>
#include <string>
#include <typeinfo>

#include "taco/util/error.h"
#include "taco/util/intrusive_ptr.h"
#include "taco/util/uncopyable.h"

namespace taco {

namespace internal {
class ExprVisitorStrict;

struct TENode : public util::Manageable<TENode>, private util::Uncopyable {
  virtual ~TENode() = default;
  virtual void accept(ExprVisitorStrict*) const = 0;
  virtual void print(std::ostream& os) const = 0;
};
}  // namespace internal

class Expr : public util::IntrusivePtr<const internal::TENode> {
public:

  Expr() : util::IntrusivePtr<const internal::TENode>(nullptr) {}
  Expr(const internal::TENode* n)
      : util::IntrusivePtr<const internal::TENode>(n) {}

  Expr(int);
  Expr(float);
  Expr(double);

  Expr operator-();

  void accept(internal::ExprVisitorStrict *) const;
  friend std::ostream& operator<<(std::ostream&, const Expr&);
};

/// Expr type check/conversion functions
template <typename T>
inline bool isa(Expr e) {
  return e.defined() && dynamic_cast<const typename T::Node*>(e.ptr) != nullptr;
}

template <typename T>
inline const T to(Expr e) {
  iassert(isa<T>(e)) << "Cannot convert " << typeid(e).name()
                     << " to " << typeid(T).name();
  return T(static_cast<const typename T::Node*>(e.ptr));
}

}
#endif
