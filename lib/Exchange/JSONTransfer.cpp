//
//
//

#include "IteratorRecognition/Config.hpp"

#include "IteratorRecognition/Exchange/JSONTransfer.hpp"

#include "IteratorRecognition/Support/Utils/DebugInfo.hpp"

#include "llvm/Analysis/LoopInfo.h"
// using llvm::Loop

#include "llvm/IR/Instruction.h"
// using llvm::Instruction

#include "llvm/Support/FileSystem.h"
// using llvm::sys::fs::F_Text

#include "llvm/Support/Path.h"
// using llvm::sys::path::filename

#include "llvm/Support/ToolOutputFile.h"
// using llvm::ToolOutputFile

#include "llvm/Support/Debug.h"
// using LLVM_DEBUG macro
// using llvm::dbgs
// using llvm::errs

#include <utility>
// using std::move

#include <algorithm>
// using std::transform

#include <system_error>
// using std::error_code

// namespace aliases

namespace itr = iteratorrecognition;

namespace iteratorrecognition {

void WriteJSONToFile(const llvm::json::Value &V,
                     const llvm::Twine &FilenamePrefix,
                     const llvm::Twine &Dir) {
  std::string absFilename{Dir.str() + "/" + FilenamePrefix.str() + ".json"};
  llvm::StringRef filename{llvm::sys::path::filename(absFilename)};
  llvm::errs() << "Writing file '" << filename << "'... ";

  std::error_code ec;
  llvm::ToolOutputFile of(absFilename, ec, llvm::sys::fs::F_Text);

  if (ec) {
    llvm::errs() << "error opening file '" << filename << "' for writing!\n";
    of.os().clear_error();
  }

  of.os() << llvm::formatv("{0:2}", V);
  of.os().close();

  if (!of.os().has_error()) {
    of.keep();
  }

  llvm::errs() << " done. \n";
}

} // namespace iteratorrecognition

//

namespace llvm {
namespace json {

Value toJSON(const Instruction &I) {
  std::string outs;
  raw_string_ostream ss(outs);

  ss << I;
  Value info{ss.str()};

  return std::move(info);
}

Value toJSON(const Loop &CurLoop) {
  Object infoMapping;

  std::string outs;
  raw_string_ostream ss(outs);

  ss << *CurLoop.getLoopLatch()->getTerminator();
  infoMapping["latch"] = ss.str();

  const auto &info = itr::dbg::extract(CurLoop);
  infoMapping["di"] = toJSON(info);

  return std::move(infoMapping);
}

Value toJSON(const itr::dbg::LoopDebugInfoT &Info) {
  Object infoMapping;

  infoMapping["line"] = std::get<0>(Info);
  infoMapping["column"] = std::get<1>(Info);
  infoMapping["function"] = std::get<2>(Info);
  infoMapping["filename"] = std::get<3>(Info);

  return std::move(infoMapping);
}

Value toJSON(const itr::IteratorRecognitionInfo::CondensationToLoopsMapT &Map) {
  Object root;
  Array condensations;

  for (const auto &e : Map) {
    const auto &cn = *e.getFirst();
    const auto &loops = e.getSecond();

    auto mapping = std::move(*toJSON(cn).getAsObject());

    Array loopsArray;
    std::transform(loops.begin(), loops.end(), std::back_inserter(loopsArray),
                   [&](const auto &e) { return std::move(toJSON(*e)); });
    mapping["loops"] = std::move(loopsArray);

    condensations.push_back(std::move(mapping));
  }

  root["condensations"] = std::move(condensations);

  return std::move(root);
}

Value toJSON(const itr::UpdateAction &UA) { return UA.toJSON(); }

} // namespace json
} // namespace llvm

