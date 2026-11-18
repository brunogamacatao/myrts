#include "export.hpp"

namespace fs = std::filesystem;

namespace myrts::editor {

std::expected<fs::path, std::string> export_game(Project& project, const fs::path& game_program, const fs::path& out_dir) {
  if (project.dirty()) return std::unexpected("save the game first");
  project.revalidate();
  if (!project.report().ok()) return std::unexpected("the game has problems: " + project.report().problems.front());
  std::error_code ec;
  if (!fs::is_regular_file(game_program, ec)) return std::unexpected("the game program was not found: " + game_program.generic_string());

  const std::string id = project.package().id.empty() ? "game" : project.package().id;
  const fs::path folder = out_dir / id;
  fs::remove_all(folder, ec);
  fs::create_directories(folder, ec);
  if (ec) return std::unexpected("cannot make " + folder.generic_string() + ": " + ec.message());
  const std::string program_name = id + game_program.extension().generic_string();
  fs::copy_file(game_program, folder / program_name, fs::copy_options::overwrite_existing, ec);
  if (ec) return std::unexpected("cannot copy the program: " + ec.message());
  fs::permissions(folder / program_name, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec, fs::perm_options::add, ec);
  ec.clear();
  fs::copy(project.root(), folder / "game", fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
  if (ec) return std::unexpected("cannot copy the game's files: " + ec.message());
  return folder;
}

}  // namespace myrts::editor
