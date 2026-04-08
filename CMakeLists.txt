cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

# On macOS, link against OBS.app bundled frameworks before any system paths.
if(APPLE)
  list(PREPEND CMAKE_PREFIX_PATH "/Applications/OBS.app/Contents/Frameworks")
endif()

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE ENABLE_FRONTEND_API)
  # Xcode generator does not always propagate INTERFACE_INCLUDE_DIRECTORIES from
  # imported targets — read the property and add it explicitly.
  get_target_property(_fe_api_includes OBS::obs-frontend-api INTERFACE_INCLUDE_DIRECTORIES)
  if(_fe_api_includes)
    target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM PRIVATE ${_fe_api_includes})
  endif()
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
  # macOS 14+ SDK removed AGL.framework; deps Qt6 .prl files still reference it.
  # Remove the AGL link dependency from Qt6::Gui to avoid linker failure on macOS 14+.
  # macOS 14+ SDK removed AGL.framework; Qt6 WrapOpenGL still references it.
  # Remove AGL from WrapOpenGL link deps to avoid linker failure on macOS 14+.
  if(APPLE AND TARGET WrapOpenGL::WrapOpenGL)
    get_target_property(_wrap_link_libs WrapOpenGL::WrapOpenGL INTERFACE_LINK_LIBRARIES)
    if(_wrap_link_libs)
      list(REMOVE_ITEM _wrap_link_libs
        "/System/Library/Frameworks/AGL.framework"
        "-framework AGL"
        "AGL")
      set_target_properties(WrapOpenGL::WrapOpenGL
        PROPERTIES INTERFACE_LINK_LIBRARIES "${_wrap_link_libs}")
    endif()
  endif()
endif()

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin.cpp
    src/chalk-source.cpp
    src/chalk-mode.cpp
    src/mark-list.cpp
    src/marks/freehand-mark.cpp
    src/marks/arrow-mark.cpp
    src/marks/circle-mark.cpp
    src/marks/cone-mark.cpp
)

target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_17)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
