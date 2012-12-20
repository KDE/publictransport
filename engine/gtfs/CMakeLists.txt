
# Sources for the GTFS provider type
set ( gtfs_SRCS
    gtfs/serviceprovidergtfs.cpp
    gtfs/gtfsimporter.cpp
    gtfs/gtfsdatabase.cpp
    gtfs/gtfsservice.cpp
)

# Add GTFS-realtime sources if enabled
if ( BUILD_GTFS_REALTIME )
    # Add GTFS-realtime sources
    message( "     - Build GTFS-realtime support" )
    list ( APPEND gtfs_SRCS gtfs/gtfsrealtime.cpp  )
endif ( BUILD_GTFS_REALTIME )

# Add sources of this directory to the sources list of the parent directories CMakeLists.txt
set ( publictransport_engine_SRCS ${publictransport_engine_SRCS} ${gtfs_SRCS} PARENT_SCOPE )

# Install .operations-file describing the GTFS service
install( FILES publictransport.operations
         DESTINATION ${DATA_INSTALL_DIR}/plasma/services )
