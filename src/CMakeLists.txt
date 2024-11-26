project(cormorant LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

# Header files
set(CXX_HEADERS
    include/application.h
    include/cache.h
    include/group.h
    include/frame_buffer.h
    include/glCommon.h
    include/imageView.h
    include/ui.h
)
set_source_files_properties(${CXX_HEADERS} PROPERTIES HEADER_FILE_ONLY true)

# Source files
set(CXX_SRCS
    application.cpp
    cache.cpp
    group.cpp
    frame_buffer.cpp
    imageView.cpp
    main.cpp
    ui.cpp
)

add_executable(cormorant ${CXX_SRCS} ${CXX_HEADERS})
target_link_libraries(cormorant PRIVATE glad glfw imgui tinyfiledialogs glm stb_image tinyxml2 tinyEXIF)
