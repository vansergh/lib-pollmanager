# Filter values through regex
#   FilterRegex({INCLUDE | EXCLUDE} <regex> <listname> [items...])
#   Element will included into result list if
#     INCLUDE is specified and it matches with regex or
#     EXCLUDE is specified and it doesn't match with regex.
# Example:
#   FilterRegex(INCLUDE "(a|c)" LISTOUT a b c d) => a c
#   FilterRegex(EXCLUDE "(a|c)" LISTOUT a b c d) => b d
function(FilterRegex _action _regex _listname)
    # check an action
    if("${_action}" STREQUAL "INCLUDE")
        set(has_include TRUE)
    elseif("${_action}" STREQUAL "EXCLUDE")
        set(has_include FALSE)
    else()
        message(FATAL_ERROR "Incorrect value for ACTION: ${_action}")
    endif()

    set(${_listname})
    foreach(element ${ARGN})
        string(REGEX MATCH ${_regex} result ${element})
        if(result)
            if(has_include)
                list(APPEND ${_listname} ${element})
            endif()
        else()
            if(NOT has_include)
                list(APPEND ${_listname} ${element})
            endif()
        endif()
    endforeach()

    # put result in parent scope variable
    set(${_listname} ${${_listname}} PARENT_SCOPE)
endfunction()
