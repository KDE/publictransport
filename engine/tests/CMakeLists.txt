add_definitions (${QT_DEFINITIONS})

include_directories(
   ${CMAKE_CURRENT_SOURCE_DIR}/../
   ${CMAKE_CURRENT_BINARY_DIR}
   ${CMAKE_CURRENT_BINARY_DIR}/../ # For config.h
)

set( CMAKE_EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )

set( LocationsTest_SRCS LocationsTest.cpp )
add_executable( LocationsTest ${LocationsTest_SRCS} )
add_test( LocationsTest LocationsTest )
target_link_libraries( LocationsTest Qt5::Test KF5::Plasma )

set( ServiceProvidersTest_SRCS ServiceProvidersTest.cpp )
add_executable( ServiceProvidersTest ${ServiceProvidersTest_SRCS} )
add_test( ServiceProvidersTest ServiceProvidersTest )
target_link_libraries( ServiceProvidersTest Qt5::Test KF5::Plasma )

set( StopSuggestionsTest_SRCS StopSuggestionsTest.cpp )
add_executable( StopSuggestionsTest ${StopSuggestionsTest_SRCS} )
add_test( StopSuggestionsTest StopSuggestionsTest )
target_link_libraries( StopSuggestionsTest Qt5::Test KF5::Plasma )

qt5_wrap_cpp( engine_tests_MOC_SRCS ../enums.h )

set( DeparturesTest_SRCS DeparturesTest.cpp ${engine_tests_MOC_SRCS}  )
add_executable( DeparturesTest ${DeparturesTest_SRCS} )
add_test( DeparturesTest DeparturesTest )
target_link_libraries( DeparturesTest Qt5::Test KF5::Plasma )

set( ScriptApiTest_SRCS
    ScriptApiTest.cpp
   # Use files directly from the data engine
   ../global.cpp
   ../serviceproviderglobal.cpp
   ../departureinfo.cpp
   ../script/scriptapi.cpp
    ${engine_tests_MOC_SRCS} )
add_executable( ScriptApiTest ${ScriptApiTest_SRCS} )
add_test( ScriptApiTest ScriptApiTest )
target_link_libraries( ScriptApiTest Qt5::Test KF5::Plasma
        Qt5::Network Qt5::Script z )

qt5_wrap_cpp( GeneralTransitTest_MOC_SRCS
    ../serviceprovider.h
    ../serviceproviderdata.h
    ../enums.h
    ../script/script_thread.h
    ../script/serviceproviderscript.h
)
set( GeneralTransitTest_SRCS GeneralTransitTest.cpp
    ../gtfs/gtfsimporter.cpp
    ../gtfs/gtfsdatabase.cpp
    ../serviceproviderdata.cpp
    ../serviceproviderdatareader.cpp
    ../serviceproviderglobal.cpp
    ../serviceprovidertestdata.cpp
    ../enums.h
    ../departureinfo.cpp
    ../request.cpp
    ../global.cpp
    ../serviceprovider.cpp
    ../script/serviceproviderscript.cpp
    ../script/script_thread.cpp
    ../script/scriptapi.cpp
    ../script/scriptobjects.cpp

    ${GeneralTransitTest_MOC_SRCS}
)

add_executable( GeneralTransitTest ${GeneralTransitTest_SRCS} )
add_test( GeneralTransitTest GeneralTransitTest )
target_link_libraries( GeneralTransitTest Qt5::Test
        Qt5::Sql KF5::KIOCore KF5::ThreadWeaver Qt5::Network
        Qt5::Script Qt5::Xml z )

