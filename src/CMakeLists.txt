project(cormorant LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

# Header files
set(CXX_HEADERS
    include/colors.h
)
set_source_files_properties(${CXX_HEADERS} PROPERTIES HEADER_FILE_ONLY true)

# Source files
set(CXX_SRCS
    main.cpp
)

add_executable(cormorant ${CXX_SRCS} ${CXX_HEADERS})
target_link_libraries(cormorant PRIVATE glad glfw imgui)
