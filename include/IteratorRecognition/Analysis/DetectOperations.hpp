//
//
//

#pragma once

#include "IteratorRecognition/Config.hpp"

#include "IteratorRecognition/Support/ShadowDependenceGraph.hpp"

#include "llvm/Analysis/LoopInfo.h"
// using llvm::Loop

#include "llvm/IR/Instruction.h"
// using llvm::Instruction

#include "llvm/IR/Instructions.h"

#include "llvm/ADT/SmallPtrSet.h"
// using llvm::SmallPtrSet

#include "llvm/ADT/SetVector.h"
// using llvm::SetVector

#include "llvm/Support/raw_ostream.h"

#include "llvm/Support/Debug.h"
// using llvm::dbgs

#define DEBUG_TYPE "itr-detectops"

namespace iteratorrecognition {

struct LoadModifyStore {
  llvm::Instruction *Target;
  llvm::SmallPtrSet<llvm::Instruction *, 8> Sources;
  llvm::SmallPtrSet<llvm::Instruction *, 16> Operations;
};

class MutationDetector {
public:
  template <typename GraphT>
  void process(const SDependenceGraph<GraphT> &SDG, const llvm::Loop &CurLoop,
               LoadModifyStore &LMS) {
    llvm::SetVector<llvm::Value *> workList;
    llvm::SmallPtrSet<llvm::Value *, 32> visited;

    workList.insert(LMS.Target);

    while (!workList.empty()) {
      auto *curTarget =
          llvm::dyn_cast<llvm::Instruction>(workList.pop_back_val());

      if (!curTarget || visited.count(curTarget)) {
        continue;
      }
      visited.insert(curTarget);

      const auto *curTargetNode = SDG.findNodeFor(curTarget);
      if (!curTargetNode) {
        continue;
      }

      llvm::SmallPtrSet<llvm::Value *, 16> inverse_edge_units;
      for (auto *ie : curTargetNode->inverse_edges()) {
        for (auto *iu : ie->units()) {
          inverse_edge_units.insert(iu);
        }
      }

      if (auto *ii = llvm::dyn_cast<llvm::StoreInst>(curTarget)) {
        // TODO maybe this should be the next target
        if (inverse_edge_units.count(ii->getValueOperand())) {
          workList.insert(ii->getValueOperand());
        }
      } else if (auto *ii = llvm::dyn_cast<llvm::LoadInst>(curTarget)) {
        // TODO maybe this should be the next target
        LMS.Sources.insert(ii);
      } else if (auto *ii = llvm::dyn_cast<llvm::BinaryOperator>(curTarget)) {
        for (auto &op : ii->operands()) {
          if (inverse_edge_units.count(op.get())) {
            workList.insert(op);
          }
        }
        LMS.Operations.insert(ii);
      } else if (auto *ii = llvm::dyn_cast<llvm::CmpInst>(curTarget)) {
        for (auto &op : ii->operands()) {
          if (inverse_edge_units.count(op.get())) {
            workList.insert(op);
          }
        }
        LMS.Operations.insert(ii);
      } else if (auto *ii = llvm::dyn_cast<llvm::SelectInst>(curTarget)) {
        if (inverse_edge_units.count(ii->getTrueValue())) {
          workList.insert(ii->getTrueValue());
        }
        if (inverse_edge_units.count(ii->getFalseValue())) {
          workList.insert(ii->getFalseValue());
        }
        // do not insert as operation
        // skip condition
        // just aggregate the operands to unify the cfg
      } else if (auto *ii =
                     llvm::dyn_cast<llvm::GetElementPtrInst>(curTarget)) {
        for (auto &op : ii->operands()) {
          if (inverse_edge_units.count(op.get())) {
            workList.insert(op);
          }
        }
        LMS.Operations.insert(ii);
      } else if (auto *ii = llvm::dyn_cast<llvm::CastInst>(curTarget)) {
        for (auto &op : ii->operands()) {
          if (inverse_edge_units.count(op.get())) {
            workList.insert(op.get());
          }
        }
        LMS.Operations.insert(ii);
      } else if (auto *ii = llvm::dyn_cast<llvm::PHINode>(curTarget)) {
        if (ii->getBasicBlockIndex(CurLoop.getHeader()) > -1) {
          LMS.Sources.insert(ii);
        } else {
          for (auto &op : ii->incoming_values()) {
            if (inverse_edge_units.count(op)) {
              workList.insert(op);
            }
          }
          LMS.Operations.insert(ii);
        }
      } else {
        LLVM_DEBUG(llvm::dbgs()
                       << "unhandled instruction: " << *curTarget << '\n';);
        // TODO see what to do with this
        break;
      }
    } // work list iteration end

    LMS.Operations.erase(LMS.Target);
  }
};

} // namespace iteratorrecognition

#undef DEBUG_TYPE
