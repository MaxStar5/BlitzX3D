cmake_minimum_required(VERSION 3.16)
project(blitzide_imgui LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../imgui)
set(SDL3_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/SDL3/cmake)
find_package(SDL3 CONFIG REQUIRED)

# ---- Dear ImGui ----
add_library(imgui STATIC
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/SDL3/include)
target_link_libraries(imgui PUBLIC SDL3::SDL3)

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
  ../theme.cpp
)

target_include_directories(blitzide_imgui PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
)

target_link_libraries(blitzide_imgui PRIVATE
  imgui
  texteditor
  SDL3::SDL3
)

add_custom_command(TARGET blitzide_imgui POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/SDL3/lib/$<IF:$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>,x64,x86>/SDL3.dll
    $<TARGET_FILE_DIR:blitzide_imgui>/SDL3.dll
)

if(WIN32)
  target_link_libraries(blitzide_imgui PRIVATE opengl32 comdlg32 wininet)
  target_compile_definitions(blitzide_imgui PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  find_package(OpenGL REQUIRED)
  target_link_libraries(blitzide_imgui PRIVATE OpenGL::GL)
  target_link_libraries(blitzide_imgui PRIVATE dl m pthread)
endif()
