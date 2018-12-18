//
//
//

#pragma once

#include "llvm/ADT/SmallPtrSet.h"
// using llvm::SmallPtrSetImpl

#include "llvm/IR/Instruction.h"
// using llvm::Instruction

#include "IteratorRecognition/Config.hpp"

#include "IteratorRecognition/Analysis/IteratorRecognition.hpp"

#include "IteratorRecognition/Support/Utils/InstTraversal.hpp"

namespace iteratorrecognition {

void FindIteratorVars(const IteratorInfo &Info,
                      llvm::SmallPtrSetImpl<llvm::Value *> &Values) {
  auto &loopBlocks = Info.getLoop()->getBlocksSet();

  for (auto *e : Info) {
    for (auto &u : e->uses()) {
      auto *user = llvm::dyn_cast<llvm::Instruction>(u.getUser());
      if (user && !Info.isIterator(user) &&
          loopBlocks.count(user->getParent())) {
        Values.insert(e);
      }
    }
  }
}

void FindPayloadVars(const IteratorInfo &Info,
                     llvm::SmallPtrSetImpl<llvm::Value *> &Values) {
  auto &loopBlocks = Info.getLoop()->getBlocksSet();
  auto loopInsts = make_loop_inst_range(Info.getLoop());

  for (auto &e : loopInsts) {
    if (Info.isIterator(&e)) {
      break;
    }

    for (auto &u : e.uses()) {
      auto *user = llvm::dyn_cast<llvm::Instruction>(u.getUser());
      if (user && !Info.isIterator(user)) {
        Values.insert(&e);
      }
    }
  }
}

} // namespace iteratorrecognition