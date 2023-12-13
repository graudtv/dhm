#pragma once

#include <algorithm>
#include <cassert>
#include <vector>

namespace dhm {

/* Represents integer range [FirstIdx; LastIdx)
 * Note that LastIdx is not included into the range
 */
struct WorkRangeLinear {
  int FirstIdx;
  int LastIdx;

  WorkRangeLinear(int First, int Last) : FirstIdx(First), LastIdx(Last) {
    assert(FirstIdx >= 0);
    assert(LastIdx >= 0);
  }

  int size() const { return LastIdx - FirstIdx; }
  WorkRangeLinear shift(int Offset) const {
    return WorkRangeLinear{FirstIdx + Offset, LastIdx + Offset};
  }
};

/* Example
 * Suppose we are splitting 11 work items to 4 workers
 * Then workers will have work ranges [0, 3), [3, 6), [6, 9), [9, 11),
 * i.e. work sizes are 3, 3, 3, 2
 */
class WorkSplitterLinear {
public:
  WorkSplitterLinear(int WorkSz, int NumWorkers)
      : WorkSz(WorkSz), NumWorkers(NumWorkers) {
    assert(WorkSz >= 0 && "invalid WorkSz");
    assert(NumWorkers >= 1 && "invalid NumWorkers");
  }

  WorkRangeLinear getRange(int WorkerId) const {
    assert(WorkerId >= 0 && "invalid WorkerId");
    assert(WorkerId < NumWorkers && "invalid WorkerId");

    int DefaultGroupSz = WorkSz / NumWorkers;

    if (WorkerId < WorkSz % NumWorkers) {
      int FirstIdx = WorkerId * (DefaultGroupSz + 1);
      int LastIdx = FirstIdx + (DefaultGroupSz + 1);
      return WorkRangeLinear{FirstIdx, LastIdx};
    }

    int NumOfEnlargedGroups = WorkSz % NumWorkers;
    int FirstIdx = NumOfEnlargedGroups * (DefaultGroupSz + 1) +
                   (WorkerId - NumOfEnlargedGroups) * DefaultGroupSz;
    int LastIdx = FirstIdx + DefaultGroupSz;
    return WorkRangeLinear{FirstIdx, LastIdx};
  }

  template <class T = int> std::vector<T> getSizes() const {
    std::vector<T> Sizes(NumWorkers); // {} must not be used here!
    int DefaultGroupSz = WorkSz / NumWorkers;
    int NonDefaultWorkers = WorkSz % NumWorkers;
    std::fill_n(Sizes.begin(), NonDefaultWorkers, DefaultGroupSz + 1);
    std::fill_n(Sizes.begin() + NonDefaultWorkers,
                NumWorkers - NonDefaultWorkers, DefaultGroupSz);
    return Sizes;
  }

  template <class T = int> std::vector<T> getDisplacements() const {
    std::vector<T> Displacements(NumWorkers); // {} must not be used here!
    int DefaultGroupSz = WorkSz / NumWorkers;
    int NonDefaultWorkers = WorkSz % NumWorkers;
    int Offset = 0;
    int WorkerId = 0;
    for (; WorkerId < NonDefaultWorkers; ++WorkerId) {
      Displacements[WorkerId] = Offset;
      Offset += DefaultGroupSz + 1;
    }
    for (; WorkerId < NumWorkers; ++WorkerId) {
      Displacements[WorkerId] = Offset;
      Offset += DefaultGroupSz;
    }
    return Displacements;
  }

  /* checks if every worker will have exactly the same amount of work */
  bool isEvenlyDivided() const { return WorkSz % NumWorkers == 0; }

  size_t getMinWorkSize() const { return WorkSz / NumWorkers; }

  size_t getMaxWorkSize() const {
    auto MinSz = getMinWorkSize();
    return isEvenlyDivided() ? MinSz : (MinSz + 1);
  }

private:
  int WorkSz;
  int NumWorkers;
};

} // namespace dhm
