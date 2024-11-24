include(FetchContent)

# ------ GLFW -------------------------------------------------------

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(GLFW_LIBRARY_TYPE "Link glfw static or dynamic" STATIC)
option(GLFW_BUILD_TESTS "" OFF)
option(GLFW_BUILD_DOCS "" OFF)
option(GLFW_INSTALL "" OFF)
option(GLFW_BUILD_EXAMPLES "" OFF)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw
    GIT_TAG        3.3.8
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
message("Fetching glfw")
FetchContent_MakeAvailable(glfw)

# ------ GLAD -------------------------------------------------------

FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

FetchContent_GetProperties(glad)

if(NOT glad_POPULATED)
    message("Fetching glad")
    FetchContent_Populate(glad)
    set(GLAD_PROFILE "core" CACHE STRING "OpenGL profile")
    set(GLAD_API "gl=4.6" CACHE STRING "API type/version pairs such as \"gl=4.6\"")
    set(GLAD_GENERATOR "c" CACHE STRING "Language to generate the binding for")
    add_subdirectory(${glad_SOURCE_DIR} ${glad_BINARY_DIR})
endif()

# ------ ImGui ------------------------------------------------------

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG        v1.91.5-docking
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

FetchContent_GetProperties(imgui)

if(NOT imgui_POPULATED)
    message("Fetching ImGui")
    FetchContent_Populate(imgui)

    add_library(imgui
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp)

    target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${glfw_SOURCE_DIR}/include)

    target_link_libraries(imgui PRIVATE glfw)
endif ()

# ------ tinyfiledialogs --------------------------------------------

add_subdirectory(tinyfiledialogs)

# ------ stb_image --------------------------------------------------

FetchContent_Declare(
    stb_image
    GIT_REPOSITORY  https://github.com/nothings/stb.git
    GIT_TAG         master
    GIT_SHALLOW     TRUE
    GIT_PROGRESS    TRUE
)

FetchContent_GetProperties(stb_image)
if(NOT stb_image_POPULATED)
    message("Fetching stb_image")
    FetchContent_Populate(stb_image)
    add_library(stb_image INTERFACE ${stb_image_SOURCE_DIR}/stb_image.h ${stb_image_SOURCE_DIR}/stb_image_resize2.h)
    target_include_directories(stb_image INTERFACE ${stb_image_SOURCE_DIR})
endif()

# ------ glm --------------------------------------------------------

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

message("Fetching glm")
FetchContent_MakeAvailable(glm)
target_compile_definitions(glm INTERFACE GLM_FORCE_SILENT_WARNINGS)
