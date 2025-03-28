project(cormorant LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Header files
set(CXX_HEADERS
    include/application.h
    include/cache.h
    include/config.h
    include/export.h
    include/glCommon.h
    include/group.h
    include/frame_buffer.h
    include/imageView.h
    include/stats.h
    include/styles.h
    include/ui.h
    include/version.h
    res/fontFA6.h
    res/fontOpenSans.h
    res/icon64.h
)
set_source_files_properties(${CXX_HEADERS} PROPERTIES HEADER_FILE_ONLY true)

# Source files
set(CXX_SRCS
    application.cpp
    cache.cpp
    config.cpp
    export.cpp
    frame_buffer.cpp
    glCommon.cpp
    group.cpp
    imageView.cpp
    main.cpp
    stats.cpp
    ui.cpp
)

# Resource file
if (WIN32)
    set(RESOURCE_FILE res/win.rc)
endif()

add_executable(cormorant ${CXX_SRCS} ${CXX_HEADERS} ${RESOURCE_FILE})
target_link_libraries(cormorant PRIVATE glad glfw imgui implot tinyfiledialogs glm stb_image tinyxml2 tinyEXIF whereami)
target_include_directories(cormorant PRIVATE include)
target_include_directories(cormorant PRIVATE res)

# Use the Windows subsystem (instead of console) for Windows release builds.
if (WIN32)
    set_target_properties(cormorant PROPERTIES LINK_FLAGS_RELEASE "/ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWS")
    set_target_properties(cormorant PROPERTIES LINK_FLAGS_MINSIZEREL "/ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWS")
endif()
