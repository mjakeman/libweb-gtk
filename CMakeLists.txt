cmake_minimum_required(VERSION 3.16...3.22)

project(ladybird
        VERSION 0.0.1
        LANGUAGES CXX
        DESCRIPTION "An embedding API for LibWeb and LibJS"
)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
# See slide 100 of the following ppt :^)
# https://crascit.com/wp-content/uploads/2019/09/Deep-CMake-For-Library-Authors-Craig-Scott-CppCon-2019.pdf
if (NOT APPLE)
    set(CMAKE_INSTALL_RPATH $ORIGIN;$ORIGIN/../${CMAKE_INSTALL_LIBDIR})
endif()
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/EnableLLD.cmake)

if (ENABLE_ADDRESS_SANITIZER)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

if (ENABLE_MEMORY_SANITIZER)
    add_compile_options(-fsanitize=memory -fsanitize-memory-track-origins -fno-omit-frame-pointer)
    add_link_options(-fsanitize=memory -fsanitize-memory-track-origins)
endif()

if (ENABLE_UNDEFINED_SANITIZER)
    add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=undefined)
endif()

# Lagom
# FIXME: PROJECT_IS_TOP_LEVEL with CMake 3.21+
set(LADYBIRD_IS_TOP_LEVEL FALSE)
set(LADYBIRD_CUSTOM_TARGET_SUFFIX "-ladybird")
if ("${CMAKE_BINARY_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
    set(LADYBIRD_IS_TOP_LEVEL TRUE)
    set(LADYBIRD_CUSTOM_TARGET_SUFFIX "")
endif()

if (LADYBIRD_IS_TOP_LEVEL)
    get_filename_component(
            SERENITY_SOURCE_DIR "${ladybird_SOURCE_DIR}/serenity"
            ABSOLUTE
    )
    list(APPEND CMAKE_MODULE_PATH "${SERENITY_SOURCE_DIR}/Meta/CMake")
    include(cmake/EnableLagom.cmake)
    include(lagom_compile_options NO_POLICY_SCOPE)
else()
    # FIXME: Use SERENITY_SOURCE_DIR in Lagom/CMakeLists.txt
    set(SERENITY_SOURCE_DIR "${SERENITY_PROJECT_ROOT}")
endif()

add_compile_options(-DAK_DONT_REPLACE_STD)
add_compile_options(-Wno-expansion-to-defined)
add_compile_options(-Wno-user-defined-literals)

# Needed by Gtkmm
add_compile_options(-fexceptions)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(BROWSER_SOURCE_DIR ${SERENITY_SOURCE_DIR}/Userland/Applications/Browser/)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED gtkmm-4.0)
pkg_check_modules(SOUP3 REQUIRED libsoup-3.0)

include_directories(${GTK4_INCLUDE_DIRS})
link_directories(${GTK4_LIBRARY_DIRS})
add_definitions(${GTK4_CFLAGS_OTHER})

include_directories(${SOUP3_INCLUDE_DIRS})
link_directories(${SOUP3_LIBRARY_DIRS})
add_definitions(${SOUP3_CFLAGS_OTHER})

add_subdirectory(src)

if(NOT CMAKE_SKIP_INSTALL_RULES)
    include(cmake/InstallRules.cmake)
endif()

include(CTest)
#if (BUILD_TESTING)
#    add_test(
#        NAME LibWeb
#        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/../bin/headless-browser --run-tests ${SERENITY_SOURCE_DIR}/Tests/LibWeb
#    )
#    set_tests_properties(LibWeb PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
#endif()
