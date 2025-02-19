#include "openmc/bin_grid_partitioner.h"
#include "openmc/error.h"
#include "openmc/partitioner_utils.h"
#include "openmc/partitioners.h"
#include "openmc/timer.h"

#include <set>

namespace openmc {

BinGridPartitioner::BinGridPartitioner(
  const Universe& univ, const BoundingBox& bounds, int32_t grid_res)
  : grid_res_(grid_res), fallback_(univ), bounds_(bounds)
{
  write_message("Building bin grid partitioner...", 5);
  Timer build_timer;
  build_timer.start();

  // Initialize
  for (int i = 0; i < 3; i++) {
    bin_dim_[i] = (bounds_.max_[i] - bounds_.min_[i]) / grid_res;
  }

  // Put all points into the bins
  auto points =
    binned_point_search<CellPointUncompressed>(univ, fallback_, bounds_);
  bin_grid_.resize(grid_res * grid_res * grid_res);
  for (const auto& p : points) {
    bin_grid_[convert_to_index(p.pos_)].push_back(p.cell_);
  }

  // Remove any redundant points that may be in the bins
  for (auto& bin : bin_grid_) {
    std::set<int> unique_cells;
    for (int cell : bin) {
      unique_cells.insert(cell);
    }
    bin.clear();
    for (int cell : unique_cells) {
      bin.push_back(cell);
    }
  }

  write_message("Bin grid partitioner construction completed in " +
                  std::to_string(build_timer.elapsed()) + " seconds.",
    PARTITIONER_PERFORMANCE_MONITORING_LOG_LEVEL);
}

BinGridPartitioner::~BinGridPartitioner() {}

BinGridPartitioner::BinGridPartitioner(
  const Universe& univ, const BoundingBox& bounds, hid_t file)
  : fallback_(univ), bounds_(bounds)
{
  hid_t bin_grid_group = open_group(file, "bin_grid");

  read_attr_int(bin_grid_group, "grid_res", &grid_res_);

  for (int i = 0; i < 3; i++) {
    bin_dim_[i] = (bounds_.max_[i] - bounds_.min_[i]) / grid_res_;
  }

  bin_grid_.resize(grid_res_ * grid_res_ * grid_res_);

  hid_t bins = open_group(bin_grid_group, "bins");
  for (int i = 0; i < bin_grid_.size(); i++) {
    read_dataset(bins, std::to_string(i).c_str(), bin_grid_[i]);
  }
  close_group(bins);

  close_group(bin_grid_group);
}

void BinGridPartitioner::export_to_hdf5(const std::string& path) const
{
  hid_t file = file_open(path, 'w');

  // Write header
  write_attribute(file, "filetype", "partitioner");

  // write general partitioner information
  write_attribute(
    file, "part_type", static_cast<int>(PartitionerTypeID::BinGrid));
  write_dataset(file, "bounds_max", bounds_.max_);
  write_dataset(file, "bounds_min", bounds_.min_);

  hid_t bin_grid_group = open_group(file, "bin_grid");

  write_attribute(bin_grid_group, "grid_res", grid_res_);

  hid_t bins = create_group(bin_grid_group, "bins");
  for (int i = 0; i < bin_grid_.size(); i++) {
    write_dataset(bins, std::to_string(i).c_str(), bin_grid_[i]);
  }
  close_group(bins);

  close_group(bin_grid_group);
  file_close(file);
}

int BinGridPartitioner::convert_to_index(const Position& r) const
{
  // The idea behind this mapping is to count, on each axis, how many bins
  // (rounded down) the point is from the minimum bounds of the bin grid
  // partitioner. This information gives us the location of a unique bins, and
  // we can convert this information into a base grid_res_ number to map it to
  // an index.
  int idx = 0;
  for (int i = 0; i < 3; i++) {
    idx = grid_res_ * idx +
          static_cast<int>((r[i] - bounds_.min_[i]) / bin_dim_[i]);
  }
  return idx;
}

const std::vector<int>& BinGridPartitioner::get_cells(
  Position r, Direction u) const
{
  if (!bounds_.contains(r)) {
    return get_cells_fallback(r, u);
  }

  return bin_grid_[convert_to_index(r)];
}

const std::vector<int>& BinGridPartitioner::get_cells_fallback(
  Position r, Direction u) const
{
  return fallback_.get_cells(r, u);
}

}; // namespace openmc