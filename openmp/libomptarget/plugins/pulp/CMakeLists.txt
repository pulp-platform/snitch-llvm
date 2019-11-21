##===----------------------------------------------------------------------===##
#
#                   Not the LLVM Compiler Infrastructure
#
# This file is dual licensed under the MIT and the University of Illinois Open
# Source Licenses. See LICENSE.txt for details.
#
##===----------------------------------------------------------------------===##
#
# Build a plugin for a PULP accelerator.
#
##===----------------------------------------------------------------------===##
set(LIBOMPTARGET_PULP_BUILD ON CACHE BOOL
  "Should the PULP offloading plugin be built")

if(NOT LIBOMPTARGET_PULP_BUILD)
  libomptarget_say("Not building PULP offloading plugin: build disabled.")
  return()
endif()

if(NOT EXISTS $ENV{HERO_PULP_INC_DIR}/pulp.h)
  libomptarget_say("Not building PULP offloading plugin: PULP SDK not found (HERO_PULP_INC_DIR to the correct path)")
  return()
endif()
libomptarget_say("Building PULP offloading plugin.")

# Define the suffix for the runtime messaging dumps.
add_definitions(-DTARGET_NAME=PULP)
set(CMAKE_CXX_FLAGS "-fpermissive")
#set(CMAKE_CXX_FLAGS "-fms-extensions -Wno-error=reserved-user-defined-literal")

# set flags (FIXME: include with idirafter to prevent problems with using accelerator definitions of pulp.h)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} $ENV{LIBGOMP_PLUGIN_PULP_HERO_CPPFLAGS} -lpthread -idirafter $ENV{HERO_PULP_INC_DIR}")
set(CMAKE_LD_FLAGS "${CMAKE_LD_FLAGS} $ENV{LIBGOMP_PLUGIN_PULP_HERO_LDFLAGS} -lpthread")

if(EXISTS $ENV{HERO_LIBPULP_DIR}/inc/pulp.h)
  include_directories($ENV{HERO_LIBPULP_DIR}/inc)
endif()

include_directories(${LIBOMPTARGET_DEP_LIBFFI_INCLUDE_DIR})
include_directories(${LIBOMPTARGET_DEP_LIBELF_INCLUDE_DIR})

if(LIBOMPTARGET_ENABLE_PREM)
  add_definitions(-DPREM_MODE)
endif()

add_library(omptarget.rtl.pulp SHARED
  rtl.cpp
)

# Install plugin under the lib destination folder.
install(TARGETS omptarget.rtl.pulp LIBRARY DESTINATION "${OPENMP_INSTALL_LIBDIR}")

if(LIBOMPTARGET_ENABLE_PREM)
  target_link_libraries(omptarget.rtl.pulp
    ${LIBOMPTARGET_DEP_LIBFFI_LIBRARIES}
    ${LIBOMPTARGET_DEP_LIBELF_LIBRARIES}
    "cmux-vote"
    "premnotify"
  )
else()
  target_link_libraries(omptarget.rtl.pulp
    ${LIBOMPTARGET_DEP_LIBFFI_LIBRARIES}
    ${LIBOMPTARGET_DEP_LIBELF_LIBRARIES}
  )
endif()

if(EXISTS $ENV{HERO_LIBPULP_DIR}/lib/libpulp.so)
  target_link_libraries(omptarget.rtl.pulp
      $ENV{HERO_LIBPULP_DIR}/lib/libpulp.so)
else()
  target_link_libraries(omptarget.rtl.pulp
      libpulp.so)
endif()

# Report to the parent scope that we are building a plugin for PULP.
set(LIBOMPTARGET_SYSTEM_TARGETS "${LIBOMPTARGET_SYSTEM_TARGETS} pulp" PARENT_SCOPE)
