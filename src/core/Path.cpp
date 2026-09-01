#include "core/Path.h"

#include <algorithm>

namespace td::core {

Path::Path(std::vector<Vec2> waypoints) : points_(std::move(waypoints)) {
    cumulative_.reserve(points_.size());
    float run = 0.0f;
    for (size_t i = 0; i < points_.size(); ++i) {
        if (i > 0) run += distance(points_[i - 1], points_[i]);
        cumulative_.push_back(run);
    }
}

Vec2 Path::positionAt(float d) const {
    if (points_.empty()) return {0.0f, 0.0f};
    if (points_.size() == 1) return points_.front();

    const float total = totalLength();
    if (d <= 0.0f) return points_.front();
    if (d >= total) return points_.back();

    // First waypoint whose arc length exceeds d; the segment ending there is the
    // one we are on. upper_bound never returns begin() here because d > 0.
    const auto it = std::upper_bound(cumulative_.begin(), cumulative_.end(), d);
    const size_t hi = static_cast<size_t>(it - cumulative_.begin());
    const size_t lo = hi - 1;

    const float segLen = cumulative_[hi] - cumulative_[lo];
    if (segLen <= 0.0f) return points_[hi];  // zero-length segment: no divide

    const float t = (d - cumulative_[lo]) / segLen;
    return lerp(points_[lo], points_[hi], t);
}

}  // namespace td::core
