# Build all .cc files in this directory tree into a single executable called
# "v2x-conflict-5g". The default scratch CMake logic only picks up top-level
# .cc files, which is why this subdirectory needs its own CMakeLists.txt.

file(GLOB_RECURSE v2x_conflict_5g_sources CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/*.cc
)

build_exec(
  EXECNAME v2x-conflict-5g
  EXECNAME_PREFIX scratch_
  SOURCE_FILES "${v2x_conflict_5g_sources}"
  LIBRARIES_TO_LINK "${ns3-libs}" "${ns3-contrib-libs}"
  EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/v2x-conflict-5g/
)
