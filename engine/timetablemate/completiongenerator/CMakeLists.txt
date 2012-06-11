
project(completiongenerator)
# Find the required Libaries
find_package( KDE4 REQUIRED )
include( KDE4Defaults )
add_definitions( ${QT_DEFINITIONS} ${KDE4_DEFINITIONS} )
include_directories(
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_BINARY_DIR}
   ${KDE4_INCLUDES} ../../engine )

set( completiongenerator_SRCS
   main.cpp
   global_generator.cpp
   documentationparser.cpp
   outputgenerator.cpp
   parser_structures.cpp

   # Use files directly from the data engine
   ../../global.cpp
   ../../scripting.cpp
   ../../serviceproviderglobal.cpp
   ../../departureinfo.cpp
)

kde4_add_executable( completiongenerator
                     ${completiongenerator_SRCS} )

target_link_libraries( completiongenerator ${KDE4_KDECORE_LIBS}
        ${QT_LIBRARIES} ${QT_QTNETWORK_LIBRARY} ${QT_QTCORE_LIBRARY}
        ${QT_QTSCRIPT_LIBRARY} ${KDE4_THREADWEAVER_LIBS} )
