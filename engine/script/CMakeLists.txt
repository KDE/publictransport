
# Sources for the script provider type
set ( script_SRCS
    script/serviceproviderscript.cpp
    script/script_thread.cpp
    script/scripting.cpp
    script/scriptobjects.cpp
)

# Add sources of this directory to the sources list of the parent directories CMakeLists.txt
set ( publictransport_engine_SRCS ${publictransport_engine_SRCS} ${script_SRCS} PARENT_SCOPE )

# Add scripted provider plugins
add_subdirectory( serviceProviders )
