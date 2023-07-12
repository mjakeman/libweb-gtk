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
#        WebContentView.cpp
        ContentViewImpl.cpp

        Embed/webcontentview.cpp
        Embed/webembed.cpp
)

set(EMBED
        "Embed/webcontentview.h"
        "Embed/webembed.h"
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

# Loosely inspired by HarfBuzz's Build System
# Licensed under the "Old" MIT License
set(INTROSPECTION ON)
if (INTROSPECTION)
    find_package(PkgConfig)
    pkg_check_modules(GOBJECT_INTROSPECTION QUIET gobject-introspection-1.0)

    find_program(G_IR_SCANNER g-ir-scanner
            HINTS ${PC_g_ir_scanner}
    )

    find_program(G_IR_COMPILER g-ir-compiler
            HINTS ${PC_g_ir_compiler}
    )

    set(G_IR_SCANNER_CMD "${G_IR_SCANNER}")
    set(web_libpath "$<TARGET_FILE_DIR:webembed>")

    add_custom_command (
            TARGET webembed
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            POST_BUILD
            COMMAND ${G_IR_SCANNER_CMD}
            --warn-all --no-libtool --verbose
            --namespace=Web
            --nsversion=0.0
            --identifier-prefix=Web
            --symbol-prefix=web_
            --include GObject-2.0
            --include Gtk-4.0
            --pkg-export=webembed
            --library=webembed
            -L${web_libpath}
            ${GTK4_CFLAGS}
            ${EMBED}
            -o ${web_libpath}/Web-0.0.gir
            DEPENDS webembed
    )

    add_custom_command (
            TARGET webembed
            POST_BUILD
            COMMAND "${G_IR_COMPILER}"
            --verbose --debug
            --includedir ${CMAKE_CURRENT_BINARY_DIR}
            ${web_libpath}/Web-0.0.gir
            -o ${web_libpath}/Web-0.0.typelib
            DEPENDS ${web_libpath}/Web-0.0.gir webembed
    )
endif ()