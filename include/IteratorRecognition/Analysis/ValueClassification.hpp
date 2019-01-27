//
//
//

#pragma once

#include "IteratorRecognition/Config.hpp"

#include "llvm/ADT/SmallPtrSet.h"
// using llvm::SmallPtrSetImpl

namespace llvm {
class Instruction;
class DominatorTree;
} // namespace llvm

namespace iteratorrecognition {

class IteratorInfo;

void FindIteratorVars(const IteratorInfo &Info,
                      llvm::SmallPtrSetImpl<llvm::Instruction *> &Values);

void FindPayloadVars(const IteratorInfo &Info,
                     llvm::SmallPtrSetImpl<llvm::Instruction *> &Values);

void FindMemPayloadLiveVars(
    const llvm::SmallPtrSetImpl<llvm::Instruction *> &PayloadValues,
    llvm::SmallPtrSetImpl<llvm::Instruction *> &MemLiveInThru,
    llvm::SmallPtrSetImpl<llvm::Instruction *> &MemLiveOut);

void FindVirtRegPayloadLiveVars(
    const IteratorInfo &Info,
    const llvm::SmallPtrSetImpl<llvm::Instruction *> &PayloadValues,
    llvm::SmallPtrSetImpl<llvm::Instruction *> &VirtRegLive);

void SplitVirtRegPayloadLiveVars(
    const IteratorInfo &Info,
    const llvm::SmallPtrSetImpl<llvm::Instruction *> &PayloadValues,
    const llvm::SmallPtrSetImpl<llvm::Instruction *> &VirtRegLive,
    const llvm::DominatorTree &DT,
    llvm::SmallPtrSetImpl<llvm::Instruction *> &VirtRegLiveIn,
    llvm::SmallPtrSetImpl<llvm::Instruction *> &VirtRegLiveThru,
    llvm::SmallPtrSetImpl<llvm::Instruction *> &VirtRegLiveOut);

void FindPayloadTempAndLiveVars(
    const IteratorInfo &Info,
    const llvm::SmallPtrSetImpl<llvm::Instruction *> &PayloadValues,
    llvm::SmallPtrSetImpl<llvm::Instruction *> &TempValues,
    llvm::SmallPtrSetImpl<llvm::Instruction *> &LiveValues);

void FindDirectUsesOfIn(
    const llvm::SmallPtrSetImpl<llvm::Instruction *> &Values,
    const llvm::SmallPtrSetImpl<llvm::Instruction *> &OtherValues,
    llvm::SmallPtrSetImpl<llvm::Instruction *> &DirectUserValues);

} // namespace iteratorrecognition
