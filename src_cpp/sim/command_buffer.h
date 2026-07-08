#pragma once

#include <entt/entt.hpp>
#include <functional>
#include <vector>

namespace sim {

class CommandBuffer {
  public:
    void push(std::function<void(entt::registry &)> op) {
        _ops.push_back(std::move(op));
    }

    void flush(entt::registry &reg) {
        for (auto &op : _ops) {
            op(reg);
        }
        _ops.clear();
    }

    bool empty() const { return _ops.empty(); }

  private:
    std::vector<std::function<void(entt::registry &)>> _ops;
};

} // namespace sim
