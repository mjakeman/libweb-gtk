set(SOURCES
        ${BROWSER_SOURCE_DIR}/CookieJar.cpp
        ${BROWSER_SOURCE_DIR}/Database.cpp
        ${BROWSER_SOURCE_DIR}/History.cpp
        #    BrowserWindow.cpp
        #    ConsoleWidget.cpp
        EventLoopImplementationGLib.cpp
        HelperProcess.cpp
        #    InspectorWidget.cpp
        #    LocationEdit.cpp
        #    ModelTranslator.cpp
        #    Settings.cpp
        #    SettingsDialog.cpp
        #    Tab.cpp
        Utilities.cpp
        WebContentView.cpp
        #    ladybird.qrc
        main.cpp
)

add_executable(ladybird ${SOURCES})
target_link_libraries(ladybird PRIVATE ${GTK4_LIBRARIES} LibCore LibFileSystem LibGfx LibGUI LibIPC LibJS LibMain LibWeb LibWebView LibSQL)

target_include_directories(ladybird PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(ladybird PRIVATE ${SERENITY_SOURCE_DIR}/Userland/)
target_include_directories(ladybird PRIVATE ${SERENITY_SOURCE_DIR}/Userland/Applications/)
target_include_directories(ladybird PRIVATE ${SERENITY_SOURCE_DIR}/Userland/Services/)

#add_executable(headless-browser
#        HeadlessBrowser.cpp
#        #${SERENITY_SOURCE_DIR}/Userland/Utilities/headless-browser.cpp
#        #${SERENITY_SOURCE_DIR}/Userland/Services/WebContent/WebDriverConnection.cpp
#        #HelperProcess.cpp
#        #Utilities.cpp)
#)

#target_include_directories(headless-browser PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(ladybird PRIVATE ${GTK4_LIBRARIES} LibWeb LibWebView LibWebSocket LibCrypto LibFileSystem LibGemini LibHTTP LibJS LibGfx LibMain LibTLS LibIPC LibJS LibDiff)

set_target_properties(ladybird PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER org.SerenityOS.Ladybird
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
        MACOSX_BUNDLE TRUE
        WIN32_EXECUTABLE TRUE
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER org.SerenityOS.Ladybird
)

add_custom_target(run${LADYBIRD_CUSTOM_TARGET_SUFFIX}
        COMMAND "${CMAKE_COMMAND}" -E env "SERENITY_SOURCE_DIR=${SERENITY_SOURCE_DIR}" "$<TARGET_FILE:ladybird>" $ENV{LAGOM_ARGS}
        USES_TERMINAL
        VERBATIM
)

add_custom_target(debug${LADYBIRD_CUSTOM_TARGET_SUFFIX}
        COMMAND "${CMAKE_COMMAND}" -E env "SERENITY_SOURCE_DIR=${SERENITY_SOURCE_DIR}" gdb -ex "set follow-fork-mode child" "$<TARGET_FILE:ladybird>"
        USES_TERMINAL
)

add_custom_target(debug-lldb${LADYBIRD_CUSTOM_TARGET_SUFFIX}
        COMMAND "${CMAKE_COMMAND}" -E env "SERENITY_SOURCE_DIR=${SERENITY_SOURCE_DIR}" lldb "$<TARGET_FILE:ladybird>"
        USES_TERMINAL
)

#qt_finalize_executable(ladybird)

add_subdirectory(SQLServer)
add_subdirectory(WebContent)
add_subdirectory(WebDriver)
#add_dependencies(ladybird SQLServer WebContent WebDriver headless-browser)
add_dependencies(ladybird SQLServer WebContent WebDriver)

if (APPLE)
    # FIXME: Create a proper app bundle for each helper process
    set(app_dir "$<TARGET_FILE_DIR:ladybird>")
    set(bundle_dir "$<TARGET_BUNDLE_DIR:ladybird>")
    add_custom_command(TARGET ladybird POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:SQLServer>" "${app_dir}"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:WebContent>" "${app_dir}"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:WebDriver>" "${app_dir}"
            #        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:headless-browser>" "${app_dir}"
    )
endif()