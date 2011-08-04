
find_package( KDE4 REQUIRED )
include( KDE4Defaults )

include_directories( ${QT_INCLUDES} ${KDE4_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR} )
set( CMAKE_EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )

set( PublicTransportAppletTest_SRCS PublicTransportAppletTest.cpp )
qt4_automoc( ${PublicTransportAppletTest_SRCS} )
add_executable( PublicTransportAppletTest ${PublicTransportAppletTest_SRCS} )
add_test( PublicTransportAppletTest PublicTransportAppletTest )
target_link_libraries( PublicTransportAppletTest
  ${KDE4_KDECORE_LIBS} ${QT_QTTEST_LIBRARY} ${KDE4_KDEUI_LIBS} ${KDE4_PLASMA_LIBS} publictransporthelper
)

set( JourneySearchParserTest_SRCS JourneySearchParserTest.cpp ../journeysearchparser.cpp )
qt4_automoc( ${JourneySearchParserTest_SRCS} )
add_executable( JourneySearchParserTest ${JourneySearchParserTest_SRCS} )
add_test( JourneySearchParserTest JourneySearchParserTest )
target_link_libraries( JourneySearchParserTest
  ${KDE4_KDECORE_LIBS} ${QT_QTTEST_LIBRARY} ${KDE4_KDEUI_LIBS}
)
