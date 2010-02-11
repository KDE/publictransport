project( plasma-engine-openstreetmap )
 
# Find the required Libaries
find_package( KDE4 REQUIRED )
include( KDE4Defaults )
 
add_definitions( ${QT_DEFINITIONS} ${KDE4_DEFINITIONS} )
include_directories(
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_BINARY_DIR}
   ${KDE4_INCLUDES} )
 
# We add our source code here
set( openstreetmapdataengine_SRCS openstreetmapdataengine.cpp )
 
# Now make sure all files get to the right place
kde4_add_plugin( plasma_engine_openstreetmap ${openstreetmapdataengine_SRCS} )
target_link_libraries( plasma_engine_openstreetmap 
                       ${KDE4_PLASMA_LIBS} ${KDE4_KDEUI_LIBS} kio )
 
install( TARGETS plasma_engine_openstreetmap
         DESTINATION ${PLUGIN_INSTALL_DIR} )
 
install( FILES plasma-engine-openstreetmap.desktop
         DESTINATION ${SERVICES_INSTALL_DIR} )
