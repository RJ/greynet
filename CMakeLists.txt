# This plugin depends on libf2f:
#SET(F2F_LIB_DIR "/home/rj/src/libf2f/lib")
#SET(F2F_INC_DIR "/home/rj/src/libf2f/include")
#LINK_DIRECTORIES(    ${F2F_LIB_DIR} )
#INCLUDE_DIRECTORIES( ${F2F_INC_DIR} )


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
                        f2f
                        gloox
                        ${Boost_LIBRARIES}
                        ${PLAYDAR_PLUGIN_LDFLAGS}
                     )
