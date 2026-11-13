#include "campaign.hpp"

#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>

namespace myrts {

std::expected<Campaign, std::string> Campaign::parse(std::string_view json_text) {
  using json = nlohmann::json;
  Campaign campaign;
  try {
    const json doc = json::parse(json_text);
    for (const json& l : doc.at("levels")) {
      Level level;
      level.name = l.at("name").get<std::string>();
      level.map = l.at("map").get<std::string>();
      level.briefing = l.value("briefing", std::string{});
      if (l.contains("ai")) {
        const json& a = l.at("ai");
        level.ai.start_delay = a.value("start_delay", level.ai.start_delay);
        level.ai.first_wave = a.value("first_wave", level.ai.first_wave);
        level.ai.wave_growth = a.value("wave_growth", level.ai.wave_growth);
        level.ai.desired_workers = a.value("desired_workers", level.ai.desired_workers);
        level.ai.think_interval = a.value("think_interval", level.ai.think_interval);
      }
      if (l.contains("enemy_stock")) {
        const json& s = l.at("enemy_stock");
        level.enemy_stock = Stock{s.value("gold", 0), s.value("lumber", 0), s.value("oil", 0)};
      }
      level.enemy_workers = l.value("enemy_workers", level.enemy_workers);
      campaign.levels_.push_back(std::move(level));
    }
  } catch (const std::exception& e) {
    return std::unexpected(std::string("invalid campaign: ") + e.what());
  }
  if (campaign.levels_.empty()) return std::unexpected("the campaign has no levels");
  return campaign;
}

std::expected<Campaign, std::string> Campaign::load_file(const std::string& path) {
  std::ifstream f(path);
  if (!f) return std::unexpected("cannot open " + path);
  const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  return parse(text);
}

Campaign Campaign::single(std::string map) {
  Campaign c;
  Level level;
  level.name = "Skirmish";
  level.map = std::move(map);
  c.levels_.push_back(std::move(level));
  return c;
}

}  // namespace myrts
