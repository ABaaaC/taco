#ifndef TACO_ERROR_H
#define TACO_ERROR_H

#include <string>
#include <sstream>
#include <string>
#include <ostream>

namespace taco {
namespace internal {

struct ErrorReport {
  enum Kind { User, Internal, Temporary };

  std::ostringstream *msg;
  const char *file;
  const char *func;
  int line;

  bool condition;
  const char *conditionString;

  Kind kind;
  bool warning;

  ErrorReport(const char *file, const char *func, int line, bool condition,
              const char *conditionString, Kind kind, bool warning);

  template<typename T>
  ErrorReport &operator<<(T x) {
    if (condition) {
      return *this;
    }
    (*msg) << x;
    return *this;
  }

  // Support for manipulators, such as std::endl
  ErrorReport &operator<<(std::ostream& (*manip)(std::ostream&)) {
    if (condition) {
      return *this;
    }
    (*msg) << manip;
    return *this;
  }

  ~ErrorReport() noexcept(false) {
    if (condition) {
      return;
    }
    explode();
  }

  void explode();
};

// internal asserts
#ifdef TACO_ASSERTS
  #define iassert(c)                                                          \
    taco::internal::ErrorReport(__FILE__, __FUNCTION__, __LINE__, (c), #c,    \
                               taco::internal::ErrorReport::Internal, false)
  #define ierror                                                              \
    taco::internal::ErrorReport(__FILE__, __FUNCTION__, __LINE__, false, NULL,\
                               taco::internal::ErrorReport::Internal, false)
#else
  struct Dummy {
    template<typename T>
    Dummy &operator<<(T x) {
      return *this;
    }
    // Support for manipulators, such as std::endl
    Dummy &operator<<(std::ostream& (*manip)(std::ostream&)) {
      return *this;
    }
  };

  #define iassert(c) taco::internal::Dummy()
  #define ierror taco::internal::Dummy()
#endif

#define unreachable                                                            \
  ierror << "reached unreachable location"

// User asserts
#define uassert(c)                                                             \
  taco::internal::ErrorReport(__FILE__,__FUNCTION__,__LINE__, (c), #c,         \
                              taco::internal::ErrorReport::User, false)
#define uerror                                                                 \
  taco::internal::ErrorReport(__FILE__,__FUNCTION__,__LINE__, false, nullptr,  \
                              taco::internal::ErrorReport::User, false)
#define uwarning                                                               \
  taco::internal::ErrorReport(__FILE__,__FUNCTION__,__LINE__, false, nullptr,  \
                              taco::internal::ErrorReport::User, true)

// Temporary assertions (planned for the future)
#define tassert(c)                                                             \
  taco::internal::ErrorReport(__FILE__, __FUNCTION__, __LINE__, (c), #c,       \
                             taco::internal::ErrorReport::Temporary, false)
#define terror                                                                 \
  taco::internal::ErrorReport(__FILE__, __FUNCTION__, __LINE__, false, NULL,   \
                             taco::internal::ErrorReport::Temporary, false)

#define not_supported_yet terror

}}

#endif
