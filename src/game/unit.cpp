#include "unit.hpp"

#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>

#include "core/pathfinding.hpp"

using json = nlohmann::json;

namespace {
// How long a unit waits for a tile held by another unit before looking for a way around.
constexpr double kRepathDelay = 0.25;

std::optional<Command> parse_command(const std::string& s) {
  if (s == "move") return Command::Move;
  if (s == "stop") return Command::Stop;
  if (s == "attack") return Command::Attack;
  if (s == "repair") return Command::Repair;
  if (s == "harvest") return Command::Harvest;
  if (s == "build") return Command::Build;
  return std::nullopt;
}
}  // namespace

const char* command_label(Command c) {
  switch (c) {
    case Command::Move: return "Move";
    case Command::Stop: return "Stop";
    case Command::Attack: return "Attack";
    case Command::Repair: return "Repair";
    case Command::Harvest: return "Harvest";
    case Command::Build: return "Build";
  }
  return "";
}

std::expected<std::shared_ptr<const UnitType>, std::string> load_unit_type(const std::string& filename) {
  std::ifstream f(filename);
  if (!f) return std::unexpected("cannot open " + filename);
  try {
    const json data = json::parse(f);
    auto type = std::make_shared<UnitType>();
    type->name = data.at("name").get<std::string>();
    type->level = data.value("level", 1);
    type->max_hp = data.at("hp").get<int>();
    type->move_speed = data.value("move_speed", 64.0);

    const auto& s = data.at("stats");
    type->stats = {s.value("armor", 0), s.value("damage_min", 0), s.value("damage_max", 0),
                   s.value("range", 0), s.value("sight", 0),      s.value("speed", 0)};

    for (const auto& c : data.value("commands", json::array())) {
      if (const auto cmd = parse_command(c.get<std::string>())) type->commands.push_back(*cmd);
    }

    auto sprite = load_sprite_def(data.at("sprite").get<std::string>());
    if (!sprite) return std::unexpected(sprite.error());
    type->sprite = std::move(*sprite);
    return type;
  } catch (const json::exception& e) {
    return std::unexpected(filename + ": " + e.what());
  }
}

Unit::Unit(int id, std::shared_ptr<const UnitType> type, myrts::Point tile, int tile_size,
           myrts::Occupancy& occupancy)
    : id_(id),
      type_(std::move(type)),
      tile_size_(tile_size),
      hp_(type_->max_hp),
      x_(static_cast<float>(tile.x * tile_size + tile_size / 2)),
      y_(static_cast<float>(tile.y * tile_size + tile_size / 2)),
      tile_(tile) {
  anim_.current = "idle_" + facing_;
  occupancy.try_reserve(tile_, id_);
}

myrts::Rect Unit::box() const {
  const auto s = static_cast<float>(tile_size_);
  return {x_ - s / 2, y_ - s / 2, s, s};
}

void Unit::set_path(std::deque<myrts::Point> path) {
  if (!path.empty() && path.front() == anchor()) path.pop_front();
  path_ = std::move(path);
  if (!path_.empty()) goal_ = path_.back();
  blocked_time_ = 0.0;
}

void Unit::stop() {
  path_.clear();
  blocked_time_ = 0.0;
}

void Unit::face(float dx, float dy) {
  std::string dir;
  if (dy > 0.1f) dir += "down";
  if (dy < -0.1f) dir += "up";
  if (dx > 0.1f) dir += dir.empty() ? "right" : "_right";
  if (dx < -0.1f) dir += dir.empty() ? "left" : "_left";
  if (!dir.empty()) facing_ = dir;
}

void Unit::face_tile(myrts::Point target) {
  face(static_cast<float>(target.x - tile_.x), static_cast<float>(target.y - tile_.y));
}

void Unit::begin_step(myrts::Point next) {
  next_ = next;
  moving_ = true;
  blocked_time_ = 0.0;
}

void Unit::try_advance(double dt, const myrts::Grid& grid, myrts::Occupancy& occupancy) {
  if (moving_ || path_.empty()) return;

  const myrts::Point next = path_.front();
  if (occupancy.try_reserve(next, id_)) {
    path_.pop_front();
    begin_step(next);
    return;
  }

  blocked_time_ += dt;
  if (blocked_time_ < kRepathDelay) return;

  // Another unit is in the way: look for a route around every tile held by others.
  const myrts::Grid others = occupancy.blocked_for(id_);
  auto detour = myrts::a_star(grid, tile_, goal_, &others);
  if (detour.empty()) {
    path_.clear();  // no way through; stay put
  } else {
    detour.pop_front();  // current tile
    path_ = std::move(detour);
  }
  blocked_time_ = 0.0;
}

void Unit::update(double dt, const myrts::Grid& grid, myrts::Occupancy& occupancy) {
  try_advance(dt, grid, occupancy);

  if (moving_) {
    const float tx = static_cast<float>(next_.x * tile_size_ + tile_size_ / 2);
    const float ty = static_cast<float>(next_.y * tile_size_ + tile_size_ / 2);
    const float dx = tx - x_;
    const float dy = ty - y_;
    const float dist = std::hypot(dx, dy);
    const float step = static_cast<float>(type_->move_speed * dt);

    face(dx, dy);
    if (dist <= step) {
      x_ = tx;
      y_ = ty;
      occupancy.release(tile_, id_);
      tile_ = next_;
      moving_ = false;
      try_advance(0.0, grid, occupancy);  // chain into the next step without an idle frame
    } else {
      x_ += dx / dist * step;
      y_ += dy / dist * step;
    }
  }

  std::string prefix = moving_ ? "walk_" : "idle_";
  if (carrying > 0) prefix += cargo == Cargo::Gold ? "gold_" : "wood_";
  if (chopping_ && !moving_) prefix = "chop_";
  set_animation(anim_, prefix + facing_);
  update_animation(*type_->sprite, anim_, dt);
}
