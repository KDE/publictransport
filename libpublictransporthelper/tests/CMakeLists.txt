
find_package( KDE4 REQUIRED )
include( KDE4Defaults )

include_directories( ${QT_INCLUDES} ${KDE4_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR} )
set( CMAKE_EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )

set( PublicTransportHelperTest_SRCS PublicTransportHelperTest.cpp )
qt4_automoc( ${PublicTransportHelperTest_SRCS} )
add_executable( PublicTransportHelperTest ${PublicTransportHelperTest_SRCS} )
add_test( PublicTransportHelperTest PublicTransportHelperTest )
target_link_libraries( PublicTransportHelperTest
	${KDE4_KDECORE_LIBS} ${QT_QTTEST_LIBRARY} ${KDE4_KDEUI_LIBS} ${KDE4_PLASMA_LIBS} publictransporthelper
)

set( PublicTransportHelperGuiTest_SRCS PublicTransportHelperGuiTest.cpp )
qt4_automoc( ${PublicTransportHelperGuiTest_SRCS} )
add_executable( PublicTransportHelperGuiTest ${PublicTransportHelperGuiTest_SRCS} )
add_test( PublicTransportHelperGuiTest PublicTransportHelperGuiTest )
target_link_libraries( PublicTransportHelperGuiTest
  ${KDE4_KDECORE_LIBS} ${QT_QTTEST_LIBRARY} ${KDE4_KDEUI_LIBS} publictransporthelper
)
