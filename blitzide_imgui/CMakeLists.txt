cmake_minimum_required(VERSION 3.16)
project(blitzide_imgui LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../imgui)
set(GLFW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/glfw)

# ---- Dear ImGui ----
add_library(imgui STATIC
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends ${GLFW_DIR}/include)

# ---- GLFW (vendored) ----
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(${GLFW_DIR} glfw_build)

# ---- TextEditor widget ----
add_library(texteditor STATIC TextEditor.cpp)
target_include_directories(texteditor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${IMGUI_DIR})

# ---- The IDE app ----
add_executable(blitzide_imgui
  main.cpp
  app.cpp
  prefs.cpp
  publish.cpp
  spawn.cpp
  update.cpp
  blitzlang.cpp
  filedialog.cpp
)

target_include_directories(blitzide_imgui PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
  ${GLFW_DIR}/include
)

target_link_libraries(blitzide_imgui PRIVATE
  imgui
  texteditor
  glfw
)

if(WIN32)
  target_link_libraries(blitzide_imgui PRIVATE opengl32 comdlg32)
  target_compile_definitions(blitzide_imgui PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  find_package(OpenGL REQUIRED)
  target_link_libraries(blitzide_imgui PRIVATE OpenGL::GL)
  # X11 deps for GLFW
  find_package(X11 REQUIRED)
  target_link_libraries(blitzide_imgui PRIVATE ${X11_LIBRARIES} dl m pthread)
endif()
