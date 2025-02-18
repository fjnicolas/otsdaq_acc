# ======================================================================
#  otsdaq-components main build file
#
#  cd .../path/to/build/directory
#  source .../path/to/otsdaq-components/ups/setup_for_development
#  buildtool
# ======================================================================


# use cmake 3.17 or later
cmake_minimum_required (VERSION 3.19 FATAL_ERROR)

find_package(cetmodules 3.16.00 REQUIRED)

project(otsdaq-acc VERSION 2.10.00)

include(CetCMakeEnv)
cet_cmake_env()
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

if (CMAKE_CXX_COMPILER_ID STREQUAL Clang AND
     CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 14.0.0)
  # Clang 14 has a bug in which it complains about destructors of template classes
  # e.g. ArtdaqInput<T>::~ArtdaqInput(), when it is expecting the destructor name to
  # match what is on the left-hand side of the ::. This is a violation, however, of
  # the C++20 standard. https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p1787r6.html
  set(DT_FLAG -Wno-dtor-name)
else()
  set(DT_FLAG)
endif()

cet_set_compiler_flags(
 DIAGS 
 VIGILANT
 WERROR
 NO_UNDEFINED
 EXTRA_FLAGS -pedantic -D_LIBCPP_ENABLE_CXX17_REMOVED_FEATURES=1 ${DT_FLAG}
 )
 
find_package(otsdaq 2.06.06 REQUIRED EXPORT)
find_package(messagefacility 1.10.00 REQUIRED EXPORT)

# XDAQ Extra setup
include_directories($ENV{XDAQ_ROOT}/include/linux $ENV{XDAQ_ROOT}/include)
link_directories($ENV{XDAQ_ROOT}/lib)
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

 include(BuildPlugins)

# Code
add_subdirectory(otsdaq-acc)

# doxygen
if( $ENV{OTS_DOXY} MATCHES "DOIT" )
	add_subdirectory(doc)
endif()

cet_cmake_config()
