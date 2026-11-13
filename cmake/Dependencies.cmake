# Dependencies are provided by vcpkg when configured with its toolchain
# (see CMakePresets.json). Without it, FIND_PACKAGE_ARGS makes FetchContent
# look for an installed package first and download it only if not found.
include(FetchContent)

# JSON parsing (sprite definitions)
FetchContent_Declare(nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  FIND_PACKAGE_ARGS NAMES nlohmann_json)

# Unit tests
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.16.1
  GIT_SHALLOW TRUE
  FIND_PACKAGE_ARGS 3 CONFIG)
