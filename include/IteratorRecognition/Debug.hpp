//
//
//

#ifndef ITR_DEBUG_HPP
#define ITR_DEBUG_HPP

#include "IteratorRecognition/Config.hpp"

#define DEFINE_DEBUG_LEVELS                                                    \
  enum class LogLevel { Info, Notice, Warning, Error, Debug }

#if ITERATORRECOGNITION_DEBUG

#include "llvm/IR/Function.h"
// using llvm::Function

#include "llvm/Support/FileSystem.h"
// using llvm::sys::fs::OpenFlags

#include "llvm/Support/raw_ostream.h"
// using llvm::errs
// using llvm::raw_fd_ostream

#include <cstdio>
// using std::tmpnam

#include <system_error>
// using std::error_code

DEFINE_DEBUG_LEVELS;

namespace iteratorrecognition {
namespace debug {

extern bool passDebugFlag;
extern LogLevel passLogLevel;

} // namespace debug
} // namespace iteratorrecognition

#define DEBUG_MSG(L, STR)                                                      \
  do {                                                                         \
    if (iteratorrecognition::debug::passDebugFlag &&                           \
        L <= iteratorrecognition::debug::passLogLevel)                         \
      llvm::errs() << STR;                                                     \
  } while (false)

#define DEBUG_CMD(L, C)                                                        \
  do {                                                                         \
    if (iteratorrecognition::debug::passDebugFlag &&                           \
        L <= iteratorrecognition::debug::passLogLevel)                         \
      C;                                                                       \
  } while (false)

namespace iteratorrecognition {
namespace debug {

static bool dumpFunction(const llvm::Function *CurFunc = nullptr) {
  if (!CurFunc)
    return false;

  std::error_code ec;
  char filename[L_tmpnam];
  std::tmpnam(filename);

  llvm::raw_fd_ostream dbgFile(filename, ec, llvm::sys::fs::F_Text);

  llvm::errs() << "\nfunction dumped at: " << filename << "\n";
  CurFunc->print(dbgFile);

  return false;
}

} // namespace debug
} // namespace iteratorrecognition

#else

#define DEBUG_MSG(L, S)                                                        \
  do {                                                                         \
  } while (false)

#define DEBUG_CMD(L, C)                                                        \
  do {                                                                         \
  } while (false)

namespace llvm {
class Function;
} // namespace llvm

DEFINE_DEBUG_LEVELS;

namespace iteratorrecognition {
namespace debug {

static constexpr bool dumpFunction(const llvm::Function *CurFunc = nullptr) {
  return true;
}

} // namespace debug
} // namespace iteratorrecognition

#endif // ITR_DEBUG

#endif // header