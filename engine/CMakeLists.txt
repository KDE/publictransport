project(plasma-engine-publictransport)

# Find the required Libaries
find_package(KDE4 REQUIRED)
include(KDE4Defaults)

add_definitions (${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
include_directories(
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_BINARY_DIR}
   ${KDE4_INCLUDES}
   )

# We add our source code here
set(publictransport_engine_SRCS publictransportdataengine.cpp
    timetableaccessor.cpp timetableaccessor_efa.cpp timetableaccessor_fahrplaner.cpp timetableaccessor_rmv.cpp timetableaccessor_vvs.cpp timetableaccessor_vrn.cpp timetableaccessor_bvg.cpp timetableaccessor_dvb.cpp timetableaccessor_nasa.cpp timetableaccessor_imhd.cpp)

# Now make sure all files get to the right place
kde4_add_plugin(plasma_engine_publictransport ${publictransport_engine_SRCS})
target_link_libraries(plasma_engine_publictransport
                      ${KDE4_KDECORE_LIBS} ${KDE4_KPARTS_LIBS} 
                      ${KDE4_PLASMA_LIBS})

install(TARGETS plasma_engine_publictransport
        DESTINATION ${PLUGIN_INSTALL_DIR})

install(FILES plasma-engine-publictransport.desktop
        DESTINATION ${SERVICES_INSTALL_DIR})
	