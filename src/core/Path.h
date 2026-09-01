#pragma once

#include <vector>

#include "core/Vec2.h"

namespace td::core {

// A fixed enemy route, parameterised by distance travelled rather than by
// waypoint index. That choice makes movement a single `distance += speed * dt`,
// makes "how far along is this enemy" a float comparison (which is what
// targetPriority=first needs), and makes leak detection `distance >= length`.
class Path {
public:
    Path() = default;
    explicit Path(std::vector<Vec2> waypoints);

    float totalLength() const { return cumulative_.empty() ? 0.0f : cumulative_.back(); }
    Vec2 positionAt(float d) const;  // clamped at both ends
    bool empty() const { return points_.empty(); }

    const std::vector<Vec2>& waypoints() const { return points_; }

private:
    std::vector<Vec2> points_;
    std::vector<float> cumulative_;  // arc length at each waypoint; cumulative_[0] == 0
};

}  // namespace td::core
