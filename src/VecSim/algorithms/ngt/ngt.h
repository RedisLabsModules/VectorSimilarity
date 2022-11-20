/*
 *Copyright Redis Ltd. 2021 - present
 *Licensed under your choice of the Redis Source Available License 2.0 (RSALv2) or
 *the Server Side Public License v1 (SSPLv1).
 */

#pragma once

#include "VecSim/utils/visited_nodes_handler.h"
#include "VecSim/spaces/spaces.h"
#include "VecSim/utils/arr_cpp.h"
#include "VecSim/memory/vecsim_malloc.h"
#include "VecSim/utils/vecsim_stl.h"
#include "VecSim/utils/vec_utils.h"
#include "VecSim/utils/vecsim_results_container.h"
#include "VecSim/query_result_struct.h"
#include "VecSim/vec_sim_common.h"
#include "VecSim/vec_sim_index.h"

#include <deque>
#include <memory>
#include <cassert>
#include <climits>
#include <queue>
#include <random>
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <sys/resource.h>
#include <fstream>

using std::pair;

typedef enum { INNER, LEAF } VPTNodeRole;

template <typename DataType, typename DistType>
class NGTIndex;

// should be able to access data by id;
template <typename DataType, typename DistType>
struct VPTNode : public VecsimBaseObject {
public:
    VPTNodeRole role;
    size_t size;
    VPTNode<DataType, DistType> *left, *right, *parent;
    union {
        struct {
            vecsim_stl::vector<idType> *ids;
        };
        struct {
            DistType radius;
            bool own_pivot;
            union {
                void *pivot;
                idType pivot_id;
            };
        };
    };

public:
    VPTNode(vecsim_stl::vector<idType> *ids, size_t size, VPTNode<DataType, DistType> *parent,
            std::shared_ptr<VecSimAllocator> allocator);
    ~VPTNode();

    inline const VPTNode<DataType, DistType> *left_leaf() const {
        const VPTNode<DataType, DistType> *curr = this;
        while (LEAF != curr->role) {
            curr = curr->left;
        }
        return curr;
    }
    inline const VPTNode<DataType, DistType> *right_leaf() const {
        const VPTNode<DataType, DistType> *curr = this;
        while (LEAF != curr->role) {
            curr = curr->right;
        }
        return curr;
    }

    void merge();
    void rebuild(NGTIndex<DataType, DistType> &vpt);
    inline bool unbalanced() const;
};

template <typename DataType, typename DistType>
class CandidatesFromTree {
private:
    const VPTNode<DataType, DistType> *_begin;
    const VPTNode<DataType, DistType> *_end;

public:
    CandidatesFromTree(const VPTNode<DataType, DistType> *root)
        : _begin(root->left_leaf()), _end(root->right_leaf()->right) {}
    ~CandidatesFromTree() = default;

    class CandidatesItr {
    private:
        const VPTNode<DataType, DistType> *curr;
        size_t idx;

    public:
        CandidatesItr(const VPTNode<DataType, DistType> *begin) : curr(begin), idx(0) {}
        ~CandidatesItr() {}

        CandidatesItr &operator++() {
            idx++;
            if (curr->size == idx) {
                curr = curr->right;
                idx = 0;
            }
            return *this;
        }

        CandidatesItr operator++(int) {
            CandidatesItr retval = *this;
            ++(*this);
            return retval;
        }

        idType operator*() const { return (*curr->ids)[idx]; }

        bool operator==(CandidatesItr other) const {
            return (curr == other.curr) && (idx == other.idx);
        }
        bool operator!=(CandidatesItr other) const {
            return (curr != other.curr) || (idx != other.idx);
        }
    };

    CandidatesItr begin() { return CandidatesItr(_begin); }
    CandidatesItr end() { return CandidatesItr(_end); }
};

template <typename DataType, typename DistType>
VPTNode<DataType, DistType>::VPTNode(vecsim_stl::vector<idType> *ids, size_t size,
                                     VPTNode<DataType, DistType> *parent,
                                     std::shared_ptr<VecSimAllocator> allocator)
    : VecsimBaseObject(allocator), role(LEAF), size(size), left(NULL), right(NULL), parent(parent),
      ids(ids) {}

template <typename DataType, typename DistType>
VPTNode<DataType, DistType>::~VPTNode() {
    if (INNER == this->role) {
        if (own_pivot) {
            this->allocator->free_allocation(pivot);
            own_pivot = false;
        }
        delete left;
        delete right;
    } else {
        assert(LEAF == this->role);
        delete ids;
    }
}

template <typename DataType, typename DistType>
void VPTNode<DataType, DistType>::rebuild(NGTIndex<DataType, DistType> &ngt) {
    if (size > ngt.max_per_leaf_) {
        ngt.splitLeaf(this);
        left->rebuild(ngt);
        right->rebuild(ngt);
    }
}

template <typename DataType, typename DistType>
void VPTNode<DataType, DistType>::merge() {
    assert(INNER == this->role);
    if (own_pivot) {
        allocator->free_allocation(pivot);
        own_pivot = false;
    }

    ids = new (allocator) vecsim_stl::vector<idType>(allocator);
    ids->reserve(size);
    CandidatesFromTree<DataType, DistType> ids_itr(this);
    for (auto id : ids_itr) {
        ids->push_back(id);
    }

    auto left_leaf_left = left_leaf()->left;
    left_leaf_left->right = this;
    delete left;
    left = left_leaf_left;

    auto right_leaf_right = right_leaf()->right;
    right_leaf_right->left = this;
    delete right;
    right = right_leaf_right;

    this->role = LEAF;
}

template <typename DataType, typename DistType>
inline bool VPTNode<DataType, DistType>::unbalanced() const {
    assert(INNER == this->role);
    double ratio = (double)left->size / right->size;
    return (ratio < 0.5) || (2 < ratio);
}

// This type is strongly bounded to `idType` because of the way we get the link list:
//
// linklistsizeint *neighbours_list = get_linklist(element_internal_id);
// unsigned short neighbours_count = getListCount(neighbours_list);
// auto *neighbours = (idType *)(neighbours_list + 1);
//
// TODO: reduce the type to smaller type when possible
typedef idType linklistsizeint;

template <typename DistType>
using candidatesMaxHeap = vecsim_stl::max_priority_queue<DistType, idType>;
template <typename DistType>
using candidatesLabelsMaxHeap = vecsim_stl::abstract_priority_queue<DistType, labelType>;

template <typename DataType, typename DistType>
class NGTIndex : public VecSimIndexAbstract<DistType> {
protected:
    // Index build parameters
    size_t max_elements_;
    size_t M_;
    size_t max_per_leaf_;
    size_t ef_construction_;

    // Index search parameter
    size_t ef_;
    double epsilon_;

    // Index meta-data (based on the data dimensionality and index parameters)
    size_t data_size_;
    size_t size_data_per_element_;
    size_t size_links_per_element_;
    size_t size_links_level0_;
    size_t label_offset_;
    size_t offsetData_, offsetLevel0_;
    size_t incoming_links_offset0;
    size_t incoming_links_offset;
    double mult_;

    // Index level generator of the top level for a new element
    std::default_random_engine random_index_generator_;

    // Index state
    size_t cur_element_count;
    // TODO: after introducing the memory reclaim upon delete, max_id is redundant since the valid
    // internal ids are being kept as a continuous sequence [0, 1, ..,, cur_element_count-1].
    // We can remove this field completely if we change the serialization version, as the decoding
    // relies on this field.
    idType max_id;

    // Index data structures
    idType entrypoint_node_;
    char *data_level0_memory_;
    VPTNode<DataType, DistType> VPtree_root;
    VPTNode<DataType, DistType> VPtree_dummy;
    vecsim_stl::unordered_map<idType, VPTNode<DataType, DistType> *> idToLeaf;
    std::shared_ptr<VisitedNodesHandler> visited_nodes_handler;

    // used for synchronization only when parallel indexing / searching is enabled.
#ifdef ENABLE_PARALLELIZATION
    std::unique_ptr<VisitedNodesHandlerPool> visited_nodes_handler_pool;
    size_t pool_initial_size;
    std::mutex global;
    std::mutex cur_element_count_guard_;
    std::vector<std::mutex> link_list_locks_;
#endif

#ifdef BUILD_TESTS
//     friend class HNSWIndexSerializer;
#include "VecSim/algorithms/ngt/ngt_base_tests_friends.h"
#endif

    // TODO: remove when rebuildVPTree is iterative
    friend struct VPTNode<DataType, DistType>;

    NGTIndex() = delete;                 // default constructor is disabled.
    NGTIndex(const NGTIndex &) = delete; // default (shallow) copy constructor is disabled.
    inline void setExternalLabel(idType internal_id, labelType label);
    inline labelType *getExternalLabelPtr(idType internal_id) const;
    inline size_t getRandomIndex(size_t size);
    inline vecsim_stl::vector<idType> *getIncomingEdgesPtr(idType internal_id) const;
    inline void setIncomingEdgesPtr(idType internal_id, void *edges_ptr);
    inline linklistsizeint *get_linklist(idType internal_id) const;
    inline void setListCount(linklistsizeint *ptr, unsigned short int size);
    inline void removeExtraLinks(linklistsizeint *node_ll, candidatesMaxHeap<DistType> candidates,
                                 idType *node_neighbors, const vecsim_stl::vector<bool> &bitmap,
                                 idType *removed_links, size_t *removed_links_num);
    template <typename Identifier> // Either idType or labelType
    inline DistType
    processCandidate(idType curNodeId, const void *data_point, size_t ef, tag_t visited_tag,
                     vecsim_stl::abstract_priority_queue<DistType, Identifier> &top_candidates,
                     candidatesMaxHeap<DistType> &candidates_set, DistType lowerBound) const;
    inline void processCandidate_RangeSearch(
        idType curNodeId, const void *data_point, double epsilon, tag_t visited_tag,
        std::unique_ptr<vecsim_stl::abstract_results_container> &top_candidates,
        candidatesMaxHeap<DistType> &candidate_set, DistType lowerBound, DistType radius) const;
    CandidatesFromTree<DataType, DistType> searchTree(const VPTNode<DataType, DistType> *root,
                                                      const void *query, size_t batchSize) const;
    CandidatesFromTree<DataType, DistType> searchRangeTree(const void *query,
                                                           DistType radius) const;
    CandidatesFromTree<DataType, DistType> insertToTree(idType id, const void *data);
    void removeFromTree(idType id);
    inline void splitLeaf(VPTNode<DataType, DistType> *node);
    inline void rebuildVPTree(VPTNode<DataType, DistType> *root);
    candidatesMaxHeap<DistType>
    searchGraph(CandidatesFromTree<DataType, DistType> &initial_candidates, const void *data_point,
                size_t ef) const;
    candidatesLabelsMaxHeap<DistType> *
    searchGraph_WithTimeout(CandidatesFromTree<DataType, DistType> &initial_candidates,
                            const void *data_point, size_t ef, size_t k, void *timeoutCtx,
                            VecSimQueryResult_Code *rc) const;
    VecSimQueryResult *
    searchRangeGraph_WithTimeout(CandidatesFromTree<DataType, DistType> &initial_candidates,
                                 const void *data_point, double epsilon, DistType radius,
                                 void *timeoutCtx, VecSimQueryResult_Code *rc) const;
    void getNeighborsByHeuristic2(candidatesMaxHeap<DistType> &top_candidates, size_t M);
    inline idType mutuallyConnectNewElement(idType cur_c,
                                            candidatesMaxHeap<DistType> &top_candidates);
    void repairConnectionsForDeletion(idType element_internal_id, idType neighbour_id,
                                      idType *neighbours_list, idType *neighbour_neighbours_list,
                                      vecsim_stl::vector<bool> &neighbours_bitmap);
    inline void resizeIndex(size_t new_max_elements);
    inline void SwapLastIdWithDeletedId(idType element_internal_id);

    // Protected internal function that implements generic single vector insertion.
    int appendVector(const void *vector_data, labelType label);

    // Protected internal function that implements generic single vector deletion.
    int removeVector(idType id);

    inline void emplaceToHeap(vecsim_stl::abstract_priority_queue<DistType, idType> &heap,
                              DistType dist, idType id) const;
    inline void emplaceToHeap(vecsim_stl::abstract_priority_queue<DistType, labelType> &heap,
                              DistType dist, idType id) const;

public:
    NGTIndex(const NGTParams *params, std::shared_ptr<VecSimAllocator> allocator,
             size_t random_seed = 100, size_t initial_pool_size = 1);
    virtual ~NGTIndex();

    inline void setEf(size_t ef);
    inline size_t getEf() const;
    inline void setEpsilon(double epsilon);
    inline double getEpsilon() const;
    inline size_t indexSize() const override;
    inline size_t getIndexCapacity() const;
    inline size_t getEfConstruction() const;
    inline size_t getM() const;
    inline labelType getExternalLabel(idType internal_id) const;
    inline VisitedNodesHandler *getVisitedList() const;
    VecSimIndexInfo info() const override;
    VecSimInfoIterator *infoIterator() const override;
    bool preferAdHocSearch(size_t subsetSize, size_t k, bool initial_check) override;
    char *getDataByInternalId(idType internal_id) const;
    inline unsigned short int getListCount(const linklistsizeint *ptr) const;

    VecSimQueryResult_List topKQuery(const void *query_data, size_t k,
                                     VecSimQueryParams *queryParams) override;
    VecSimQueryResult_List rangeQuery(const void *query_data, double radius,
                                      VecSimQueryParams *queryParams) override;

    // inline priority queue getter that need to be implemented by derived class
    virtual inline candidatesLabelsMaxHeap<DistType> *getNewMaxPriorityQueue() const = 0;

protected:
    // inline label to id setters that need to be implemented by derived class
    virtual inline std::unique_ptr<vecsim_stl::abstract_results_container>
    getNewResultsContainer(size_t cap) const = 0;
    virtual inline void replaceIdOfLabel(labelType label, idType new_id, idType old_id) = 0;
    virtual inline void setVectorId(labelType label, idType id) = 0;
    virtual inline void resizeLabelLookup(size_t new_max_elements) = 0;
};

/**
 * getters and setters of index data
 */

template <typename DataType, typename DistType>
void NGTIndex<DataType, DistType>::setEf(size_t ef) {
    ef_ = ef;
}

template <typename DataType, typename DistType>
size_t NGTIndex<DataType, DistType>::getEf() const {
    return ef_;
}

template <typename DataType, typename DistType>
void NGTIndex<DataType, DistType>::setEpsilon(double epsilon) {
    epsilon_ = epsilon;
}

template <typename DataType, typename DistType>
double NGTIndex<DataType, DistType>::getEpsilon() const {
    return epsilon_;
}

template <typename DataType, typename DistType>
size_t NGTIndex<DataType, DistType>::indexSize() const {
    return cur_element_count;
}

template <typename DataType, typename DistType>
size_t NGTIndex<DataType, DistType>::getIndexCapacity() const {
    return max_elements_;
}

template <typename DataType, typename DistType>
size_t NGTIndex<DataType, DistType>::getEfConstruction() const {
    return ef_construction_;
}

template <typename DataType, typename DistType>
size_t NGTIndex<DataType, DistType>::getM() const {
    return M_;
}

template <typename DataType, typename DistType>
labelType NGTIndex<DataType, DistType>::getExternalLabel(idType internal_id) const {
    labelType return_label;
    memcpy(&return_label,
           (data_level0_memory_ + internal_id * size_data_per_element_ + label_offset_),
           sizeof(labelType));
    return return_label;
}

template <typename DataType, typename DistType>
void NGTIndex<DataType, DistType>::setExternalLabel(idType internal_id, labelType label) {
    memcpy((data_level0_memory_ + internal_id * size_data_per_element_ + label_offset_), &label,
           sizeof(labelType));
}

template <typename DataType, typename DistType>
labelType *NGTIndex<DataType, DistType>::getExternalLabelPtr(idType internal_id) const {
    return (labelType *)(data_level0_memory_ + internal_id * size_data_per_element_ +
                         label_offset_);
}

template <typename DataType, typename DistType>
char *NGTIndex<DataType, DistType>::getDataByInternalId(idType internal_id) const {
    return (data_level0_memory_ + internal_id * size_data_per_element_ + offsetData_);
}

template <typename DataType, typename DistType>
size_t NGTIndex<DataType, DistType>::getRandomIndex(size_t size) {
    std::uniform_real_distribution<double> distribution(0.0, 1.0);
    double r = distribution(random_index_generator_) * size;
    return (size_t)r;
}

template <typename DataType, typename DistType>
vecsim_stl::vector<idType> *
NGTIndex<DataType, DistType>::getIncomingEdgesPtr(idType internal_id) const {
    return reinterpret_cast<vecsim_stl::vector<idType> *>(
        *(void **)(data_level0_memory_ + internal_id * size_data_per_element_ +
                   incoming_links_offset0));
}

template <typename DataType, typename DistType>
void NGTIndex<DataType, DistType>::setIncomingEdgesPtr(idType internal_id, void *edges_ptr) {
    memcpy(data_level0_memory_ + internal_id * size_data_per_element_ + incoming_links_offset0,
           &edges_ptr, sizeof(void *));
}

template <typename DataType, typename DistType>
linklistsizeint *NGTIndex<DataType, DistType>::get_linklist(idType internal_id) const {
    return (linklistsizeint *)(data_level0_memory_ + internal_id * size_data_per_element_ +
                               offsetLevel0_);
}

template <typename DataType, typename DistType>
unsigned short int NGTIndex<DataType, DistType>::getListCount(const linklistsizeint *ptr) const {
    return *((unsigned short int *)ptr);
}

template <typename DataType, typename DistType>
void NGTIndex<DataType, DistType>::setListCount(linklistsizeint *ptr, unsigned short int size) {
    *((unsigned short int *)(ptr)) = *((unsigned short int *)&size);
}

template <typename DataType, typename DistType>
VisitedNodesHandler *NGTIndex<DataType, DistType>::getVisitedList() const {
    return visited_nodes_handler.get();
}

/**
 * helper functions
 */

template <typename DataType, typename DistType>
void NGTIndex<DataType, DistType>::splitLeaf(VPTNode<DataType, DistType> *node) {
    assert(LEAF == node->role);
    // Holds to the ids before changing role.
    auto node_ids = node->ids;
    node->role = INNER;

    size_t i = getRandomIndex(node->size);
    node->own_pivot = false;
    node->pivot_id = (*node_ids)[i];
    (*node_ids)[i] = (*node_ids)[node->size - 1];
    node_ids->pop_back();

    std::vector<pair<DistType, idType>> dist_cache;
    dist_cache.reserve(node->size);
    dist_cache.emplace_back(0, node->pivot_id);
    void *pivot_data = getDataByInternalId(node->pivot_id);
    for (auto id : *node_ids) {
        dist_cache.emplace_back(this->dist_func(pivot_data, getDataByInternalId(id), this->dim),
                                id);
    }
    auto med = dist_cache.begin() + (node->size / 2);
    std::nth_element(dist_cache.begin() + 1, med, dist_cache.end());

    node->radius = med->first;

    auto left_ids = new (this->allocator) vecsim_stl::vector<idType>(this->allocator);
    left_ids->reserve(1 + node->size / 2);
    for (auto pair = dist_cache.begin(); pair != med; ++pair) {
        left_ids->push_back(pair->second);
    }

    auto right_ids = new (this->allocator) vecsim_stl::vector<idType>(this->allocator);
    right_ids->reserve(1 + node->size / 2);
    for (auto pair = med; pair != dist_cache.end(); ++pair) {
        right_ids->push_back(pair->second);
    }

    // After passing the ids to the left and the right, we can delete current vector.
    delete node_ids;

    auto old_left = node->left;
    auto old_right = node->right;

    node->left = new (this->allocator)
        VPTNode<DataType, DistType>(left_ids, left_ids->size(), node, this->allocator);
    node->right = new (this->allocator)
        VPTNode<DataType, DistType>(right_ids, right_ids->size(), node, this->allocator);

    node->left->left = old_left;
    node->left->right = node->right;
    node->right->left = node->left;
    node->right->right = old_right;

    old_left->right = node->left;
    old_right->left = node->right;

    for (auto id : *left_ids) {
        idToLeaf[id] = node->left;
    }
    for (auto id : *right_ids) {
        idToLeaf[id] = node->right;
    }
}

template <typename DataType, typename DistType>
void NGTIndex<DataType, DistType>::rebuildVPTree(VPTNode<DataType, DistType> *root) {
    assert(INNER == root->role);

    // TODO: make vector temporary (not allocated)
    // vecsim_stl::vector<idType> tree_ids(this->allocator);
    // tree_ids.reserve(root->size);
    auto tree_ids = new (this->allocator) vecsim_stl::vector<idType>(this->allocator);
    tree_ids->reserve(root->size);

    auto right_next_leaf = root->right_leaf()->right;
    auto left_next_leaf = root->left_leaf()->left;
    for (auto curr = root->left_leaf(); curr != right_next_leaf; curr = curr->right) {
        tree_ids->insert(tree_ids->end(), curr->ids->begin(), curr->ids->end());
    }

    if (root->own_pivot) {
        this->allocator->free_allocation(root->pivot);
        root->own_pivot = false;
    }
    delete root->left;
    delete root->right;

    root->left = left_next_leaf;
    root->right = right_next_leaf;
    left_next_leaf->right = root;
    right_next_leaf->left = root;

    // TODO: make iterative
    root->ids = tree_ids;
    root->role = LEAF;
    for (auto i : *tree_ids) {
        idToLeaf[i] = root;
    }
    root->rebuild(*this);
}

template <typename DataType, typename DistType>
CandidatesFromTree<DataType, DistType>
NGTIndex<DataType, DistType>::insertToTree(idType id, const void *data) {
    VPTNode<DataType, DistType> *curr = &this->VPtree_root;
    VPTNode<DataType, DistType> *ret = curr;

    while (curr->role != LEAF) {
        // Increase the size of the current node, as we are going to insert the vector to one of it
        // leaves
        curr->size++;

        if (curr->unbalanced()) {
            // Arbitrarily insert id to sub-tree
            curr->left_leaf()->ids->push_back(id);
            // Rebuild sub-tree.
            rebuildVPTree(curr);
            return searchTree(curr, data, ef_construction_);
        } else {
            auto pivot = curr->own_pivot ? curr->pivot : getDataByInternalId(curr->pivot_id);
            if (curr->radius < this->dist_func(pivot, data, this->dim)) {
                curr = curr->right;
            } else {
                curr = curr->left;
            }
        }
        if (curr->size <= ef_construction_) {
            ret = curr;
        }
    }

    curr->size++;
    curr->ids->push_back(id);
    idToLeaf[id] = curr;
    if (curr->size > this->max_per_leaf_) {
        splitLeaf(curr);
    }

    // TEST
    // for (idType id = 0; id < cur_element_count; id++) {
    //     auto curleaf = idToLeaf[id];
    //     auto pos = std::find(curleaf->ids->begin(), curleaf->ids->end(), id);
    //     assert(pos != curleaf->ids->end());
    // }
    // for (auto cur = this->VPtree_dummy.right; cur != &this->VPtree_dummy; cur = cur->right) {
    //     assert(cur->role == LEAF);
    //     assert(cur->size <= this->max_per_leaf_);
    //     if (cur->parent)
    //         assert(cur->parent->role == INNER);
    // }
    /////////////
    return CandidatesFromTree<DataType, DistType>(ret);
}

// assumes the id in the tree. this should be checked before calling this function.
template <typename DataType, typename DistType>
void NGTIndex<DataType, DistType>::removeFromTree(idType id) {

    VPTNode<DataType, DistType> *leaf = idToLeaf.at(id);
    idToLeaf.erase(id);
    auto pos = std::find(leaf->ids->begin(), leaf->ids->end(), id);
    assert(pos != leaf->ids->end());
    *pos = leaf->ids->back();
    leaf->ids->pop_back();
    leaf->size--;

    VPTNode<DataType, DistType> *top_unbalanced = nullptr;
    auto curr = leaf;
    while (curr != &this->VPtree_root) {
        curr = curr->parent;
        curr->size--;
        if (!curr->own_pivot && curr->pivot_id == id) {
            curr->pivot = this->allocator->allocate(this->data_size_);
            memcpy(curr->pivot, getDataByInternalId(id), this->data_size_);
            curr->own_pivot = true;
        }
        if (curr->unbalanced()) {
            top_unbalanced = curr;
        }
    }

    if (top_unbalanced) {
        rebuildVPTree(top_unbalanced);
    } else if (leaf != &this->VPtree_root && leaf->parent->size <= this->max_per_leaf_) {
        CandidatesFromTree<DataType, DistType> ids_itr(leaf->parent);
        for (auto i : ids_itr) {
            idToLeaf[i] = leaf->parent;
        }
        leaf->parent->merge();
    }
    // TEST
    // for (idType i = 0; i < cur_element_count; i++) {
    //     if (i == id)
    //         continue;
    //     auto curleaf = idToLeaf[i];
    //     auto pos = std::find(curleaf->ids->begin(), curleaf->ids->end(), i);
    //     assert(pos != curleaf->ids->end());
    // }
    // for (auto cur = this->VPtree_dummy.right; cur != &this->VPtree_dummy; cur = cur->right) {
    //     assert(cur->role == LEAF);
    //     assert(cur->size <= this->max_per_leaf_);
    //     if (cur->parent)
    //         assert(cur->parent->role == INNER);
    // }
    /////////////
}

template <typename DataType, typename DistType>
CandidatesFromTree<DataType, DistType>
NGTIndex<DataType, DistType>::searchTree(const VPTNode<DataType, DistType> *root, const void *query,
                                         size_t batchSize) const {
    const VPTNode<DataType, DistType> *curr = root;
    while (curr->role != LEAF && curr->size > batchSize) {
        auto pivot = curr->own_pivot ? curr->pivot : getDataByInternalId(curr->pivot_id);
        if (curr->radius < this->dist_func(pivot, query, this->dim)) {
            curr = curr->right;
        } else {
            curr = curr->left;
        }
    }
    return CandidatesFromTree<DataType, DistType>(curr);
}

template <typename DataType, typename DistType>
CandidatesFromTree<DataType, DistType>
NGTIndex<DataType, DistType>::searchRangeTree(const void *query, DistType radius) const {
    const VPTNode<DataType, DistType> *curr = &this->VPtree_root;
    DistType d = std::numeric_limits<DistType>::max();
    while (curr->role != LEAF) {
        auto pivot = curr->own_pivot ? curr->pivot : getDataByInternalId(curr->pivot_id);
        d = this->dist_func(pivot, query, this->dim);
        if (curr->radius < d) {
            curr = curr->right;
        } else {
            curr = curr->left;
        }
    }
    return CandidatesFromTree<DataType, DistType>(curr);
}

template <typename DataType, typename DistType>
void NGTIndex<DataType, DistType>::removeExtraLinks(
    linklistsizeint *node_ll, candidatesMaxHeap<DistType> candidates, idType *node_neighbors,
    const vecsim_stl::vector<bool> &neighbors_bitmap, idType *removed_links,
    size_t *removed_links_num) {

    auto orig_candidates = candidates;
    // candidates will store the newly selected neighbours (for the relevant node).
    getNeighborsByHeuristic2(candidates, M_);

    // check the diff in the link list, save the neighbours
    // that were chosen to be removed, and update the new neighbours
    size_t removed_idx = 0;
    size_t link_idx = 0;

    while (orig_candidates.size() > 0) {
        if (orig_candidates.top().second != candidates.top().second) {
            if (neighbors_bitmap[orig_candidates.top().second]) {
                removed_links[removed_idx++] = orig_candidates.top().second;
            }
            orig_candidates.pop();
        } else {
            node_neighbors[link_idx++] = candidates.top().second;
            candidates.pop();
            orig_candidates.pop();
        }
    }
    setListCount(node_ll, link_idx);
    *removed_links_num = removed_idx;
}

template <typename DataType, typename DistType>
void NGTIndex<DataType, DistType>::emplaceToHeap(
    vecsim_stl::abstract_priority_queue<DistType, idType> &heap, DistType dist, idType id) const {
    heap.emplace(dist, id);
}

template <typename DataType, typename DistType>
void NGTIndex<DataType, DistType>::emplaceToHeap(
    vecsim_stl::abstract_priority_queue<DistType, labelType> &heap, DistType dist,
    idType id) const {
    heap.emplace(dist, getExternalLabel(id));
}

// This function handles both label heaps and internal ids heaps. It uses the `emplaceToHeap`
// overloading to emplace correctly for both cases.
template <typename DataType, typename DistType>
template <typename Identifier>
DistType NGTIndex<DataType, DistType>::processCandidate(
    idType curNodeId, const void *data_point, size_t ef, tag_t visited_tag,
    vecsim_stl::abstract_priority_queue<DistType, Identifier> &top_candidates,
    candidatesMaxHeap<DistType> &candidate_set, DistType lowerBound) const {

#ifdef ENABLE_PARALLELIZATION
    std::unique_lock<std::mutex> lock(link_list_locks_[curNodeId]);
#endif
    linklistsizeint *node_ll = get_linklist(curNodeId);
    size_t links_num = getListCount(node_ll);
    auto *node_links = (idType *)(node_ll + 1);

    __builtin_prefetch(visited_nodes_handler->getElementsTags() + *node_links);
    __builtin_prefetch(getDataByInternalId(*node_links));

    for (size_t j = 0; j < links_num; j++) {
        idType *candidate_pos = node_links + j;
        idType candidate_id = *candidate_pos;

        // Pre-fetch the next candidate data into memory cache, to improve performance.
        idType *next_candidate_pos = node_links + j + 1;
        __builtin_prefetch(visited_nodes_handler->getElementsTags() + *next_candidate_pos);
        __builtin_prefetch(getDataByInternalId(*next_candidate_pos));

        if (this->visited_nodes_handler->getNodeTag(candidate_id) == visited_tag)
            continue;

        this->visited_nodes_handler->tagNode(candidate_id, visited_tag);
        char *currObj1 = (getDataByInternalId(candidate_id));

        DistType dist1 = this->dist_func(data_point, currObj1, this->dim);
        if (lowerBound > dist1 || top_candidates.size() < ef) {
            candidate_set.emplace(-dist1, candidate_id);

            emplaceToHeap(top_candidates, dist1, candidate_id);

            if (top_candidates.size() > ef)
                top_candidates.pop();

            lowerBound = top_candidates.top().first;
        }
    }
    // Pre-fetch the neighbours list of the top candidate (the one that is going
    // to be processed in the next iteration) into memory cache, to improve performance.
    __builtin_prefetch(get_linklist(candidate_set.top().second));

    return lowerBound;
}

template <typename DataType, typename DistType>
void NGTIndex<DataType, DistType>::processCandidate_RangeSearch(
    idType curNodeId, const void *query_data, double epsilon, tag_t visited_tag,
    std::unique_ptr<vecsim_stl::abstract_results_container> &results,
    candidatesMaxHeap<DistType> &candidate_set, DistType dyn_range, DistType radius) const {

#ifdef ENABLE_PARALLELIZATION
    std::unique_lock<std::mutex> lock(link_list_locks_[curNodeId]);
#endif
    linklistsizeint *node_ll = get_linklist(curNodeId);
    size_t links_num = getListCount(node_ll);
    auto *node_links = (idType *)(node_ll + 1);

    __builtin_prefetch(visited_nodes_handler->getElementsTags() + *(node_ll + 1));
    __builtin_prefetch(getDataByInternalId(*node_links));

    for (size_t j = 0; j < links_num; j++) {
        idType *candidate_pos = node_links + j;
        idType candidate_id = *candidate_pos;

        // Pre-fetch the next candidate data into memory cache, to improve performance.
        idType *next_candidate_pos = node_links + j + 1;
        __builtin_prefetch(visited_nodes_handler->getElementsTags() + *next_candidate_pos);
        __builtin_prefetch(getDataByInternalId(*next_candidate_pos));

        if (this->visited_nodes_handler->getNodeTag(candidate_id) == visited_tag)
            continue;
        this->visited_nodes_handler->tagNode(candidate_id, visited_tag);
        char *candidate_data = getDataByInternalId(candidate_id);

        DistType candidate_dist = this->dist_func(query_data, candidate_data, this->dim);
        if (candidate_dist < dyn_range) {
            candidate_set.emplace(-candidate_dist, candidate_id);

            // If the new candidate is in the requested radius, add it to the results set.
            if (candidate_dist <= radius) {
                results->emplace(getExternalLabel(candidate_id), candidate_dist);
            }
        }
    }
    // Pre-fetch the neighbours list of the top candidate (the one that is going
    // to be processed in the next iteration) into memory cache, to improve performance.
    __builtin_prefetch(get_linklist(candidate_set.top().second));
}

template <typename DataType, typename DistType>
candidatesMaxHeap<DistType> NGTIndex<DataType, DistType>::searchGraph(
    CandidatesFromTree<DataType, DistType> &initial_candidates, const void *data_point,
    size_t ef) const {

#ifdef ENABLE_PARALLELIZATION
    this->visited_nodes_handler =
        this->visited_nodes_handler_pool->getAvailableVisitedNodesHandler();
#endif

    tag_t visited_tag = this->visited_nodes_handler->getFreshTag();

    candidatesMaxHeap<DistType> top_candidates(this->allocator);
    candidatesMaxHeap<DistType> candidate_set(this->allocator);

    for (auto id : initial_candidates) {
        if (id != max_id) { // skip oun id
            DistType dist = this->dist_func(data_point, getDataByInternalId(id), this->dim);
            top_candidates.emplace(dist, id);
            candidate_set.emplace(-dist, id);
            this->visited_nodes_handler->tagNode(id, visited_tag);
        }
    }
    while (top_candidates.size() > ef) {
        top_candidates.pop();
    }

    DistType lowerBound = top_candidates.top().first;

    while (!candidate_set.empty()) {
        pair<DistType, idType> curr_el_pair = candidate_set.top();
        if ((-curr_el_pair.first) > lowerBound && top_candidates.size() >= ef) {
            break;
        }
        candidate_set.pop();

        lowerBound = processCandidate(curr_el_pair.second, data_point, ef, visited_tag,
                                      top_candidates, candidate_set, lowerBound);
    }

#ifdef ENABLE_PARALLELIZATION
    visited_nodes_handler_pool->returnVisitedNodesHandlerToPool(this->visited_nodes_handler);
#endif
    return top_candidates;
}

template <typename DataType, typename DistType>
void NGTIndex<DataType, DistType>::getNeighborsByHeuristic2(
    candidatesMaxHeap<DistType> &top_candidates, const size_t M) {
    if (top_candidates.size() < M) {
        return;
    }

    candidatesMaxHeap<DistType> queue_closest(this->allocator);
    vecsim_stl::vector<pair<DistType, idType>> return_list(this->allocator);
    while (top_candidates.size() > 0) {
        // the distance is saved negatively to have the queue ordered such that first is closer
        // (higher).
        queue_closest.emplace(-top_candidates.top().first, top_candidates.top().second);
        top_candidates.pop();
    }

    while (queue_closest.size()) {
        if (return_list.size() >= M)
            break;
        pair<DistType, idType> current_pair = queue_closest.top();
        DistType candidate_to_query_dist = -current_pair.first;
        queue_closest.pop();
        bool good = true;

        // a candidate is "good" to become a neighbour, unless we find
        // another item that was already selected to the neighbours set which is closer
        // to both q and the candidate than the distance between the candidate and q.
        for (pair<DistType, idType> second_pair : return_list) {
            DistType candidate_to_selected_dist =
                this->dist_func(getDataByInternalId(second_pair.second),
                                getDataByInternalId(current_pair.second), this->dim);
            if (candidate_to_selected_dist < candidate_to_query_dist) {
                good = false;
                break;
            }
        }
        if (good) {
            return_list.push_back(current_pair);
        }
    }

    for (pair<DistType, idType> current_pair : return_list) {
        top_candidates.emplace(-current_pair.first, current_pair.second);
    }
}

template <typename DataType, typename DistType>
idType NGTIndex<DataType, DistType>::mutuallyConnectNewElement(
    idType cur_c, candidatesMaxHeap<DistType> &top_candidates) {
    getNeighborsByHeuristic2(top_candidates, M_);
    if (top_candidates.size() > M_)
        throw std::runtime_error(
            "Should be not be more than M_ candidates returned by the heuristic");

    vecsim_stl::vector<idType> selectedNeighbors(this->allocator);
    selectedNeighbors.reserve(M_);
    while (top_candidates.size() > 0) {
        selectedNeighbors.push_back(top_candidates.top().second);
        top_candidates.pop();
    }

    idType next_closest_entry_point = selectedNeighbors.back();
    {
        linklistsizeint *ll_cur = get_linklist(cur_c);
        if (*ll_cur) {
            throw std::runtime_error("The newly inserted element should have blank link list");
        }
        setListCount(ll_cur, selectedNeighbors.size());
        auto *data = (idType *)(ll_cur + 1);
        for (size_t idx = 0; idx < selectedNeighbors.size(); idx++) {
            if (data[idx])
                throw std::runtime_error("Possible memory corruption");
            data[idx] = selectedNeighbors[idx];
        }
        auto *incoming_edges = new (this->allocator) vecsim_stl::vector<idType>(this->allocator);
        setIncomingEdgesPtr(cur_c, (void *)incoming_edges);
    }

    // go over the selected neighbours - selectedNeighbor is the neighbour id
    vecsim_stl::vector<bool> neighbors_bitmap(this->allocator);
    for (idType selectedNeighbor : selectedNeighbors) {
#ifdef ENABLE_PARALLELIZATION
        std::unique_lock<std::mutex> lock(link_list_locks_[selectedNeighbor]);
#endif
        linklistsizeint *ll_other = get_linklist(selectedNeighbor);
        size_t sz_link_list_other = getListCount(ll_other);

        if (sz_link_list_other > M_)
            throw std::runtime_error("Bad value of sz_link_list_other");
        if (selectedNeighbor == cur_c)
            throw std::runtime_error("Trying to connect an element to itself");

        // get the array of neighbours - for the current neighbour
        auto *neighbor_neighbors = (idType *)(ll_other + 1);

        // If the selected neighbor can add another link (hasn't reached the max) - add it.
        if (sz_link_list_other < M_) {
            neighbor_neighbors[sz_link_list_other] = cur_c;
            setListCount(ll_other, sz_link_list_other + 1);
        } else {
            // try finding "weak" elements to replace it with the new one with the heuristic:
            candidatesMaxHeap<DistType> candidates(this->allocator);
            // (re)use the bitmap to represent the set of the original neighbours for the current
            // selected neighbour.
            neighbors_bitmap.assign(max_id + 1, false);
            DistType d_max = this->dist_func(getDataByInternalId(cur_c),
                                             getDataByInternalId(selectedNeighbor), this->dim);
            candidates.emplace(d_max, cur_c);
            // consider cur_c as if it was a link of the selected neighbor
            neighbors_bitmap[cur_c] = true;
            for (size_t j = 0; j < sz_link_list_other; j++) {
                candidates.emplace(this->dist_func(getDataByInternalId(neighbor_neighbors[j]),
                                                   getDataByInternalId(selectedNeighbor),
                                                   this->dim),
                                   neighbor_neighbors[j]);
                neighbors_bitmap[neighbor_neighbors[j]] = true;
            }

            idType removed_links[sz_link_list_other + 1];
            size_t removed_links_num;
            removeExtraLinks(ll_other, candidates, neighbor_neighbors, neighbors_bitmap,
                             removed_links, &removed_links_num);

            // remove the current neighbor from the incoming list of nodes for the
            // neighbours that were chosen to remove (if edge wasn't bidirectional)
            auto *neighbour_incoming_edges = getIncomingEdgesPtr(selectedNeighbor);
            for (size_t i = 0; i < removed_links_num; i++) {
                idType node_id = removed_links[i];
                auto *node_incoming_edges = getIncomingEdgesPtr(node_id);
                // if we removed cur_c (the node just inserted), then it points to the current
                // neighbour, but not vise versa.
                if (node_id == cur_c) {
                    neighbour_incoming_edges->push_back(cur_c);
                    continue;
                }

                // if the node id (the neighbour's neighbour to be removed)
                // wasn't pointing to the neighbour (i.e., the edge was uni-directional),
                // we should remove the current neighbor from the node's incoming edges.
                // otherwise, the edge turned from bidirectional to
                // uni-directional, so we insert it to the neighbour's
                // incoming edges set.
                auto it = std::find(node_incoming_edges->begin(), node_incoming_edges->end(),
                                    selectedNeighbor);
                if (it != node_incoming_edges->end()) {
                    node_incoming_edges->erase(it);
                } else {
                    neighbour_incoming_edges->push_back(node_id);
                }
            }
        }
    }
    return next_closest_entry_point;
}

template <typename DataType, typename DistType>
void NGTIndex<DataType, DistType>::repairConnectionsForDeletion(
    idType element_internal_id, idType neighbour_id, idType *neighbours_list,
    idType *neighbour_neighbours_list, vecsim_stl::vector<bool> &neighbours_bitmap) {

    // put the deleted element's neighbours in the candidates.
    candidatesMaxHeap<DistType> candidates(this->allocator);
    unsigned short neighbours_count = getListCount(neighbours_list);
    auto *neighbours = (idType *)(neighbours_list + 1);
    for (size_t j = 0; j < neighbours_count; j++) {
        // Don't put the neighbor itself in his own candidates
        if (neighbours[j] == neighbour_id) {
            continue;
        }
        candidates.emplace(this->dist_func(getDataByInternalId(neighbours[j]),
                                           getDataByInternalId(neighbour_id), this->dim),
                           neighbours[j]);
    }

    // add the deleted element's neighbour's original neighbors in the candidates.
    vecsim_stl::vector<bool> neighbour_orig_neighbours_set(max_id + 1, false, this->allocator);
    unsigned short neighbour_neighbours_count = getListCount(neighbour_neighbours_list);
    auto *neighbour_neighbours = (idType *)(neighbour_neighbours_list + 1);
    for (size_t j = 0; j < neighbour_neighbours_count; j++) {
        neighbour_orig_neighbours_set[neighbour_neighbours[j]] = true;
        // Don't add the removed element to the candidates, nor nodes that are already in the
        // candidates set.
        if (neighbours_bitmap[neighbour_neighbours[j]] ||
            neighbour_neighbours[j] == element_internal_id) {
            continue;
        }
        candidates.emplace(this->dist_func(getDataByInternalId(neighbour_id),
                                           getDataByInternalId(neighbour_neighbours[j]), this->dim),
                           neighbour_neighbours[j]);
    }

    size_t removed_links_num;
    idType removed_links[neighbour_neighbours_count];
    removeExtraLinks(neighbour_neighbours_list, candidates, neighbour_neighbours,
                     neighbour_orig_neighbours_set, removed_links, &removed_links_num);

    // remove neighbour id from the incoming list of nodes for his
    // neighbours that were chosen to remove
    auto *neighbour_incoming_edges = getIncomingEdgesPtr(neighbour_id);

    for (size_t i = 0; i < removed_links_num; i++) {
        idType node_id = removed_links[i];
        auto *node_incoming_edges = getIncomingEdgesPtr(node_id);

        // if the node id (the neighbour's neighbour to be removed)
        // wasn't pointing to the neighbour (edge was one directional),
        // we should remove it from the node's incoming edges.
        // otherwise, edge turned from bidirectional to one directional,
        // and it should be saved in the neighbor's incoming edges.
        auto it = std::find(node_incoming_edges->begin(), node_incoming_edges->end(), neighbour_id);
        if (it != node_incoming_edges->end()) {
            node_incoming_edges->erase(it);
        } else {
            neighbour_incoming_edges->push_back(node_id);
        }
    }

    // updates for the new edges created
    unsigned short updated_links_num = getListCount(neighbour_neighbours_list);
    for (size_t i = 0; i < updated_links_num; i++) {
        idType node_id = neighbour_neighbours[i];
        if (!neighbour_orig_neighbours_set[node_id]) {
            auto *node_incoming_edges = getIncomingEdgesPtr(node_id);
            // if the node has an edge to the neighbour as well, remove it
            // from the incoming nodes of the neighbour
            // otherwise, need to update the edge as incoming.
            linklistsizeint *node_links_list = get_linklist(node_id);
            unsigned short node_links_size = getListCount(node_links_list);
            auto *node_links = (idType *)(node_links_list + 1);
            bool bidirectional_edge = false;
            for (size_t j = 0; j < node_links_size; j++) {
                if (node_links[j] == neighbour_id) {
                    neighbour_incoming_edges->erase(std::find(neighbour_incoming_edges->begin(),
                                                              neighbour_incoming_edges->end(),
                                                              node_id));
                    bidirectional_edge = true;
                    break;
                }
            }
            if (!bidirectional_edge) {
                node_incoming_edges->push_back(neighbour_id);
            }
        }
    }
}

template <typename DataType, typename DistType>
void NGTIndex<DataType, DistType>::SwapLastIdWithDeletedId(idType element_internal_id) {
    // swap label
    replaceIdOfLabel(getExternalLabel(max_id), element_internal_id, max_id);

    // Update Tree data
    auto leaf = idToLeaf[max_id];
    auto pos = std::find(leaf->ids->begin(), leaf->ids->end(), max_id);
    *pos = element_internal_id;
    idToLeaf[element_internal_id] = leaf;
    idToLeaf.erase(max_id);

    auto curr = leaf;
    while (curr != &this->VPtree_root) {
        curr = curr->parent;
        if (!curr->own_pivot && curr->pivot_id == max_id) {
            curr->pivot_id = element_internal_id;
        }
    }

    // swap neighbours

    linklistsizeint *neighbours_list = get_linklist(max_id);
    unsigned short neighbours_count = getListCount(neighbours_list);
    auto *neighbours = (idType *)(neighbours_list + 1);

    // go over the neighbours that also points back to the last element whose is going to
    // change, and update the id.
    for (size_t i = 0; i < neighbours_count; i++) {
        idType neighbour_id = neighbours[i];
        linklistsizeint *neighbour_neighbours_list = get_linklist(neighbour_id);
        unsigned short neighbour_neighbours_count = getListCount(neighbour_neighbours_list);

        auto *neighbour_neighbours = (idType *)(neighbour_neighbours_list + 1);
        bool bidirectional_edge = false;
        for (size_t j = 0; j < neighbour_neighbours_count; j++) {
            // if the edge is bidirectional, update for this neighbor
            if (neighbour_neighbours[j] == max_id) {
                bidirectional_edge = true;
                neighbour_neighbours[j] = element_internal_id;
                break;
            }
        }

        // if this edge is uni-directional, we should update the id in the neighbor's
        // incoming edges.
        if (!bidirectional_edge) {
            auto *neighbour_incoming_edges = getIncomingEdgesPtr(neighbour_id);
            auto it = std::find(neighbour_incoming_edges->begin(), neighbour_incoming_edges->end(),
                                max_id);
            assert(it != neighbour_incoming_edges->end());
            neighbour_incoming_edges->erase(it);
            neighbour_incoming_edges->push_back(element_internal_id);
        }
    }

    // next, go over the rest of incoming edges (the ones that are not bidirectional) and make
    // updates.
    auto *incoming_edges = getIncomingEdgesPtr(max_id);
    for (auto incoming_edge : *incoming_edges) {
        linklistsizeint *incoming_neighbour_neighbours_list = get_linklist(incoming_edge);
        unsigned short incoming_neighbour_neighbours_count =
            getListCount(incoming_neighbour_neighbours_list);
        auto *incoming_neighbour_neighbours = (idType *)(incoming_neighbour_neighbours_list + 1);
        for (size_t j = 0; j < incoming_neighbour_neighbours_count; j++) {
            if (incoming_neighbour_neighbours[j] == max_id) {
                incoming_neighbour_neighbours[j] = element_internal_id;
                break;
            }
        }
    }

    // swap the last_id graph data, and invalidate the deleted id's data
    memcpy(data_level0_memory_ + element_internal_id * size_data_per_element_ + offsetLevel0_,
           data_level0_memory_ + max_id * size_data_per_element_ + offsetLevel0_,
           size_data_per_element_);
    memset(data_level0_memory_ + max_id * size_data_per_element_ + offsetLevel0_, 0,
           size_data_per_element_);

    if (max_id == this->entrypoint_node_) {
        this->entrypoint_node_ = element_internal_id;
    }
}

/* typedef struct {
    VecSimType type;     // Datatype to index.
    size_t dim;          // Vector's dimension.
    VecSimMetric metric; // Distance metric to use in the index.
    size_t initialCapacity;
    size_t blockSize;
    size_t M;
    size_t maxPerLeaf;
    size_t efConstruction;
    size_t efRuntime;
    double epsilon;
} HNSWParams; */
template <typename DataType, typename DistType>
NGTIndex<DataType, DistType>::NGTIndex(const NGTParams *params,
                                       std::shared_ptr<VecSimAllocator> allocator,
                                       size_t random_seed, size_t pool_initial_size)
    : VecSimIndexAbstract<DistType>(allocator, params->dim, params->type, params->metric,
                                    params->blockSize, params->multi),
      max_elements_(params->initialCapacity),
      data_size_(VecSimType_sizeof(params->type) * this->dim),
      VPtree_root(new (allocator) vecsim_stl::vector<idType>(allocator), 0, nullptr, allocator),
      VPtree_dummy(nullptr, 0, nullptr, allocator), idToLeaf(params->initialCapacity, allocator)

#ifdef ENABLE_PARALLELIZATION
      ,
      link_list_locks_(max_elements_)
#endif
{
    size_t M = params->M ? params->M : DEFAULT_M;
    M_ = M;

    size_t mpl = params->maxPerLeaf ? params->maxPerLeaf : DEFAULT_MAX_PER_LEAF;
    max_per_leaf_ = mpl;

    size_t ef_construction = params->efConstruction ? params->efConstruction : DEFAULT_EF_C;
    ef_construction_ = std::max(ef_construction, M_);
    ef_ = params->efRuntime ? params->efRuntime : DEFAULT_EF_RT;
    epsilon_ = params->epsilon > 0.0 ? params->epsilon : DEFAULT_EPSILON;

    cur_element_count = 0;
    max_id = INVALID_ID;
#ifdef ENABLE_PARALLELIZATION
    pool_initial_size = pool_initial_size;
    visited_nodes_handler_pool = std::unique_ptr<VisitedNodesHandlerPool>(
        new (this->allocator)
            VisitedNodesHandlerPool(pool_initial_size, max_elements_, this->allocator));
#else
    visited_nodes_handler = std::shared_ptr<VisitedNodesHandler>(
        new (this->allocator) VisitedNodesHandler(max_elements_, this->allocator));
#endif

    // initializations for special treatment of the first node
    entrypoint_node_ = INVALID_ID;

    if (M <= 1)
        throw std::runtime_error("NGT index parameter M cannot be 1");
    mult_ = 1 / log(1.0 * M_);
    random_index_generator_.seed(random_seed);

    // data_level0_memory will look like this:
    // | -----4------ | -----4*M0----------- | ----------8----------| --data_size_-- | ----8---- |
    // | <links_len>  | <link_1> <link_2>... | <incoming_links_ptr> |     <data>     |  <label>  |
    if (M_ > ((SIZE_MAX - sizeof(void *) - sizeof(linklistsizeint)) / sizeof(idType)) + 1)
        throw std::runtime_error("NGT index parameter M is too large: argument overflow");
    size_links_level0_ = sizeof(linklistsizeint) + M_ * sizeof(idType) + sizeof(void *);

    if (size_links_level0_ > SIZE_MAX - data_size_ - sizeof(labelType))
        throw std::runtime_error("NGT index parameter M is too large: argument overflow");
    size_data_per_element_ = size_links_level0_ + data_size_ + sizeof(labelType);

    // No need to test for overflow because we passed the test for size_links_level0_ and this is
    // less.
    incoming_links_offset0 = M_ * sizeof(idType) + sizeof(linklistsizeint);
    offsetData_ = size_links_level0_;
    label_offset_ = size_links_level0_ + data_size_;
    offsetLevel0_ = 0;

    data_level0_memory_ =
        (char *)this->allocator->callocate(max_elements_ * size_data_per_element_);
    if (data_level0_memory_ == nullptr)
        throw std::runtime_error("Not enough memory");

    VPtree_root.left = &VPtree_dummy;
    VPtree_root.right = &VPtree_dummy;
    VPtree_dummy.left = &VPtree_root;
    VPtree_dummy.right = &VPtree_root;
}

template <typename DataType, typename DistType>
NGTIndex<DataType, DistType>::~NGTIndex() {
    if (max_id != INVALID_ID) {
        for (idType id = 0; id <= max_id; id++) {
            delete getIncomingEdgesPtr(id);
        }
    }

    this->allocator->free_allocation(data_level0_memory_);
}

/**
 * Index API functions
 */
template <typename DataType, typename DistType>
void NGTIndex<DataType, DistType>::resizeIndex(size_t new_max_elements) {
    resizeLabelLookup(new_max_elements);
    idToLeaf.reserve(new_max_elements);
#ifdef ENABLE_PARALLELIZATION
    visited_nodes_handler_pool = std::unique_ptr<VisitedNodesHandlerPool>(
        new (this->allocator)
            VisitedNodesHandlerPool(this->pool_initial_size, new_max_elements, this->allocator));
    std::vector<std::mutex>(new_max_elements).swap(link_list_locks_);
#else
    visited_nodes_handler = std::unique_ptr<VisitedNodesHandler>(
        new (this->allocator) VisitedNodesHandler(new_max_elements, this->allocator));
#endif
    // Reallocate base layer
    char *data_level0_memory_new = (char *)this->allocator->reallocate(
        data_level0_memory_, new_max_elements * size_data_per_element_);
    if (data_level0_memory_new == nullptr)
        throw std::runtime_error("Not enough memory: resizeIndex failed to allocate base layer");
    data_level0_memory_ = data_level0_memory_new;

    max_elements_ = new_max_elements;
}

template <typename DataType, typename DistType>
int NGTIndex<DataType, DistType>::removeVector(const idType element_internal_id) {

    vecsim_stl::vector<bool> neighbours_bitmap(this->allocator);

    removeFromTree(element_internal_id);

    // go over the graph and repair connections
    linklistsizeint *neighbours_list = get_linklist(element_internal_id);
    unsigned short neighbours_count = getListCount(neighbours_list);
    auto *neighbours = (idType *)(neighbours_list + 1);
    // reset the neighbours' bitmap for the current level.
    neighbours_bitmap.assign(max_id + 1, false);
    // store the deleted element's neighbours set in a bitmap for fast access.
    for (size_t j = 0; j < neighbours_count; j++) {
        neighbours_bitmap[neighbours[j]] = true;
    }
    // go over the neighbours that also points back to the removed point and make a local
    // repair.
    for (size_t i = 0; i < neighbours_count; i++) {
        idType neighbour_id = neighbours[i];
        linklistsizeint *neighbour_neighbours_list = get_linklist(neighbour_id);
        unsigned short neighbour_neighbours_count = getListCount(neighbour_neighbours_list);

        auto *neighbour_neighbours = (idType *)(neighbour_neighbours_list + 1);
        bool bidirectional_edge = false;
        for (size_t j = 0; j < neighbour_neighbours_count; j++) {
            // if the edge is bidirectional, do repair for this neighbor
            if (neighbour_neighbours[j] == element_internal_id) {
                bidirectional_edge = true;
                repairConnectionsForDeletion(element_internal_id, neighbour_id, neighbours_list,
                                             neighbour_neighbours_list, neighbours_bitmap);
                break;
            }
        }

        // if this edge is uni-directional, we should remove the element from the neighbor's
        // incoming edges.
        if (!bidirectional_edge) {
            auto *neighbour_incoming_edges = getIncomingEdgesPtr(neighbour_id);
            neighbour_incoming_edges->erase(std::find(neighbour_incoming_edges->begin(),
                                                      neighbour_incoming_edges->end(),
                                                      element_internal_id));
        }
    }

    // next, go over the rest of incoming edges (the ones that are not bidirectional) and make
    // repairs.
    auto *incoming_edges = getIncomingEdgesPtr(element_internal_id);
    for (auto incoming_edge : *incoming_edges) {
        linklistsizeint *incoming_node_neighbours_list = get_linklist(incoming_edge);
        repairConnectionsForDeletion(element_internal_id, incoming_edge, neighbours_list,
                                     incoming_node_neighbours_list, neighbours_bitmap);
    }
    delete incoming_edges;

    // Swap the last id with the deleted one, and invalidate the last id data.
    if (max_id == element_internal_id) {
        // we're deleting the last internal id, just invalidate data without swapping.
        memset(data_level0_memory_ + max_id * size_data_per_element_ + offsetLevel0_, 0,
               size_data_per_element_);
    } else {
        SwapLastIdWithDeletedId(element_internal_id);
    }
    --cur_element_count;
    --max_id;

    // If we need to free a complete block & there is a least one block between the
    // capacity and the size.
    if (cur_element_count % this->blockSize == 0 &&
        cur_element_count + this->blockSize <= max_elements_) {

        // Check if the capacity is aligned to block size.
        size_t extra_space_to_free = max_elements_ % this->blockSize;

        // Remove one block from the capacity.
        this->resizeIndex(max_elements_ - this->blockSize - extra_space_to_free);
    }
    return true;
}

template <typename DataType, typename DistType>
int NGTIndex<DataType, DistType>::appendVector(const void *vector_data, const labelType label) {

    idType cur_c;

    DataType normalized_blob[this->dim]; // This will be use only if metric == VecSimMetric_Cosine
    if (this->metric == VecSimMetric_Cosine) {
        memcpy(normalized_blob, vector_data, this->dim * sizeof(DataType));
        normalizeVector(normalized_blob, this->dim);
        vector_data = normalized_blob;
    }

    {
#ifdef ENABLE_PARALLELIZATION
        std::unique_lock<std::mutex> templock_curr(cur_element_count_guard_);
#endif

        if (cur_element_count >= max_elements_) {
            size_t vectors_to_add = this->blockSize - max_elements_ % this->blockSize;
            resizeIndex(max_elements_ + vectors_to_add);
        }
        cur_c = max_id = cur_element_count++;
        setVectorId(label, cur_c);
    }
#ifdef ENABLE_PARALLELIZATION
    std::unique_lock<std::mutex> lock_el(link_list_locks_[cur_c]);
    std::unique_lock<std::mutex> entry_point_lock(global);
#endif

    memset(data_level0_memory_ + cur_c * size_data_per_element_ + offsetLevel0_, 0,
           size_data_per_element_);

    // Initialisation of the data and label
    setExternalLabel(cur_c, label);
    memcpy(getDataByInternalId(cur_c), vector_data, data_size_);

    // this condition only means that we are not inserting the first element.
    if (cur_element_count > 1) {
        CandidatesFromTree<DataType, DistType> candidates = insertToTree(cur_c, vector_data);
        candidatesMaxHeap<DistType> top_candidates =
            searchGraph(candidates, vector_data, ef_construction_);
        mutuallyConnectNewElement(cur_c, top_candidates);
    } else {
        // No need for graph links for the first element
        setIncomingEdgesPtr(cur_c,
                            new (this->allocator) vecsim_stl::vector<idType>(this->allocator));
        insertToTree(cur_c, vector_data);
    }

    return true;
}

template <typename DataType, typename DistType>
candidatesLabelsMaxHeap<DistType> *NGTIndex<DataType, DistType>::searchGraph_WithTimeout(
    CandidatesFromTree<DataType, DistType> &initial_candidates, const void *data_point, size_t ef,
    size_t k, void *timeoutCtx, VecSimQueryResult_Code *rc) const {

#ifdef ENABLE_PARALLELIZATION
    this->visited_nodes_handler =
        this->visited_nodes_handler_pool->getAvailableVisitedNodesHandler();
#endif

    tag_t visited_tag = this->visited_nodes_handler->getFreshTag();

    candidatesLabelsMaxHeap<DistType> *top_candidates = getNewMaxPriorityQueue();
    candidatesMaxHeap<DistType> candidate_set(this->allocator);

    for (auto id : initial_candidates) {
        DistType dist = this->dist_func(data_point, getDataByInternalId(id), this->dim);
        top_candidates->emplace(dist, getExternalLabel(id));
        candidate_set.emplace(-dist, id);
        this->visited_nodes_handler->tagNode(id, visited_tag);
    }
    while (top_candidates->size() > ef) {
        top_candidates->pop();
    }

    DistType lowerBound = top_candidates->top().first;

    while (!candidate_set.empty()) {
        pair<DistType, idType> curr_el_pair = candidate_set.top();
        if ((-curr_el_pair.first) > lowerBound && top_candidates->size() >= ef) {
            break;
        }
        if (__builtin_expect(VecSimIndexInterface::timeoutCallback(timeoutCtx), 0)) {
            *rc = VecSim_QueryResult_TimedOut;
            return top_candidates;
        }
        candidate_set.pop();

        lowerBound = processCandidate(curr_el_pair.second, data_point, ef, visited_tag,
                                      *top_candidates, candidate_set, lowerBound);
    }
#ifdef ENABLE_PARALLELIZATION
    visited_nodes_handler_pool->returnVisitedNodesHandlerToPool(this->visited_nodes_handler);
#endif
    while (top_candidates->size() > k) {
        top_candidates->pop();
    }
    *rc = VecSim_QueryResult_OK;
    return top_candidates;
}

template <typename DataType, typename DistType>
VecSimQueryResult_List NGTIndex<DataType, DistType>::topKQuery(const void *query_data, size_t k,
                                                               VecSimQueryParams *queryParams) {

    VecSimQueryResult_List rl = {0};
    this->last_mode = STANDARD_KNN;

    if (cur_element_count == 0 || k == 0) {
        rl.code = VecSim_QueryResult_OK;
        rl.results = array_new<VecSimQueryResult>(0);
        return rl;
    }

    void *timeoutCtx = nullptr;

    DataType normalized_blob[this->dim]; // This will be use only if metric == VecSimMetric_Cosine.
    if (this->metric == VecSimMetric_Cosine) {
        memcpy(normalized_blob, query_data, this->dim * sizeof(DataType));
        normalizeVector(normalized_blob, this->dim);
        query_data = normalized_blob;
    }
    // Get original efRuntime and store it.
    size_t ef = ef_;

    if (queryParams) {
        timeoutCtx = queryParams->timeoutCtx;
        if (queryParams->ngtRuntimeParams.efRuntime) {
            ef = queryParams->ngtRuntimeParams.efRuntime;
        }
    }

    auto candidates = searchTree(&this->VPtree_root, query_data, ef);

    if (__builtin_expect(VecSimIndexInterface::timeoutCallback(timeoutCtx), 0)) {
        rl.code = VecSim_QueryResult_TimedOut;
        return rl;
    }

    // We now oun the results heap, we need to free (delete) it when we done
    candidatesLabelsMaxHeap<DistType> *results =
        searchGraph_WithTimeout(candidates, query_data, std::max(ef, k), k, timeoutCtx, &rl.code);

    if (VecSim_OK == rl.code) {
        rl.results = array_new_len<VecSimQueryResult>(results->size(), results->size());
        for (int i = (int)results->size() - 1; i >= 0; --i) {
            VecSimQueryResult_SetId(rl.results[i], results->top().second);
            VecSimQueryResult_SetScore(rl.results[i], results->top().first);
            results->pop();
        }
    }
    delete results;
    return rl;
}

template <typename DataType, typename DistType>
VecSimQueryResult *NGTIndex<DataType, DistType>::searchRangeGraph_WithTimeout(
    CandidatesFromTree<DataType, DistType> &initial_candidates, const void *data_point,
    double epsilon, DistType radius, void *timeoutCtx, VecSimQueryResult_Code *rc) const {

    *rc = VecSim_QueryResult_OK;
    auto res_container = getNewResultsContainer(10); // arbitrary initial cap.

#ifdef ENABLE_PARALLELIZATION
    this->visited_nodes_handler =
        this->visited_nodes_handler_pool->getAvailableVisitedNodesHandler();
#endif

    tag_t visited_tag = this->visited_nodes_handler->getFreshTag();
    candidatesMaxHeap<DistType> candidate_set(this->allocator);

    // Set the initial effective-range to be at least the distance from the entry-point.
    for (auto id : initial_candidates) {
        DistType dist = this->dist_func(data_point, getDataByInternalId(id), this->dim);
        if (dist <= radius) {
            // Entry-point is within the radius - add it to the results.
            res_container->emplace(getExternalLabel(id), dist);
        }
        candidate_set.emplace(-dist, id);
        this->visited_nodes_handler->tagNode(id, visited_tag);
    }

    DistType dynamic_range; // should be >= to range.
    if (res_container->size() > 0) {
        // If some vector was in range, set dynamic range to the requested range.
        dynamic_range = radius;
    } else {
        // else, set dynamic range to the best distance we found so far.
        dynamic_range = -(candidate_set.top().first);
    }

    DistType dynamic_range_search_boundaries = dynamic_range * (1.0 + epsilon);

    while (!candidate_set.empty()) {
        pair<DistType, idType> curr_el_pair = candidate_set.top();
        // If the best candidate is outside the dynamic range in more than epsilon (relatively) - we
        // finish the search.
        if ((-curr_el_pair.first) > dynamic_range_search_boundaries) {
            break;
        }
        if (__builtin_expect(VecSimIndexInterface::timeoutCallback(timeoutCtx), 0)) {
            *rc = VecSim_QueryResult_TimedOut;
            break;
        }
        candidate_set.pop();

        // Decrease the effective range, but keep dyn_range >= radius.
        if (-curr_el_pair.first < dynamic_range && -curr_el_pair.first >= radius) {
            dynamic_range = -curr_el_pair.first;
            dynamic_range_search_boundaries = dynamic_range * (1.0 + epsilon);
        }

        // Go over the candidate neighbours, add them to the candidates list if they are within the
        // epsilon environment of the dynamic range, and add them to the results if they are in the
        // requested radius.
        // Here we send the radius as double to match the function arguments type.
        processCandidate_RangeSearch(curr_el_pair.second, data_point, epsilon, visited_tag,
                                     res_container, candidate_set, dynamic_range_search_boundaries,
                                     radius);
    }

#ifdef ENABLE_PARALLELIZATION
    visited_nodes_handler_pool->returnVisitedNodesHandlerToPool(this->visited_nodes_handler);
#endif
    return res_container->get_results();
}

template <typename DataType, typename DistType>
VecSimQueryResult_List NGTIndex<DataType, DistType>::rangeQuery(const void *query_data,
                                                                double radius,
                                                                VecSimQueryParams *queryParams) {
    VecSimQueryResult_List rl = {0};
    this->last_mode = RANGE_QUERY;

    if (cur_element_count == 0) {
        rl.code = VecSim_QueryResult_OK;
        rl.results = array_new<VecSimQueryResult>(0);
        return rl;
    }
    void *timeoutCtx = nullptr;

    DataType normalized_blob[this->dim]; // This will be use only if metric == VecSimMetric_Cosine
    if (this->metric == VecSimMetric_Cosine) {
        memcpy(normalized_blob, query_data, this->dim * sizeof(DataType));
        normalizeVector(normalized_blob, this->dim);
        query_data = normalized_blob;
    }

    double epsilon = epsilon_;
    if (queryParams) {
        timeoutCtx = queryParams->timeoutCtx;
        if (queryParams->ngtRuntimeParams.epsilon) {
            epsilon = queryParams->ngtRuntimeParams.epsilon;
        }
    }

    auto candidates = searchRangeTree(query_data, radius * (1.0 + epsilon));

    if (__builtin_expect(VecSimIndexInterface::timeoutCallback(timeoutCtx), 0)) {
        rl.code = VecSim_QueryResult_TimedOut;
        return rl;
    }

    rl.results =
        searchRangeGraph_WithTimeout(candidates, query_data, epsilon, radius, timeoutCtx, &rl.code);

    return rl;
}

template <typename DataType, typename DistType>
VecSimIndexInfo NGTIndex<DataType, DistType>::info() const {

    VecSimIndexInfo info;
    info.algo = VecSimAlgo_NGT;
    info.ngtInfo.dim = this->dim;
    info.ngtInfo.type = this->vecType;
    info.ngtInfo.isMulti = this->isMulti;
    info.ngtInfo.metric = this->metric;
    info.ngtInfo.blockSize = this->blockSize;
    info.ngtInfo.M = this->getM();
    info.ngtInfo.maxPerLeaf = this->max_per_leaf_;
    info.ngtInfo.efConstruction = this->getEfConstruction();
    info.ngtInfo.efRuntime = this->getEf();
    info.ngtInfo.epsilon = this->epsilon_;
    info.ngtInfo.indexSize = this->indexSize();
    info.ngtInfo.indexLabelCount = this->indexLabelCount();
    info.ngtInfo.memory = this->allocator->getAllocationSize();
    info.ngtInfo.last_mode = this->last_mode;
    return info;
}

template <typename DataType, typename DistType>
VecSimInfoIterator *NGTIndex<DataType, DistType>::infoIterator() const {
    VecSimIndexInfo info = this->info();
    // For readability. Update this number when needed.
    size_t numberOfInfoFields = 12;
    VecSimInfoIterator *infoIterator = new VecSimInfoIterator(numberOfInfoFields);

    infoIterator->addInfoField(VecSim_InfoField{
        .fieldName = VecSimCommonStrings::ALGORITHM_STRING,
        .fieldType = INFOFIELD_STRING,
        .fieldValue = {FieldValue{.stringValue = VecSimAlgo_ToString(info.algo)}}});
    infoIterator->addInfoField(VecSim_InfoField{
        .fieldName = VecSimCommonStrings::TYPE_STRING,
        .fieldType = INFOFIELD_STRING,
        .fieldValue = {FieldValue{.stringValue = VecSimType_ToString(info.ngtInfo.type)}}});
    infoIterator->addInfoField(
        VecSim_InfoField{.fieldName = VecSimCommonStrings::DIMENSION_STRING,
                         .fieldType = INFOFIELD_UINT64,
                         .fieldValue = {FieldValue{.uintegerValue = info.ngtInfo.dim}}});
    infoIterator->addInfoField(VecSim_InfoField{
        .fieldName = VecSimCommonStrings::METRIC_STRING,
        .fieldType = INFOFIELD_STRING,
        .fieldValue = {FieldValue{.stringValue = VecSimMetric_ToString(info.ngtInfo.metric)}}});

    infoIterator->addInfoField(
        VecSim_InfoField{.fieldName = VecSimCommonStrings::IS_MULTI_STRING,
                         .fieldType = INFOFIELD_UINT64,
                         .fieldValue = {FieldValue{.uintegerValue = info.ngtInfo.isMulti}}});
    infoIterator->addInfoField(
        VecSim_InfoField{.fieldName = VecSimCommonStrings::INDEX_SIZE_STRING,
                         .fieldType = INFOFIELD_UINT64,
                         .fieldValue = {FieldValue{.uintegerValue = info.ngtInfo.indexSize}}});
    infoIterator->addInfoField(VecSim_InfoField{
        .fieldName = VecSimCommonStrings::INDEX_LABEL_COUNT_STRING,
        .fieldType = INFOFIELD_UINT64,
        .fieldValue = {FieldValue{.uintegerValue = info.ngtInfo.indexLabelCount}}});
    infoIterator->addInfoField(
        VecSim_InfoField{.fieldName = VecSimCommonStrings::M_STRING,
                         .fieldType = INFOFIELD_UINT64,
                         .fieldValue = {FieldValue{.uintegerValue = info.ngtInfo.M}}});
    infoIterator->addInfoField(
        VecSim_InfoField{.fieldName = VecSimCommonStrings::NGT_MAX_PER_LEAF_STRING,
                         .fieldType = INFOFIELD_UINT64,
                         .fieldValue = {FieldValue{.uintegerValue = info.ngtInfo.maxPerLeaf}}});
    infoIterator->addInfoField(
        VecSim_InfoField{.fieldName = VecSimCommonStrings::EF_CONSTRUCTION_STRING,
                         .fieldType = INFOFIELD_UINT64,
                         .fieldValue = {FieldValue{.uintegerValue = info.ngtInfo.efConstruction}}});
    infoIterator->addInfoField(
        VecSim_InfoField{.fieldName = VecSimCommonStrings::EF_RUNTIME_STRING,
                         .fieldType = INFOFIELD_UINT64,
                         .fieldValue = {FieldValue{.uintegerValue = info.ngtInfo.efRuntime}}});
    infoIterator->addInfoField(
        VecSim_InfoField{.fieldName = VecSimCommonStrings::MEMORY_STRING,
                         .fieldType = INFOFIELD_UINT64,
                         .fieldValue = {FieldValue{.uintegerValue = info.ngtInfo.memory}}});
    infoIterator->addInfoField(
        VecSim_InfoField{.fieldName = VecSimCommonStrings::SEARCH_MODE_STRING,
                         .fieldType = INFOFIELD_STRING,
                         .fieldValue = {FieldValue{
                             .stringValue = VecSimSearchMode_ToString(info.ngtInfo.last_mode)}}});
    infoIterator->addInfoField(
        VecSim_InfoField{.fieldName = VecSimCommonStrings::EPSILON_STRING,
                         .fieldType = INFOFIELD_FLOAT64,
                         .fieldValue = {FieldValue{.floatingPointValue = info.ngtInfo.epsilon}}});

    return infoIterator;
}

template <typename DataType, typename DistType>
bool NGTIndex<DataType, DistType>::preferAdHocSearch(size_t subsetSize, size_t k,
                                                     bool initial_check) {
    // This heuristic is based on sklearn decision tree classifier (with 20 leaves nodes) -
    // see scripts/HNSW_batches_clf.py
    size_t index_size = this->indexSize();
    if (subsetSize > index_size) {
        throw std::runtime_error("internal error: subset size cannot be larger than index size");
    }
    size_t d = this->dim;
    size_t M = this->getM();
    float r = (index_size == 0) ? 0.0f : (float)(subsetSize) / (float)this->indexLabelCount();
    bool res;

    // node 0
    if (index_size <= 30000) {
        // node 1
        if (index_size <= 5500) {
            // node 5
            res = true;
        } else {
            // node 6
            if (r <= 0.17) {
                // node 11
                res = true;
            } else {
                // node 12
                if (k <= 12) {
                    // node 13
                    if (d <= 55) {
                        // node 17
                        res = false;
                    } else {
                        // node 18
                        if (M <= 10) {
                            // node 19
                            res = false;
                        } else {
                            // node 20
                            res = true;
                        }
                    }
                } else {
                    // node 14
                    res = true;
                }
            }
        }
    } else {
        // node 2
        if (r < 0.07) {
            // node 3
            if (index_size <= 750000) {
                // node 15
                res = true;
            } else {
                // node 16
                if (k <= 7) {
                    // node 21
                    res = false;
                } else {
                    // node 22
                    if (r <= 0.03) {
                        // node 23
                        res = true;
                    } else {
                        // node 24
                        res = false;
                    }
                }
            }
        } else {
            // node 4
            if (d <= 75) {
                // node 7
                res = false;
            } else {
                // node 8
                if (k <= 12) {
                    // node 9
                    if (r <= 0.21) {
                        // node 27
                        if (M <= 57) {
                            // node 29
                            if (index_size <= 75000) {
                                // node 31
                                res = true;
                            } else {
                                // node 32
                                res = false;
                            }
                        } else {
                            // node 30
                            res = true;
                        }
                    } else {
                        // node 28
                        res = false;
                    }
                } else {
                    // node 10
                    if (M <= 10) {
                        // node 25
                        if (r <= 0.17) {
                            // node 33
                            res = true;
                        } else {
                            // node 34
                            res = false;
                        }
                    } else {
                        // node 26
                        if (index_size <= 300000) {
                            // node 35
                            res = true;
                        } else {
                            // node 36
                            if (r <= 0.17) {
                                // node 37
                                res = true;
                            } else {
                                // node 38
                                res = false;
                            }
                        }
                    }
                }
            }
        }
    }
    // Set the mode - if this isn't the initial check, we switched mode form batches to ad-hoc.
    this->last_mode =
        res ? (initial_check ? HYBRID_ADHOC_BF : HYBRID_BATCHES_TO_ADHOC_BF) : HYBRID_BATCHES;
    return res;
}
