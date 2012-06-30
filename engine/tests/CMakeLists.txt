
find_package( KDE4 REQUIRED )
include( KDE4Defaults )
add_definitions (${QT_DEFINITIONS} ${KDE4_DEFINITIONS})

include_directories(
   ${QT_INCLUDES}
   ${KDE4_INCLUDES}
   ${CMAKE_CURRENT_SOURCE_DIR}/../
   ${CMAKE_CURRENT_BINARY_DIR}
   ${CMAKE_CURRENT_BINARY_DIR}/../ # For config.h
)

set( CMAKE_EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )

set( LocationsTest_SRCS LocationsTest.cpp )
qt4_automoc( ${LocationsTest_SRCS} )
add_executable( LocationsTest ${LocationsTest_SRCS} )
add_test( LocationsTest LocationsTest )
target_link_libraries( LocationsTest ${QT_QTTEST_LIBRARY} ${KDE4_PLASMA_LIBS} )

set( ServiceProvidersTest_SRCS ServiceProvidersTest.cpp )
qt4_automoc( ${ServiceProvidersTest_SRCS} )
add_executable( ServiceProvidersTest ${ServiceProvidersTest_SRCS} )
add_test( ServiceProvidersTest ServiceProvidersTest )
target_link_libraries( ServiceProvidersTest ${QT_QTTEST_LIBRARY} ${KDE4_PLASMA_LIBS} )

set( StopSuggestionsTest_SRCS StopSuggestionsTest.cpp )
qt4_automoc( ${StopSuggestionsTest_SRCS} )
add_executable( StopSuggestionsTest ${StopSuggestionsTest_SRCS} )
add_test( StopSuggestionsTest StopSuggestionsTest )
target_link_libraries( StopSuggestionsTest ${QT_QTTEST_LIBRARY} ${KDE4_PLASMA_LIBS} )

set( DeparturesTest_SRCS DeparturesTest.cpp )
qt4_automoc( ${DeparturesTest_SRCS} )
add_executable( DeparturesTest ${DeparturesTest_SRCS} )
add_test( DeparturesTest DeparturesTest )
target_link_libraries( DeparturesTest ${QT_QTTEST_LIBRARY} ${KDE4_PLASMA_LIBS} )

set( ScriptApiTest_SRCS
    ScriptApiTest.cpp
   # Use files directly from the data engine
   ../global.cpp
   ../serviceproviderglobal.cpp
   ../departureinfo.cpp
   ../script/scripting.cpp )
qt4_automoc( ${ScriptApiTest_SRCS} )
add_executable( ScriptApiTest ${ScriptApiTest_SRCS} )
add_test( ScriptApiTest ScriptApiTest )
target_link_libraries( ScriptApiTest ${QT_QTTEST_LIBRARY} ${KDE4_PLASMA_LIBS}
        ${QT_QTNETWORK_LIBRARY} ${QT_QTSCRIPT_LIBRARY} )

set( GeneralTransitTest_SRCS GeneralTransitTest.cpp
    ../gtfs/generaltransitfeed_importer.cpp
    ../gtfs/generaltransitfeed_database.cpp
)
qt4_automoc( ${GeneralTransitTest_SRCS} )
add_executable( GeneralTransitTest ${GeneralTransitTest_SRCS} )
add_test( GeneralTransitTest GeneralTransitTest )
target_link_libraries( GeneralTransitTest ${QT_QTTEST_LIBRARY} ${KDE4_CORE_LIBS} ${KDE4_KUTILS_LIBS}
                                          ${QT_QTSQL_LIBRARY} )
