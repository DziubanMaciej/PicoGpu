macro (append_sources LIST_NAME DIRECTORY SET_IN_PARENT_SCOPE)
    file(GLOB_RECURSE SOURCES_IN_DIRECTORY
        ${DIRECTORY}/*.h
        ${DIRECTORY}/*.cpp
    )
    list(APPEND ${LIST_NAME} ${SOURCES_IN_DIRECTORY})
    if (${SET_IN_PARENT_SCOPE})
        set(${LIST_NAME} "${${LIST_NAME}}" PARENT_SCOPE)
    endif()
endmacro()
