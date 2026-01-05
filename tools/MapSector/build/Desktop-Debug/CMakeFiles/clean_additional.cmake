# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles/MapSector_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/MapSector_autogen.dir/ParseCache.txt"
  "MapSector_autogen"
  )
endif()
