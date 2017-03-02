#ifndef SRC_UTIL_ENV_H_
#define SRC_UTIL_ENV_H_

#include <string>
#include <unistd.h>

#include "error.h"

namespace taco {
namespace util {
std::string getFromEnv(std::string flag, std::string dflt);
std::string getTmpdir();

std::string getFromEnv(std::string flag, std::string dflt) {
  char const *ret = getenv(flag.c_str());
  if (!ret) {
    return dflt;
  } else {
    return std::string(ret);
  }
}

std::string getTmpdir() {
  // use POSIX logic for finding a temp dir
  auto tmpdir = getFromEnv("TMPDIR", "/tmp/");
  
  // if the directory does not have a trailing slash, add one
  if (tmpdir.back() != '/') {
    tmpdir += '/';
  }

  uassert(access(tmpdir.c_str(), W_OK) == 0) <<
    "Unable to write to temporary directory for code generation. "
    "Please set the environment variable TMPDIR to somewhere writable";

  return tmpdir;
}

}}

#endif /* SRC_UTIL_ENV_H_ */
