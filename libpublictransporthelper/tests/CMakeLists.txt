include_directories(   ${CMAKE_CURRENT_BINARY_DIR} )
set( CMAKE_EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )

set( PublicTransportHelperTest_SRCS PublicTransportHelperTest.cpp )
qt4_automoc( ${PublicTransportHelperTest_SRCS} )
add_executable( PublicTransportHelperTest ${PublicTransportHelperTest_SRCS} )
add_test( PublicTransportHelperTest PublicTransportHelperTest )
target_link_libraries( PublicTransportHelperTest
	KF5::KDELibs4Support Qt5::Test  KF5::Plasma publictransporthelper
)

set( PublicTransportHelperGuiTest_SRCS PublicTransportHelperGuiTest.cpp )
qt4_automoc( ${PublicTransportHelperGuiTest_SRCS} )
add_executable( PublicTransportHelperGuiTest ${PublicTransportHelperGuiTest_SRCS} )
add_test( PublicTransportHelperGuiTest PublicTransportHelperGuiTest )
target_link_libraries( PublicTransportHelperGuiTest
  KF5::KDELibs4Support Qt5::Test KF5::PlasmaQuick publictransporthelper
)
