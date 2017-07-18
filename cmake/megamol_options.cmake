
# compiler options
if(WIN32)
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG -D_DEBUG -W3 -DNOMINMAX")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG -D_NDEBUG -O3 -g0 -W3 -DNOMINMAX")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -fPIC -W3 -pedantic -std=c99")
elseif(UNIX)
  # processor word size detection
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(BITS 64)
  else()
    set(BITS 32)
  endif()
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG -D_DEBUG -Wall -pedantic -fPIC -DUNIX -D_GNU_SOURCE -D_LIN${BITS} -ggdb")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG -D_NDEBUG -O3 -g0 -Wall -pedantic -fPIC -DUNIX -D_GNU_SOURCE -D_LIN${BITS}")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-Bsymbolic")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -fPIC -DUNIX -pedantic -std=c99")
endif()


if(CMAKE_COMPILER_IS_GNUCC)
  if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.7)
    message(STATUS "Version < 4.7")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++0x")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++0x")
  else()
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++11")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++11")
  endif()
elseif(MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++11")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++11")
endif()



set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the build type." FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${CMAKE_CONFIGURATION_TYPES})

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
  set(MEGAMOL_DEBUG_BUILD ON)
  set(MEGAMOL_RELEASE_BUILD OFF)
else()# Release
  set(MEGAMOL_DEBUG_BUILD OFF)
  set(MEGAMOL_RELEASE_BUILD ON)
endif()

option(MEGAMOL_INSTALL_DEPENDENCIES "MegaMol dependencies in install" ON)
mark_as_advanced(MEGAMOL_INSTALL_DEPENDENCIES)

# Global Packages
set(CMAKE_THREAD_PREFER_PTHREAD)
find_package(Threads REQUIRED)
find_package(OpenGL REQUIRED)
