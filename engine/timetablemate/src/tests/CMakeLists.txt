
find_package( KDE4 REQUIRED )
include( KDE4Defaults )
add_definitions (${QT_DEFINITIONS} ${KDE4_DEFINITIONS})

include_directories(
   ${QT_INCLUDES}
   ${KDE4_INCLUDES}
   ${CMAKE_CURRENT_SOURCE_DIR}/../
   ${CMAKE_CURRENT_BINARY_DIR}
   ${CMAKE_CURRENT_BINARY_DIR}/../ # For javascriptcompletiongeneric.h
)

set( JavaScriptParserTest_SRCS
    JavaScriptParserTest.cpp
   # Use files directly from the data engine
   ../javascriptparser.cpp
   ../javascriptcompletiongeneric.cpp
)
qt4_automoc( ${JavaScriptParserTest_SRCS} )
add_executable( JavaScriptParserTest ${JavaScriptParserTest_SRCS} )
add_test( JavaScriptParserTest JavaScriptParserTest )
target_link_libraries( JavaScriptParserTest
    ${QT_QTTEST_LIBRARY} ${QT_QTGUI_LIBRARY} ${KDE4_KDECORE_LIBS} )
