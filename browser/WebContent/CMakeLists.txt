set(WEBCONTENT_SOURCE_DIR ${SERENITY_SOURCE_DIR}/Userland/Services/WebContent/)

set(WEBCONTENT_SOURCES
    ${WEBCONTENT_SOURCE_DIR}/ConnectionFromClient.cpp
    ${WEBCONTENT_SOURCE_DIR}/ConsoleGlobalEnvironmentExtensions.cpp
    ${WEBCONTENT_SOURCE_DIR}/PageHost.cpp
    ${WEBCONTENT_SOURCE_DIR}/WebContentConsoleClient.cpp
    ${WEBCONTENT_SOURCE_DIR}/WebDriverConnection.cpp
    ../AudioCodecPluginLadybird.cpp
    ../EventLoopImplementationGLib.cpp
        ../FontPluginGTK.cpp
    ../ImageCodecPluginLadybird.cpp
    #   ../RequestManagerSoup.cpp
    ../Utilities.cpp
    #../WebSocketClientManagerLadybird.cpp
    #../WebSocketLadybird.cpp
    #../WebSocketImplQt.cpp
    main.cpp
)

if (APPLE)
    list(APPEND WEBCONTENT_SOURCES MacOSSetup.mm)
    find_library(COCOA_LIBRARY Cocoa)
    if(NOT COCOA_LIBRARY)
        message(FATAL_ERROR "Cocoa framework not found")
    endif()
endif()

add_executable(WebContent ${WEBCONTENT_SOURCES})

target_include_directories(WebContent PRIVATE ${SERENITY_SOURCE_DIR}/Userland/Services/)
target_include_directories(WebContent PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/..)
target_link_libraries(WebContent PRIVATE ${GTK4_LIBRARIES} ${COCOA_LIBRARY} LibAudio LibCore LibFileSystem LibGfx LibIPC LibJS LibMain LibWeb LibWebSocket)
