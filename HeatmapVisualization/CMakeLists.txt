
add_executable(HeatmapVisualization "")
target_sources(HeatmapVisualization
    PRIVATE
        main.cpp
        OpenGLEngineFrontend.hpp
        OpenGLEngineFrontend.cpp
        PtexMeshRenderPass.hpp
        PtexMeshRenderPass.cpp
)
set_property(TARGET HeatmapVisualization PROPERTY OUTPUT_NAME "HeatmapVisualization")
get_target_property(executable_name HeatmapVisualization OUTPUT_NAME)
set_property(TARGET HeatmapVisualization PROPERTY VS_DEBUGGER_COMMAND "${PROJECT_SOURCE_DIR}/bin/${executable_name}.exe")
set_property(TARGET HeatmapVisualization PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/")
target_link_libraries(HeatmapVisualization PUBLIC SpaceLion)
install(TARGETS HeatmapVisualization DESTINATION bin)