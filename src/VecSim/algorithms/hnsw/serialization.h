#pragma once

#include <cstddef>
#include <string>
#include "VecSim/algorithms/hnsw/hnsw_wrapper.h"
#include "VecSim/spaces/spaces.h"

using spaces::dist_func_t;
namespace hnswlib {

// This struct is the return value of "checkIntegrity" methods (used for debugging).
typedef struct HNSWIndexMetaData {
    bool valid_state;
    long memory_usage; // in bytes
    size_t double_connections;
    size_t unidirectional_connections;
    size_t min_in_degree;
    size_t max_in_degree;
} HNSWIndexMetaData;

typedef enum EncodingVersion { EncodingVersion_V1 = 1 } EncodingVersion;

class HNSWIndexSerializer {
private:
    std::shared_ptr<hnswlib::HierarchicalNSW<float>> hnsw_index;

    void saveIndexFields(std::ofstream &output);
    void saveGraph(std::ofstream &output);
    void restoreIndexFields(std::ifstream &input, dist_func_t<float> dist_func, size_t dim);
    void restoreGraph(std::ifstream &input);
    void loadIndex_v1(std::ifstream &input, dist_func_t<float> dist_func, size_t dim);

public:
    // Wrap hnsw index.
    explicit HNSWIndexSerializer(std::shared_ptr<hnswlib::HierarchicalNSW<float>> hnsw_index);

    // Persist HNSW index into a file in the specified location.
    void saveIndex(const std::string &location);

    // Check the validity of the reproduced index.
    HNSWIndexMetaData checkIntegrity();

    // Restore the index from the file in the specified location.
    void loadIndex(const std::string &location, dist_func_t<float> dist_func, size_t dim);

    // Safe release the inner hnsw_index pointer, optionally replace it with another.
    void reset(std::shared_ptr<hnswlib::HierarchicalNSW<float>> hnsw_index = nullptr);
};

} // namespace hnswlib
