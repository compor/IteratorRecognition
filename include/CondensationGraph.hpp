//
//
//

#include "Config.hpp"

#include "Debug.hpp"

#include "Analysis/Graphs/PDGraph.hpp"

#include "llvm/ADT/DenseMap.h"
// using llvm::DenseMap

#include "llvm/ADT/EquivalenceClasses.h"
// using llvm::EquivalenceClasses

#include "llvm/ADT/iterator_range.h"
// using llvm::make_range

#include "llvm/ADT/GraphTraits.h"
// using llvm::GraphTraits

// TODO remove
#include "llvm/Support/Debug.h"
// using llvm::dbgs

#include <boost/iterator/iterator_facade.hpp>
// using boost::iterator_facade
// using boost::bidirectional_traversal_tag
// using boost::iterator_core_access

#include <vector>
// using std::vector

#include <iterator>
// using std::iterator_traits

#include <type_traits>
// using std::is_trivially_copyable
// using std::is_same
// using std::conditional_t
// using std::is_pointer
// using std::remove_pointer_t
// using std::remove_reference_t
// using std::is_class

#include <utility>
// using std::forward

#ifndef ITR_CONDENSATIONGRAPH_HPP
#define ITR_CONDENSATIONGRAPH_HPP

namespace itr {

// TODO wrap this in a node type for the sake of graph traits

template <typename GraphT>
using CondensationType =
    std::vector<typename llvm::GraphTraits<GraphT>::NodeRef>;

template <typename GraphT>
using CondensationVectorType = std::vector<CondensationType<GraphT>>;

template <typename GraphT>
using ConstCondensationVectorType = std::vector<const CondensationType<GraphT>>;

template <typename GraphT, typename GT = llvm::GraphTraits<GraphT>>
class CondensationGraph {
  using EdgesContainerType =
      llvm::DenseMap<typename GT::NodeRef,
                     typename std::vector<typename GT::NodeRef>>;

public:
  using NodeRef = typename GT::NodeRef;
  using EdgeIteratorType = typename EdgesContainerType::mapped_type::iterator;
  using EdgeRef = typename std::iterator_traits<EdgeIteratorType>::reference;

  static_assert(std::is_trivially_copyable<NodeRef>::value,
                "NodeRef is not trivially copyable!");

private:
  llvm::EquivalenceClasses<NodeRef> Nodes;
  EdgesContainerType OutEdges;
  EdgesContainerType InEdges;

  class CondensationGraphIterator
      : public boost::iterator_facade<
            CondensationGraphIterator, const NodeRef,
            typename std::iterator_traits<typename decltype(
                Nodes)::iterator>::iterator_category> {
  public:
    using base_iterator = typename decltype(Nodes)::iterator;

    const CondensationGraph *CurrentCG;
    base_iterator CurrentIt;

    CondensationGraphIterator(const CondensationGraph &CG, bool IsEnd = false)
        : CurrentCG(&CG), CurrentIt(IsEnd ? CG.Nodes.end() : CG.Nodes.begin()) {
    }

  private:
    friend class boost::iterator_core_access;

    void increment() { ++CurrentIt; };
    void decrement() { --CurrentIt; };
    bool equal(const CondensationGraphIterator &Other) const {
      return CurrentCG == Other.CurrentCG && CurrentIt == Other.CurrentIt;
    }

    const NodeRef &dereference() const {
      return *CurrentCG->Nodes.findLeader(CurrentIt);
    }
  };

  template <typename IteratorT>
  void addCondensedNode(IteratorT Begin, IteratorT End) {
    // TODO this might be required to accommodate implicit conversions
    static_assert(
        std::is_same<typename std::iterator_traits<IteratorT>::value_type,
                     NodeRef>::value,
        "Iterator type cannot be dereferenced to the expected value!");

    if (Begin == End)
      return;

    for (auto it = Begin; it != End; ++it)
      Nodes.unionSets(*Begin, *it);
  }

  void connectEdges() {
    for (auto &curLeader : nodes()) {
      llvm::DenseSet<NodeRef> srcLeaders, dstLeaders;

      for (auto &m : scc_members(curLeader)) {
        // TODO refactor these two inner loops since they are basically the same
        for (const auto &n : m->pred_nodes()) {
          auto &srcLeader = *Nodes.findLeader(n);

          if (srcLeader == curLeader || srcLeaders.count(srcLeader)) {
            continue;
          }

          srcLeaders.insert(srcLeader);
        }

        for (const auto &n : m->nodes()) {
          auto &dstLeader = *Nodes.findLeader(n);

          if (dstLeader == curLeader || dstLeaders.count(dstLeader)) {
            continue;
          }

          dstLeaders.insert(dstLeader);
        }
      }

      InEdges.try_emplace(curLeader, srcLeaders.begin(), srcLeaders.end());
      OutEdges.try_emplace(curLeader, dstLeaders.begin(), dstLeaders.end());
    }
  }

public:
  explicit CondensationGraph() = default;

  template <typename IteratorT>
  explicit CondensationGraph(IteratorT Begin, IteratorT End) {
    if (Begin == End) {
      return;
    }

    for (auto &scc : llvm::make_range(Begin, End)) {
      addCondensedNode(std::begin(scc), std::end(scc));
    }

    connectEdges();
  }

  CondensationGraph(const CondensationGraph &G) = default;

  decltype(auto) getEntryNode() const {
    return *(Nodes.member_begin(Nodes.begin()));
  }

  decltype(auto) size() const { return Nodes.getNumClasses(); }
  bool empty() const { return Nodes.empty(); }

  using nodes_iterator = CondensationGraphIterator;

  decltype(auto) nodes_begin() const { return nodes_iterator(*this); }
  decltype(auto) nodes_end() const { return nodes_iterator(*this, true); }

  decltype(auto) nodes() const {
    return llvm::make_range(nodes_begin(), nodes_end());
  }

  using scc_members_iterator = typename decltype(Nodes)::member_iterator;

  decltype(auto) scc_members_begin(nodes_iterator It) {
    return Nodes.member_begin(Nodes.findLeader(It));
  }

  decltype(auto) scc_members_begin(NodeRef Elem) {
    return Nodes.findLeader(Elem);
  }

  decltype(auto) scc_members_end() { return Nodes.member_end(); }

  template <typename T> decltype(auto) scc_members(T &&E) {
    return llvm::make_range(scc_members_begin(std::forward<T>(E)),
                            scc_members_end());
  }

  // TODO these need to be moved to a node type or associated traits class
  EdgeIteratorType child_edge_begin(NodeRef N) {
    return OutEdges[*Nodes.findLeader(N)].begin();
  }

  EdgeIteratorType child_edge_end(NodeRef N) {
    return OutEdges[*Nodes.findLeader(N)].end();
  }

  decltype(auto) children_edges(NodeRef N) {
    return llvm::make_range(child_edge_begin(N), child_edge_end(N));
  }

  EdgeIteratorType inverse_child_edge_begin(NodeRef N) {
    return InEdges[*Nodes.findLeader(N)].begin();
  }

  EdgeIteratorType inverse_child_edge_end(NodeRef N) {
    return InEdges[*Nodes.findLeader(N)].end();
  }

  decltype(auto) inverse_children_edges(NodeRef N) {
    return llvm::make_range(inverse_child_edge_begin(N),
                            inverse_child_edge_end(N));
  }
};

//

} // namespace itr

namespace itr {

// generic base for easing the task of creating graph traits for graphs

// TODO add const variant of traits helper

template <typename GraphT> struct LLVMCondensationGraphTraitsHelperBase {
  using NodeRef = typename GraphT::NodeRef;
  using NodeType =
      typename std::conditional_t<std::is_pointer<NodeRef>::value,
                                  std::remove_pointer_t<NodeRef>,
                                  std::remove_reference_t<NodeRef>>;

  using EdgeRef = typename GraphT::EdgeRef;

  static_assert(std::is_class<NodeType>::value,
                "NodeType is not a class type!");

  static NodeRef getEntryNode(GraphT *G) { return G->getEntryNode(); }
  static unsigned size(GraphT *G) { return G->size(); }

  using ChildIteratorType = typename NodeType::nodes_iterator;
  static decltype(auto) child_begin(NodeRef G) { return G->nodes_begin(); }
  static decltype(auto) child_end(NodeRef G) { return G->nodes_end(); }

  static decltype(auto) children(NodeRef G) { return G->nodes(); }

  using nodes_iterator = typename GraphT::nodes_iterator;
  static decltype(auto) nodes_begin(GraphT *G) { return G->nodes_begin(); }
  static decltype(auto) nodes_end(GraphT *G) { return G->nodes_end(); }

  static decltype(auto) nodes(GraphT *G) { return G->nodes(); }

  // TODO these require the graph nodes to be a separate object
  // using ChildEdgeIteratorType = int;
  // static ChildEdgeIteratorType child_edge_begin(NodeRef G) { return 0; }
  // static ChildEdgeIteratorType child_edge_end(NodeRef G) { return 0; }
};

template <typename GraphT> struct LLVMCondensationInverseGraphTraitsHelperBase {
  using NodeRef = typename GraphT::NodeRef;
  using NodeType =
      typename std::conditional_t<std::is_pointer<NodeRef>::value,
                                  std::remove_pointer_t<NodeRef>,
                                  std::remove_reference_t<NodeRef>>;

  using EdgeRef = typename GraphT::EdgeRef;

  static_assert(std::is_class<NodeType>::value,
                "NodeType is not a class type!");

  static NodeRef getEntryNode(GraphT *G) { return G->getEntryNode(); }
  static unsigned size(GraphT *G) { return G->size(); }

  using ChildIteratorType = typename NodeType::nodes_iterator;
  static decltype(auto) child_begin(NodeRef G) { return G->pred_nodes_begin(); }
  static decltype(auto) child_end(NodeRef G) { return G->pred_nodes_end(); }

  static decltype(auto) children(NodeRef G) { return G->pred_nodes(); }

  using nodes_iterator = typename GraphT::nodes_iterator;
  static decltype(auto) nodes_begin(GraphT *G) { return G->nodes_begin(); }
  static decltype(auto) nodes_end(GraphT *G) { return G->nodes_end(); }

  static decltype(auto) nodes(GraphT *G) { return G->nodes(); }

  // TODO these require the graph nodes to be a separate object
  // using ChildEdgeIteratorType = int;
  // static ChildEdgeIteratorType child_edge_begin(NodeRef G) { return 0; }
  // static ChildEdgeIteratorType child_edge_end(NodeRef G) { return 0; }
};

} // namespace itr

#endif // header
