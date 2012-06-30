
set ( gtfs_SRCS
    gtfs/generaltransitfeed_importer.cpp
    gtfs/generaltransitfeed_database.cpp
    gtfs/serviceprovidergtfs.cpp
    gtfs/gtfsservice.cpp
)

if ( BUILD_GTFS_REALTIME )
    set ( gtfs_SRCS ${gtfs_SRCS}
        gtfs/generaltransitfeed_realtime.cpp
    )
endif ( BUILD_GTFS_REALTIME )

# Add sources of this directory to the sources list of the parent directories CMakeLists.txt
set( publictransport_engine_SRCS ${publictransport_engine_SRCS} ${gtfs_SRCS} PARENT_SCOPE )

add_subdirectory( serviceProviders )
