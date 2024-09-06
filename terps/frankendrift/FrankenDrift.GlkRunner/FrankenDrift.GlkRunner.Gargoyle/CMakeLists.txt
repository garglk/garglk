find_program(DOTNET dotnet)
if(NOT DOTNET)
    message(FATAL_ERROR "FrankenDrift requires the .NET SDK (missing \"dotnet\")")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(GARGLK_DOTNET_OS "linux")
elseif(WIN32)
    set(GARGLK_DOTNET_OS "win")
    set(GARGLK_FRANKEN_EXT ".exe")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(GARGLK_DOTNET_OS "osx")
else()
    message(FATAL_ERROR "FrankenDrift is currently not supported on this platform")
endif()

if(APPLE)
    set(COPY_DYLIB cp "${CMAKE_BINARY_DIR}/garglk/libgarglk.dylib" "${CMAKE_CURRENT_SOURCE_DIR}")
elseif(WIN32)
    set(COPY_DYLIB cmd /c exit 0)
else()
    set(COPY_DYLIB true)
endif()

add_custom_target(frankendrift
    ALL
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${COPY_DYLIB}
    COMMAND ${DOTNET} publish --self-contained -c Release --os "${GARGLK_DOTNET_OS}" -p:GarglkStatic=false -o .
    COMMENT "Building FrankenDrift with ${DOTNET}"
)

add_dependencies(frankendrift garglk)

if(DIST_INSTALL)
    install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/FrankenDrift.GlkRunner.Gargoyle${GARGLK_FRANKEN_EXT}" DESTINATION "${PROJECT_SOURCE_DIR}/build/dist")
else()
    install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/FrankenDrift.GlkRunner.Gargoyle${GARGLK_FRANKEN_EXT}" DESTINATION "${INTERPRETER_INSTALL_DIR}")
endif()
