set(SOURCES
        main.cpp
)

add_executable(demo-browser ${SOURCES})

add_dependencies(demo-browser webembed SQLServer WebContent WebDriver)

target_include_directories(demo-browser PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(demo-browser PRIVATE ${SERENITY_SOURCE_DIR}/Userland/)
target_include_directories(demo-browser PRIVATE ${SERENITY_SOURCE_DIR}/Userland/Applications/)
target_include_directories(demo-browser PRIVATE ${SERENITY_SOURCE_DIR}/Userland/Services/)

target_link_libraries(demo-browser PRIVATE ${GTK4_LIBRARIES} webembed LibWeb LibWebView LibWebSocket LibCrypto LibFileSystem LibGemini LibHTTP LibJS LibGfx LibMain LibTLS LibIPC LibJS LibDiff LibSQL)

set_target_properties(demo-browser PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER com.mattjakeman.LibWebGTK
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
        MACOSX_BUNDLE TRUE
        WIN32_EXECUTABLE TRUE
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER com.mattjakeman.LibWebGTK
)

if (APPLE)
    # FIXME: Create a proper app bundle for each helper process
    set(app_dir "$<TARGET_FILE_DIR:demo-browser>")
    set(bundle_dir "$<TARGET_BUNDLE_DIR:demo-browser>")
    add_custom_command(TARGET demo-browser POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:SQLServer>" "${app_dir}"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:WebContent>" "${app_dir}"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:WebDriver>" "${app_dir}"
    )
endif()