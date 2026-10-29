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
