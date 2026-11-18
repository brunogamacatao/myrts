#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace myrts {

/// What checking a game package found: some facts about it (for display) and the problems.
struct PackageReport {
  std::vector<std::string> info;
  std::vector<std::string> problems;
  [[nodiscard]] bool ok() const { return problems.empty(); }
};

/// Loads a whole game package from disk (manifest, data tables, rules, tile sets, campaign) and checks it: the files
/// it names exist, and every level builds a playable match. Used by `myrts_tool validate` and by the editor.
[[nodiscard]] PackageReport check_package(const std::filesystem::path& directory);

}  // namespace myrts
