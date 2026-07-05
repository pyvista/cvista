# SPDX-License-Identifier: BSD-3-Clause
#
# Standalone C++ validator that a cvista build's parallel (STDThread) filter
# output is byte-exact with its serial (Sequential) output. Built OUT OF TREE
# against an installed/So-built cvista via find_package(VTK) -- cvista forces
# VTK_BUILD_TESTING OFF, so this cannot live in the in-tree Testing/Cxx harness.
#
#   cmake -S cvista-validate -B <build> -DVTK_DIR=<prefix>/lib/cmake/vtk-9.6
#   cmake --build <build>
#   <build>/cvista_smp_parity      # exits nonzero on any parallel!=serial divergence
cmake_minimum_required(VERSION 3.16...4.0)
project(cvista_smp_parity CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(VTK REQUIRED COMPONENTS
  CommonCore
  CommonDataModel
  CommonExecutionModel
  CommonTransforms
  FiltersCore
  FiltersGeneral
  FiltersGeometry
  FiltersSources
  ImagingCore)

add_executable(cvista_smp_parity
  main.cxx
  smpParityCases.cxx
  smpParityCompare.cxx)

target_link_libraries(cvista_smp_parity PRIVATE ${VTK_LIBRARIES})

# Required under VTK_ENABLE_KITS: wires each module's object-factory autoinit so
# concrete overrides (arrays, cells, ...) register before main() runs.
vtk_module_autoinit(TARGETS cvista_smp_parity MODULES ${VTK_LIBRARIES})
