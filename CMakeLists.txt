cmake_minimum_required(VERSION 3.11)

project(hpg C)
include(FetchContent)

option(CXX "enable C++ compilation" ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")

if(CXX)
  enable_language(CXX)
endif()

set(CMAKE_CXX_STANDARD 20)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if(MSVC AND MT)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
endif()

find_package(GUROBI)

function(lier_gurobi cible)
  target_link_libraries(${cible} optimized ${GUROBI_CXX_LIBRARY}
                                debug ${GUROBI_CXX_DEBUG_LIBRARY})
  target_link_libraries(${cible} ${GUROBI_LIBRARY})
  target_compile_definitions(${cible} PRIVATE USE_GUROBI)
endfunction()

if(GUROBI_FOUND)
  message(STATUS "Gurobi found")
  include_directories(${GUROBI_INCLUDE_DIRS})


  add_executable(solver.exe src/solver.cpp)
  lier_gurobi(solver.exe)

  add_executable(solver_colgen.exe src/solver_colgen.cpp)
  lier_gurobi(solver_colgen.exe)

  add_executable(dynamic.exe src/dynamic.cpp)
  lier_gurobi(dynamic.exe)

  add_executable(stabilization_colgen.exe src/stabilization_colgen.cpp)
  lier_gurobi(stabilization_colgen.exe)

else()
  message(STATUS "Gurobi not found")
endif()

add_executable(visualisation.exe src/visualisation.cpp)
