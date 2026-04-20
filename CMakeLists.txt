# --- Detect if the plugin is built out of tree or not ---
if(CMAKE_PROJECT_NAME STREQUAL "obs-studio")
  set(BUILD_OUT_OF_TREE OFF)
else()
  set(BUILD_OUT_OF_TREE ON)
  cmake_minimum_required(VERSION 3.16...3.26)
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(mouse-monitor-plugin VERSION 1.0.0)

include(compilerconfig)
include(defaults)
include(helpers)

if(APPLE)
  set(WrapOpenGL_AGL "" CACHE FILEPATH "" FORCE)
  
  # Patch OBS targets to remove legacy AGL framework references
  foreach(target OBS::libobs OBS::obs-frontend-api)
    if(TARGET ${target})
      get_target_property(libs ${target} INTERFACE_LINK_LIBRARIES)
      if(libs)
        list(REMOVE_ITEM libs "-framework AGL" "AGL")
        set_target_properties(${target} PROPERTIES INTERFACE_LINK_LIBRARIES "${libs}")
      endif()
    endif()
  endforeach()
endif()

add_library(${PROJECT_NAME} MODULE)

target_link_libraries(${PROJECT_NAME} PRIVATE OBS::libobs)

if(BUILD_OUT_OF_TREE)
  find_package(libobs REQUIRED)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
else()
  target_link_libraries(${PROJECT_NAME} PRIVATE OBS::frontend-api)
endif()

# Qt
find_package(Qt6 COMPONENTS Widgets Core)
if(BUILD_OUT_OF_TREE)
  if(OS_LINUX OR OS_FREEBSD OR OS_OPENBSD)
    find_package(Qt6 REQUIRED Gui)
  endif()
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE Qt::Core Qt::Widgets)

# Fix #15: Removed unused CURL dependency

# Determine shared directory path (support both in-tree and out-of-tree builds)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/shared")
  set(OBS_SHARED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shared")
else()
  set(OBS_SHARED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../shared")
endif()

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OBS_SHARED_DIR}
)

# Extra Qt linkage on older Qt versions for *nix
if((OS_LINUX OR OS_FREEBSD OR OS_OPENBSD) AND Qt_VERSION VERSION_LESS "6.9.0")
  target_link_libraries(${PROJECT_NAME} PRIVATE Qt::GuiPrivate)
endif()

# Compiler / build flags
target_compile_options(${PROJECT_NAME} PRIVATE
  $<$<C_COMPILER_ID:Clang,AppleClang>:
    -Wno-quoted-include-in-framework-header
    -Wno-comma
  >
)

set_target_properties(${PROJECT_NAME} PROPERTIES
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
  AUTOMOC ON
  AUTOUIC ON
  AUTORCC ON
)

if(APPLE)
  find_library(CARBON_FRAMEWORK Carbon)
  find_library(APP_SERVICES_FRAMEWORK ApplicationServices)
  target_link_libraries(${PROJECT_NAME} PRIVATE ${CARBON_FRAMEWORK} ${APP_SERVICES_FRAMEWORK})
  target_link_libraries(${PROJECT_NAME} PRIVATE "-F${CMAKE_CURRENT_SOURCE_DIR}/local_frameworks")

  set_target_properties(${PROJECT_NAME} PROPERTIES
    XCODE_ATTRIBUTE_FRAMEWORK_SEARCH_PATHS "${CMAKE_BINARY_DIR}/stubs"
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "@loader_path/../Frameworks;@loader_path/../../../../Frameworks"
    MACOSX_RPATH TRUE
  )
  
  # Aggressively remove AGL from the target's link list
  get_target_property(TARGET_LIBS ${PROJECT_NAME} LINK_LIBRARIES)
  if(TARGET_LIBS)
      list(REMOVE_ITEM TARGET_LIBS "-framework AGL" "AGL" "/System/Library/Frameworks/AGL.framework")
      set_target_properties(${PROJECT_NAME} PROPERTIES LINK_LIBRARIES "${TARGET_LIBS}")
  endif()

  # Enable dead stripping of dylibs to remove unused phantom dependencies like AGL
  add_link_options("-Wl,-dead_strip_dylibs")

  # Post-build: Redirect the phantom AGL dependency to libresolv to avoid duplicate entry error with OpenGL
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
      COMMAND install_name_tool -change @rpath/AGL.framework/AGL /usr/lib/libresolv.9.dylib "$<TARGET_FILE:${PROJECT_NAME}>" || true
      COMMENT "Redirecting phantom AGL dependency to libresolv"
  )
endif()

# Version header
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
  ${CMAKE_CURRENT_SOURCE_DIR}/version.h
)

# Windows installer script template
if(OS_WINDOWS)
  configure_file(
    cmake/windows/resources/installer-Windows.iss.in
    "${CMAKE_CURRENT_BINARY_DIR}/installer-Windows.generated.iss"
  )
endif()

# Sources
target_sources(${PROJECT_NAME} PRIVATE
  mouse-monitor-plugin.cpp
  mouse-monitor-plugin.hpp
  mouse-settings-dialog.cpp
  mouse-settings-dialog.hpp
  streamup-ui.hpp
  obs-websocket-api.h
  resources.qrc
  version.h
)

# Install / properties depending on build context
if(BUILD_OUT_OF_TREE)
  # out-of-tree plugin build
  set_target_properties_plugin(${PROJECT_NAME}
    PROPERTIES
      OUTPUT_NAME ${_name}
  )
else()
  # in-tree OBS build
  set_target_properties_obs(${PROJECT_NAME}
    PROPERTIES
      FOLDER "plugins/streamup"
      PREFIX ""
  )
endif()
