set(SOURCES
        ${BROWSER_SOURCE_DIR}/CookieJar.cpp
        ${BROWSER_SOURCE_DIR}/Database.cpp
        ${BROWSER_SOURCE_DIR}/History.cpp
        #    BrowserWindow.cpp
        #    ConsoleWidget.cpp
        EventLoopImplementationGLib.cpp
        EventLoopImplementationGtk.cpp
        HelperProcess.cpp
        #    InspectorWidget.cpp
        #    LocationEdit.cpp
        #    ModelTranslator.cpp
        #    Settings.cpp
        #    SettingsDialog.cpp
        #    Tab.cpp
        Utilities.cpp
        WebContentView.cpp

        Embed/webcontentview.cpp
        Embed/webembed.cpp
)

add_library(webembed ${SOURCES})

set(DEPS ${GTK4_LIBRARIES} LibCore LibFileSystem LibGfx LibGUI LibIPC LibJS LibMain LibWeb LibWebView LibSQL LibWebSocket LibCrypto LibGemini LibHTTP LibTLS LibDiff)
target_link_libraries(webembed PRIVATE ${DEPS})

foreach(dir IN LISTS INCLUDE_DIRS)
    target_include_directories(webembed PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_include_directories(webembed PRIVATE ${SERENITY_SOURCE_DIR}/Userland/)
    target_include_directories(webembed PRIVATE ${SERENITY_SOURCE_DIR}/Userland/Applications/)
    target_include_directories(webembed PRIVATE ${SERENITY_SOURCE_DIR}/Userland/Services/)
endforeach()

target_include_directories(webembed PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/>
)

add_subdirectory(SQLServer)
add_subdirectory(WebContent)
add_subdirectory(WebDriver)

add_dependencies(webembed SQLServer WebContent WebDriver)