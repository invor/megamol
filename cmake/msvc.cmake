#message(WARNING "setting msvc compiler flags")

set(COMMON_CXX_FLAGS "/Gm /DNOMINMAX /W3 /openmp /MP /GR")

set(CMAKE_CXX_FLAGS_DEBUG "${COMMON_CXX_FLAGS} /MDd /Od /DDEBUG /D_DEBUG /Zi" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "${COMMON_CXX_FLAGS} /MD /DNDEBUG /D_NDEBUG /Ob2 /Ox /Oi" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${COMMON_CXX_FLAGS} /MD /DNDEBUG /D_NDEBUG /Ob2 /Ox /Oi /Zi" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Ox /Gm /MP /fPIC /W3 /pedantic /std=c99 /openmp" CACHE STRING "" FORCE)