find_program(DOTNET dotnet)
if(NOT DOTNET)
    message(FATAL_ERROR "FrankenDrift requires the .NET SDK (missing \"dotnet\")")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(GARGLK_DOTNET_RID "linux")
elseif(WIN32)
    set(GARGLK_DOTNET_RID "win")
    set(GARGLK_FRANKEN_EXT ".exe")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(GARGLK_DOTNET_RID "osx")
else()
    message(FATAL_ERROR "FrankenDrift is currently not supported on this platform")
endif()

# Cannot cross-compile FrankenDrift for another OS ...
if(NOT CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME)
    message(FATAL_ERROR "Cross-compiling to another OS is not supported for FrankenDrift.")
endif()

# ... but CAN cross-compile for a different CPU architecture:
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|x64)$")
    set(GARGLK_DOTNET_RID "${GARGLK_DOTNET_RID}-x64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(i386|i686|X86)$")
    set(GARGLK_DOTNET_RID "${GARGLK_DOTNET_RID}-x86")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64|armv8l)$")
    set(GARGLK_DOTNET_RID "${GARGLK_DOTNET_RID}-arm64")
else()
    message(FATAL_ERROR "Unsupported CPU architecture for FrankenDrift: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

if(APPLE)
    set(COPY_DYLIB cp "${CMAKE_BINARY_DIR}/garglk/libgarglk.dylib" "${CMAKE_CURRENT_SOURCE_DIR}")
elseif(WIN32)
    set(COPY_DYLIB powershell.exe cp "${CMAKE_BINARY_DIR}/garglk/garglk.lib" "${CMAKE_CURRENT_SOURCE_DIR}")
else()
    set(COPY_DYLIB cp "${CMAKE_BINARY_DIR}/garglk/libgarglk.so" "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

add_custom_target(frankendrift
    ALL
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${COPY_DYLIB}
    COMMAND ${DOTNET} publish --self-contained -c Release -r "${GARGLK_DOTNET_RID}" -p:GarglkMode=dynamic "-p:IntermediateOutputPath=${CMAKE_CURRENT_BINARY_DIR}/" -o "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Building FrankenDrift with ${DOTNET}"
)

if(WIN32)
    # ensure that FrankenDrift gets proper HiDpi support on Windows by embedding the necessary manifest
    add_custom_target(fd_manifest
        ALL
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND "mt.exe" -nologo -manifest \"${PROJECT_SOURCE_DIR}\\terps\\garglk.manifest\" \"-updateresource:FrankenDrift.GlkRunner.Gargoyle.exe\"
        COMMENT "Adding FrankenDrift Manifest file"
    )
    add_dependencies(fd_manifest frankendrift)
endif()

add_dependencies(frankendrift garglk)

if(DIST_INSTALL)
    install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/FrankenDrift.GlkRunner.Gargoyle${GARGLK_FRANKEN_EXT}" DESTINATION "${PROJECT_SOURCE_DIR}/build/dist")
else()
    install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/FrankenDrift.GlkRunner.Gargoyle${GARGLK_FRANKEN_EXT}" DESTINATION "${INTERPRETER_INSTALL_DIR}")
endif()
