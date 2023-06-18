#ifndef OCTREE_PARTITIONER_H
#define OCTREE_PARTITIONER_H

#include "universe.h"
#include "aabb.h"
#include "zplane_partitioner.h"
#include <vector>
#include <set>

namespace openmc {

using namespace std;

struct OctreeNode {
  OctreeNode();

  bool is_leaf() const;
  
  AABB box;
  std::vector<int> cells;
  OctreeNode* children; // if this is a leaf, then children will be null
  int depth;
  int id;
};

class OctreePartitioner : public UniversePartitioner {
public:
  explicit OctreePartitioner(const Universe& univ, int target_cells_per_node=6, int max_depth=6);
  virtual ~OctreePartitioner() override;

  //! Return the list of cells that could contain the given coordinates.
  virtual const vector<int32_t>& get_cells(Position r, Direction u) const override;
  virtual const vector<int32_t>& get_cells_fallback(Position r, Direction u) const override;
private:
  //! The root node is where the searching begins
  OctreeNode root;

  // fallback if octree doesn't work
  ZPlanePartitioner fallback;

};

}

#endif