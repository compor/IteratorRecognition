//
//
//

#include "IteratorRecognition/Config.hpp"

#include "IteratorRecognition/Util.hpp"

#include "IteratorRecognition/Debug.hpp"

#include "IteratorRecognition/Analysis/IteratorRecognition.hpp"

#include "IteratorRecognition/Analysis/Graphs/PDGCondensationGraph.hpp"

#include "IteratorRecognition/Analysis/Passes/RecognizerPass.hpp"

#include "IteratorRecognition/Exchange/JSONTransfer.hpp"

#include "Pedigree/Analysis/Graphs/DependenceGraphs.hpp"

#include "Pedigree/Analysis/Passes/PDGraphPass.hpp"

#include "PassCommandLineOptions.hpp"

#include "llvm/Pass.h"
// using llvm::RegisterPass

#include "llvm/Analysis/LoopInfo.h"
// using llvm::Loop
// using llvm::LoopInfo
// using llvm::LoopInfoWrapperPass

#include "llvm/IR/Instruction.h"
// using llvm::Instruction

#include "llvm/IR/Function.h"
// using llvm::Function

#include "llvm/IR/LegacyPassManager.h"
// using llvm::PassManagerBase

#include "llvm/Transforms/IPO/PassManagerBuilder.h"
// using llvm::PassManagerBuilder
// using llvm::RegisterStandardPasses

#include "llvm/ADT/SCCIterator.h"
// using llvm::scc_begin
// using llvm::scc_end

#include "llvm/ADT/DenseMap.h"
// using llvm::DenseMap

#include "llvm/ADT/DenseSet.h"
// using llvm::DenseSet

#include "llvm/ADT/GraphTraits.h"
// using llvm::GraphTraits

#include "llvm/Support/CommandLine.h"
// using llvm::cl::opt
// using llvm::cl::desc
// using llvm::cl::location
// using llvm::cl::cat
// using llvm::cl::OptionCategory

#include "llvm/Support/Debug.h"
// using DEBUG macro
// using llvm::dbgs
// using llvm::errs

#define DEBUG_TYPE "iterator-recognition"

// namespace aliases

namespace itr = iteratorrecognition;

// plugin registration for opt

char itr::RecognizerPass::ID = 0;
static llvm::RegisterPass<itr::RecognizerPass>
    X("itr-recognize", PRJ_CMDLINE_DESC("iterator recognition pass"), false,
      false);

// plugin registration for clang

// the solution was at the bottom of the header file
// 'llvm/Transforms/IPO/PassManagerBuilder.h'
// create a static free-floating callback that uses the legacy pass manager to
// add an instance of this pass and a static instance of the
// RegisterStandardPasses class

static void registerRecognizerPass(const llvm::PassManagerBuilder &Builder,
                                   llvm::legacy::PassManagerBase &PM) {
  PM.add(new itr::RecognizerPass());

  return;
}

static llvm::RegisterStandardPasses
    RegisterRecognizerPass(llvm::PassManagerBuilder::EP_EarlyAsPossible,
                           registerRecognizerPass);

//

static llvm::cl::opt<bool>
    ExportSCC("itr-export-scc", llvm::cl::desc("export condensations"),
              llvm::cl::init(false),
              llvm::cl::cat(IteratorRecognitionCLCategory));

static llvm::cl::opt<bool> ExportMapping(
    "itr-export-mapping", llvm::cl::desc("export condensation to loop mapping"),
    llvm::cl::init(false), llvm::cl::cat(IteratorRecognitionCLCategory));

//

namespace iteratorrecognition {

void RecognizerPass::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
  AU.addRequired<llvm::LoopInfoWrapperPass>();
  AU.addRequired<pedigree::PDGraphPass>();

  AU.setPreservesAll();
}

bool RecognizerPass::runOnFunction(llvm::Function &CurFunc) {
  bool hasChanged = false;

  const auto *LI = &getAnalysis<llvm::LoopInfoWrapperPass>().getLoopInfo();
  if (LI->empty()) {
    return hasChanged;
  }

  pedigree::PDGraph &Graph{getAnalysis<pedigree::PDGraphPass>().getGraph()};

  Graph.connectRootNode();

  CondensationGraph<pedigree::PDGraph *> CG{llvm::scc_begin(&Graph),
                                            llvm::scc_end(&Graph)};

  if (ExportSCC) {
    ExportCondensations(CG, CurFunc.getName());
  }

  llvm::DenseMap<typename llvm::GraphTraits<decltype(CG)>::NodeRef,
                 llvm::DenseSet<llvm::Loop *>>
      CondensationToLoop;
  MapCondensationToLoop(CG, *LI, CondensationToLoop);

  if (ExportMapping) {
    ExportCondensationToLoopMapping(CondensationToLoop, CurFunc.getName());
  }

  // TODO example dependent declaration that can potentially be used for
  // static_assert
  // llvm::DenseMap<llvm::Loop *,
  // llvm::SmallVector<typename decltype(CG)::MemberNodeRef, 8>>
  // iterators;
  RecognizeIterator(CG, CondensationToLoop, Iterators);

  return hasChanged;
}

} // namespace iteratorrecognition