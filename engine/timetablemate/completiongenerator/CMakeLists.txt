
project(completiongenerator)
# Find the required Libaries
add_definitions( ${QT_DEFINITIONS} )
include_directories(
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_BINARY_DIR}
    ../../
   ${CMAKE_CURRENT_BINARY_DIR}/../../ # For config.h
)

qt5_wrap_cpp( completiongenerator_MOC_SRCS ../../enums.h )
set( completiongenerator_SRCS
   main.cpp
   global_generator.cpp
   documentationparser.cpp
   outputgenerator.cpp
   parser_structures.cpp

   # Use files directly from the data engine
   ../../global.cpp
   ../../serviceproviderglobal.cpp
   ../../departureinfo.cpp
   ../../script/scriptapi.cpp
   ${completiongenerator_MOC_SRCS}
)

add_executable( completiongenerator
                     ${completiongenerator_SRCS} )

target_link_libraries( completiongenerator KF5::KDELibs4Support
        Qt5::Network Qt5::Script z )
