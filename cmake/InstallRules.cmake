
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

set(package WebEmbed)

#set(webembed_applications webembed SQLServer WebContent WebDriver headless-browser)
set(webembed_applications demo-browser SQLServer WebContent WebDriver)

install(TARGETS ${webembed_applications}
  EXPORT webembedTargets
  RUNTIME
    COMPONENT webembed_Runtime
    DESTINATION ${CMAKE_INSTALL_BINDIR}
  BUNDLE
    COMPONENT webembed_Runtime
    DESTINATION bundle
  LIBRARY
    COMPONENT webembed_Runtime
    NAMELINK_COMPONENT webembed_Development
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

include("${SERENITY_SOURCE_DIR}/Meta/Lagom/get_linked_lagom_libraries.cmake")
foreach (application IN LISTS webembed_applications)
  get_linked_lagom_libraries("${application}" "${application}_lagom_libraries")
  list(APPEND all_required_lagom_libraries "${${application}_lagom_libraries}")
endforeach()
list(REMOVE_DUPLICATES all_required_lagom_libraries)

install(TARGETS ${all_required_lagom_libraries}
  EXPORT webembedTargets
  COMPONENT webembed_Runtime
  LIBRARY
    COMPONENT webembed_Runtime
    NAMELINK_COMPONENT webembed_Development
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

write_basic_package_version_file(
    "${package}ConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion
)

# Allow package maintainers to freely override the path for the configs
set(
    webembed_INSTALL_CMAKEDIR "${CMAKE_INSTALL_DATADIR}/${package}"
    CACHE PATH "CMake package config location relative to the install prefix"
)
mark_as_advanced(webembed_INSTALL_CMAKEDIR)

install(
    FILES cmake/LadybirdInstallConfig.cmake
    DESTINATION "${webembed_INSTALL_CMAKEDIR}"
    RENAME "${package}Config.cmake"
    COMPONENT webembed_Development
)

install(
    FILES "${PROJECT_BINARY_DIR}/${package}ConfigVersion.cmake"
    DESTINATION "${webembed_INSTALL_CMAKEDIR}"
    COMPONENT webembed_Development
)

install(
    EXPORT webembedTargets
    NAMESPACE webembed::
    DESTINATION "${webembed_INSTALL_CMAKEDIR}"
    COMPONENT webembed_Development
)

install(DIRECTORY
    "${SERENITY_SOURCE_DIR}/Base/res/html"
    "${SERENITY_SOURCE_DIR}/Base/res/fonts"
    "${SERENITY_SOURCE_DIR}/Base/res/icons"
    "${SERENITY_SOURCE_DIR}/Base/res/themes"
    "${SERENITY_SOURCE_DIR}/Base/res/color-palettes"
    "${SERENITY_SOURCE_DIR}/Base/res/cursor-themes"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/res"
  USE_SOURCE_PERMISSIONS MESSAGE_NEVER
  COMPONENT webembed_Runtime
)

install(FILES
    "${SERENITY_SOURCE_DIR}/Base/home/anon/.config/BrowserAutoplayAllowlist.txt"
    "${SERENITY_SOURCE_DIR}/Base/home/anon/.config/BrowserContentFilters.txt"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/res/webembed"
  COMPONENT webembed_Runtime
)
