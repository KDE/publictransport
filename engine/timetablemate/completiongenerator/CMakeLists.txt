
project(completiongenerator)
# Find the required Libaries
find_package( KDE4 REQUIRED )
include( KDE4Defaults )
add_definitions( ${QT_DEFINITIONS} ${KDE4_DEFINITIONS} )
include_directories(
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_BINARY_DIR}
   ${KDE4_INCLUDES} ../../
   ${CMAKE_CURRENT_BINARY_DIR}/../../ # For config.h
)

qt4_wrap_cpp( completiongenerator_MOC_SRCS ../../enums.h )
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
   ../../script/scripting.cpp
   ${completiongenerator_MOC_SRCS}
)

kde4_add_executable( completiongenerator
                     ${completiongenerator_SRCS} )

target_link_libraries( completiongenerator ${KDE4_KDECORE_LIBS}
        ${QT_QTNETWORK_LIBRARY} ${QT_QTSCRIPT_LIBRARY} )
