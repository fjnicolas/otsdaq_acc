# ======================================================================
#  otsdaq-acc main build file
#
#  cd .../path/to/build/directory
#  source .../path/to/otsdaq-acc/ups/setup_for_development
#  buildtool
# ======================================================================

# use cmake 3.17 or later
cmake_minimum_required (VERSION 3.17)

find_package(cetmodules)
set(CET_CETBUILDTOOLS_COMPAT TRUE)
include(Compatibility)
set(CET_WARN_DEPRECATED)

project(otsdaq-acc VERSION 1.00.00)

set(CMAKE_MODULE_PATH 
      $ENV{ART_DIR}/Modules
      $ENV{CETBUILDTOOLS_DIR}/Modules
      $ENV{XDAQ_DIR}/Modules
      ${CMAKE_MODULE_PATH}
)

include(CetCMakeEnv)
cet_cmake_env()

cet_set_compiler_flags(DIAGS VIGILANT
  WERROR
  NO_UNDEFINED
  EXTRA_FLAGS -pedantic -Wno-error=class-memaccess -Wno-unused-result -Wno-pessimizing-move -Wno-error=deprecated-copy -Wno-builtin-macro-redefined -Wno-error=format-overflow -Wno-mismatched-new-delete -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-maybe-uninitialized -Wno-ignored-qualifiers -Wno-unused-variable -Wno-deprecated-declarations -Wno-array-bounds -Wno-range-loop-construct
)
 
cet_remove_compiler_flags(-std=c++98)
#cet_report_compiler_flags()

# these are minimum required versions, not the actual product versions
find_ups_product( TRACE v3_17_00 )
find_ups_product( cetbuildtools v3_03_00 )
find_ups_product( artdaq v0_04_02 )
find_ups_product( artdaq_core v1_00_00 )
find_ups_product( artdaq_utilities v1_00_00 )
find_ups_product( art v1_06_00 )
find_ups_product( fhiclcpp v2_16_00 )
find_ups_product( messagefacility v1_10_00 )
find_ups_product( cetlib v1_03_00 )
find_ups_product( otsdaq v1_00_00 )
find_ups_product( otsdaq_utilities v1_00_00 )

 # XDAQ Extra setup
find_ups_product( xdaq v16_7_0_1)
include_directories($ENV{XDAQ_INC}/linux)
link_directories($ENV{XDAQ_LIB})
link_directories($ENV{ARTDAQ_DATABASE_LIB})
set(XDAQ_BASIC_LIB_LIST 
    xcept
    xoap
    xdaq
    toolbox
    xerces-c
    cgicc
)

set(XDAQ_XERCES-C xerces-c)

find_package(MPI QUIET)

# macros for art_dictionary and simple_plugin
include(ArtMake)
include(BuildPlugins)
include(CetTest)

# Code
add_subdirectory(otsdaq-acc)

# packaging utility
include(UseCPack)
