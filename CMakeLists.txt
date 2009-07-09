# This plugin depends on libf2f:
ADD_LIBRARY(    greynet SHARED
                src/greynet.cpp
		        src/ss_greynet.cpp
                src/jbot.cpp
           )
#
# Ensure the shared library file is called <CLASS NAME>.resolver
# Playdar looks for a class with the same name as the file, minus suffix
#
SET_TARGET_PROPERTIES(  greynet PROPERTIES
                        PREFIX ""
                        SUFFIX ".resolver"
                        CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PLAYDAR_DIR}/plugins"
                     )

TARGET_LINK_LIBRARIES(  greynet
                        /usr/local/lib/libgloox.a
                        /usr/local/lib/libf2f.a
                        /usr/local/lib/libportfwd.a
                        ${Boost_LIBRARIES}
                        ${PLAYDAR_PLUGIN_LDFLAGS}
                     )
