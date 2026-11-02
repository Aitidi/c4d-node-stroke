# The 2026 SDK discovers projectdefinition.txt and uses maxon_target* variables.
# Let its own generator translate that definition; do not patch SDK frameworks.
# The 2027 SDK consumes the target_* variables below directly.
if(COMMAND MaxonUpdateProjects_UpdateCMake)
  set(nodestroke_generated_cmake "${CMAKE_CURRENT_BINARY_DIR}/nodestroke-2026.cmake")
  MaxonUpdateProjects_UpdateCMake(
    TargetName nodestroke
    ProjectFile "${CMAKE_CURRENT_SOURCE_DIR}/projectdefinition.txt"
    CMakeFile "${nodestroke_generated_cmake}"
    Status nodestroke_generated)
  if(NOT nodestroke_generated)
    message(FATAL_ERROR "The Cinema 4D SDK could not generate the NodeStroke target.")
  endif()
  include("${nodestroke_generated_cmake}")
  if(MSVC)
    set_target_properties(nodestroke PROPERTIES PDB_NAME_RELEASE nodestroke2026)
  endif()
  return()
endif()

set(target_base_name nodestroke)
set(target_platforms windows)
set(target_settings [=[
{
  "apis": ["asset.framework", "cinema.framework", "cinema_hybrid.framework", "core.framework", "graph.framework", "nodes.framework", "nodespace.framework"],
  "c4d": true,
  "moduleid": "net.aitidi.nodestroke",
  "type": "DLL"
}
]=])
set(enable_rtti OFF)
set(enable_exception_handling OFF)
set(source_directories source)
MaxonTargets_ProcessCinemaTargetVars()
if(MSVC)
  set_target_properties(${target_name} PROPERTIES PDB_NAME_RELEASE nodestroke3)
endif()
