//
//
//

#pragma once

#include "IteratorRecognition/Config.hpp"

#include "IteratorRecognition/Analysis/IteratorValueTracking.hpp"

#include "IteratorRecognition/Support/Utils/StringConversion.hpp"

#include "Pedigree/Analysis/Hazards.hpp"

#include "llvm/Support/JSON.h"
// using json::Value
// using json::Object

#include "llvm/ADT/GraphTraits.h"
// using llvm::GraphTraits

#include "llvm/Support/Debug.h"
// using LLVM_DEBUG macro
// using llvm::dbgs

#include <vector>
// using std::vector

#include <tuple>
// using std::tie

#include <utility>
// using std::pair
// using std::make_pair

#include <memory>
// using std::unique_ptr
// using std::make_unique

namespace iteratorrecognition {

struct UpdateAction {
  virtual void update() = 0;
  virtual llvm::json::Value toJSON() const = 0;
};

inline void ExecuteAction(UpdateAction &UA) { UA.update(); }

using ActionQueueT = std::vector<std::unique_ptr<UpdateAction>>;

//

template <typename T> class GraphUpdate : public UpdateAction {
  T &underlying() { return static_cast<T &>(*this); }
  T const &underlying() const { return static_cast<T const &>(*this); }

public:
  void update() override { this->underlying().performUpdate(); }

  llvm::json::Value toJSON() const override {
    return this->underlying().convertToJSON();
  }
};

template <typename T> class GraphNoop : public GraphUpdate<GraphNoop<T>> {
  T Src, Dst;

public:
  GraphNoop(T Src, T Dst) : Src(Src), Dst(Dst) {}
  GraphNoop(const GraphNoop &) = default;

  void performUpdate() {}

  llvm::json::Value convertToJSON() const {
    llvm::json::Object mapping;

    // TODO this maybe needs to be restricted with has_unit_t
    mapping["src"] = strconv::to_string(*Src->unit());
    mapping["dst"] = strconv::to_string(*Dst->unit());
    mapping["remark"] = "unknown relation";

    return std::move(mapping);
  }
};

template <typename T, typename InfoT>
class GraphEdgeConnect : public GraphUpdate<GraphEdgeConnect<T, InfoT>> {
  T Src, Dst;
  InfoT Info;

public:
  GraphEdgeConnect(T Src, T Dst, InfoT Info) : Src(Src), Dst(Dst), Info(Info) {}
  GraphEdgeConnect(const GraphEdgeConnect &) = default;

  void performUpdate() { Src->addDependentNode(Dst, Info); }

  llvm::json::Value convertToJSON() const {
    llvm::json::Object mapping;

    // TODO this maybe needs to be restricted with has_unit_t
    mapping["src"] = strconv::to_string(*Src->unit());
    mapping["dst"] = strconv::to_string(*Dst->unit());
    mapping["remark"] = "connect";

    return std::move(mapping);
  }
};

template <typename T>
class GraphEdgeDisconnect : public GraphUpdate<GraphEdgeDisconnect<T>> {
  T Src, Dst;

public:
  GraphEdgeDisconnect(T Src, T Dst) : Src(Src), Dst(Dst) {}
  GraphEdgeDisconnect(const GraphEdgeDisconnect &) = default;

  void performUpdate() { Src->removeDependentNode(Dst); }

  llvm::json::Value convertToJSON() const {
    llvm::json::Object mapping;

    // TODO this maybe needs to be restricted with has_unit_t
    mapping["src"] = strconv::to_string(*Src->unit());
    mapping["dst"] = strconv::to_string(*Dst->unit());
    mapping["remark"] = "disconnect";

    return std::move(mapping);
  }
};

template <typename T, typename InfoT>
class GraphEdgeInfoUpdate : public GraphUpdate<GraphEdgeInfoUpdate<T, InfoT>> {
  T Src, Dst;
  InfoT Info;

public:
  GraphEdgeInfoUpdate(T Src, T Dst, InfoT Info)
      : Src(Src), Dst(Dst), Info(Info) {}
  GraphEdgeInfoUpdate(const GraphEdgeInfoUpdate &) = default;

  void performUpdate() { Src->setEdgeInfo(Dst, Info); }

  llvm::json::Value convertToJSON() const {
    llvm::json::Object mapping;

    // TODO this maybe needs to be restricted with has_unit_t
    mapping["src"] = strconv::to_string(*Src->unit());
    mapping["dst"] = strconv::to_string(*Dst->unit());
    mapping["remark"] = "update info";

    return std::move(mapping);
  }
};

//

template <typename GraphT> class IteratorVarianceGraphUpdateGenerator {
  GraphT &G;
  IteratorVarianceAnalyzer &IVA;

  using GraphNodeT = typename GraphT::NodeType;
  using GraphNodeRefT = typename llvm::GraphTraits<GraphT *>::NodeRef;
  using UnitT = typename GraphNodeT::UnitType;
  using EdgeInfoT = typename GraphNodeT::EdgeInfoType::value_type;

  using UpdateActionPairT =
      std::pair<std::unique_ptr<UpdateAction>, std::unique_ptr<UpdateAction>>;

  decltype(auto) createConnect(GraphNodeRefT Src, GraphNodeRefT Dst,
                               EdgeInfoT Info) {
    return std::make_pair(
        std::make_unique<GraphEdgeConnect<GraphNodeRefT, EdgeInfoT>>(Src, Dst,
                                                                     Info),
        std::make_unique<GraphEdgeDisconnect<GraphNodeRefT>>(Src, Dst));
  }

  decltype(auto) createDisconnect(GraphNodeRefT Src, GraphNodeRefT Dst,
                                  EdgeInfoT Info) {
    return std::make_pair(
        std::make_unique<GraphEdgeDisconnect<GraphNodeRefT>>(Src, Dst),
        std::make_unique<GraphEdgeConnect<GraphNodeRefT, EdgeInfoT>>(Src, Dst,
                                                                     Info));
  }

  decltype(auto) createUpdate(GraphNodeRefT Src, GraphNodeRefT Dst,
                              EdgeInfoT NewInfo, EdgeInfoT OldInfo) {
    return std::make_pair(
        std::make_unique<GraphEdgeInfoUpdate<GraphNodeRefT, EdgeInfoT>>(
            Src, Dst, NewInfo),
        std::make_unique<GraphEdgeInfoUpdate<GraphNodeRefT, EdgeInfoT>>(
            Src, Dst, OldInfo));
  }

public:
  IteratorVarianceGraphUpdateGenerator(GraphT &G, IteratorVarianceAnalyzer &IVA)
      : G(G), IVA(IVA) {}

  decltype(auto) create(UnitT Src, UnitT Dst) {
    auto srcIV = IVA.getOrInsertVariance(Src);
    auto dstIV = IVA.getOrInsertVariance(Dst);

    auto *srcNode = G.getNode(Src);
    auto *dstNode = G.getNode(Dst);

    UpdateActionPairT actions;

    if (srcIV == IteratorVarianceValue::Unknown ||
        dstIV == IteratorVarianceValue::Unknown) {
      actions.first =
          std::make_unique<GraphNoop<GraphNodeRefT>>(srcNode, dstNode);
      actions.second =
          std::make_unique<GraphNoop<GraphNodeRefT>>(srcNode, dstNode);
    } else if (srcIV == IteratorVarianceValue::Variant ||
               dstIV == IteratorVarianceValue::Variant) {
      if (auto infoOrEmpty = srcNode->getEdgeInfo(dstNode)) {
        if (auto info = *infoOrEmpty) {
          auto memInfo = info & pedigree::DO_Memory;
          auto newInfo = info;
          newInfo.reset(pedigree::DO_Memory);

          if (memInfo) {
            if (newInfo) {
              actions = createUpdate(srcNode, dstNode, newInfo, info);
            } else {
              actions = createDisconnect(srcNode, dstNode, memInfo);
            }
          }
        }
      }
    } else if (srcIV == IteratorVarianceValue::Invariant ||
               dstIV == IteratorVarianceValue::Invariant) {
      if (!srcNode->hasEdgeWith(dstNode)) {
        if (auto newInfo = pedigree::DetermineHazard(Src, Dst)) {
          actions = createConnect(srcNode, dstNode, newInfo);
        }
      }
    }

    return actions;
  }
};

} // namespace iteratorrecognition

