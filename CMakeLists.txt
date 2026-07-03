cmake_minimum_required(VERSION 3.16)
project(startup_intelligence_agent LANGUAGES CXX)

# C++20: required by libpqxx >= 8 (macOS/Homebrew); libpqxx 7.x (Debian/Ubuntu)
# also compiles cleanly at this standard, so it is the safe common denominator.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

add_compile_options(-Wall -Wextra)

# ---- Dependencies --------------------------------------------------------

# nlohmann/json: prefer a system package, else fetch it.
find_package(nlohmann_json 3.9 QUIET)
if(NOT nlohmann_json_FOUND)
  include(FetchContent)
  FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3)
  FetchContent_MakeAvailable(nlohmann_json)
endif()

find_package(CURL REQUIRED)

# MongoDB C++ driver: prefer the CMake config packages (mongo::*_shared targets),
# else fall back to pkg-config (Debian/Ubuntu libmongocxx-dev provides both).
find_package(mongocxx CONFIG QUIET)
find_package(bsoncxx CONFIG QUIET)
if(mongocxx_FOUND)
  set(MONGO_TARGETS mongo::mongocxx_shared mongo::bsoncxx_shared)
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(MONGOCXX REQUIRED IMPORTED_TARGET libmongocxx)
  add_library(mongo_iface INTERFACE)
  target_link_libraries(mongo_iface INTERFACE PkgConfig::MONGOCXX)
  set(MONGO_TARGETS mongo_iface)
endif()

# ---- Core library --------------------------------------------------------

add_library(sia_core
  src/config.cpp
  src/http_client.cpp
  src/db.cpp
  src/pipeline.cpp
  src/sources/hacker_news.cpp)

target_include_directories(sia_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(sia_core
  PUBLIC nlohmann_json::nlohmann_json
  PRIVATE ${MONGO_TARGETS} CURL::libcurl)

# ---- Executable ----------------------------------------------------------

add_executable(sia src/main.cpp)
target_link_libraries(sia PRIVATE sia_core)
install(TARGETS sia RUNTIME DESTINATION bin)

# ---- Tests ---------------------------------------------------------------

enable_testing()
add_executable(test_enrichers tests/test_enrichers.cpp)
target_link_libraries(test_enrichers PRIVATE sia_core)
add_test(NAME enrichers COMMAND test_enrichers)
