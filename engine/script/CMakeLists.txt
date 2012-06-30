
# Add sources of this directory to the sources list of the parent directories CMakeLists.txt
set( publictransport_engine_SRCS
    ${publictransport_engine_SRCS}

    script/serviceproviderscript.cpp
    script/script_thread.cpp
    script/scripting.cpp

    PARENT_SCOPE
)

add_subdirectory( serviceProviders )
