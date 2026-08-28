if(NOT DEFINED PACK_DIR)
    message(FATAL_ERROR "validate_asm_relocs.cmake requires -DPACK_DIR=<dir>")
endif()

file(GLOB_RECURSE VAR_ASM_FILES "${PACK_DIR}/*.asm")
list(LENGTH VAR_ASM_FILES VAR_FILE_COUNT)
if(VAR_FILE_COUNT EQUAL 0)
    message(FATAL_ERROR "No .asm files found under PACK_DIR='${PACK_DIR}' -- the check would pass vacuously")
endif()

set(VAR_PROBLEMS "")

foreach(VAR_FILE ${VAR_ASM_FILES})
    file(READ "${VAR_FILE}" VAR_CONTENT)
    string(REGEX REPLACE "\r" "" VAR_CONTENT "${VAR_CONTENT}")
    string(REGEX REPLACE ";[^\n]*" "" VAR_CONTENT "${VAR_CONTENT}")
    string(REPLACE "\n" ";" VAR_LINES "${VAR_CONTENT}")

    get_filename_component(VAR_NAME "${VAR_FILE}" NAME)
    set(VAR_PREV "")
    foreach(VAR_LINE ${VAR_LINES})
        string(STRIP "${VAR_LINE}" VAR_T)

        # addi reads rA=r0 as literal zero, so this never means "r0 + imm"
        if(VAR_T MATCHES "^addi[ \t]+r0,[ \t]*r0,")
            list(APPEND VAR_PROBLEMS
                "${VAR_NAME}: '${VAR_T}' -- addi reads rA=r0 as literal zero, so this discards whatever was in r0 (including a preceding lis). Use lis @h + ori for r0, or build in a scratch register and mr r0, rN.")
        elseif(VAR_T MATCHES "^(addi|ori)[ \t]+(r[0-9]+),[ \t]*(r[0-9]+),[ \t]*([^ \t]+)@(l|lo)$")
            set(VAR_OP "${CMAKE_MATCH_1}")
            set(VAR_RD "${CMAKE_MATCH_2}")
            set(VAR_RA "${CMAKE_MATCH_3}")
            set(VAR_SYM "${CMAKE_MATCH_4}")

            if(VAR_PREV MATCHES "^lis[ \t]+(r[0-9]+),[ \t]*([^ \t]+)@(ha|hi|h)$")
                set(VAR_LIS_RD "${CMAKE_MATCH_1}")
                set(VAR_LIS_SYM "${CMAKE_MATCH_2}")
                set(VAR_HIGH "${CMAKE_MATCH_3}")

                # only a genuine 2-instruction materialization: same register, same symbol
                if(VAR_LIS_RD STREQUAL VAR_RD AND VAR_LIS_RD STREQUAL VAR_RA AND VAR_LIS_SYM STREQUAL VAR_SYM)
                    if(VAR_OP STREQUAL "addi" AND NOT VAR_HIGH STREQUAL "ha")
                        list(APPEND VAR_PROBLEMS
                            "${VAR_NAME}: '${VAR_SYM}' pairs lis @${VAR_HIGH} with addi -- addi sign-extends the low half, so this computes the symbol MINUS 0x10000 whenever its low half is >= 0x8000. Use @ha with addi.")
                    elseif(VAR_OP STREQUAL "ori" AND VAR_HIGH STREQUAL "ha")
                        list(APPEND VAR_PROBLEMS
                            "${VAR_NAME}: '${VAR_SYM}' pairs lis @ha with ori -- ori zero-extends the low half, so the @ha correction is never cancelled and this computes the symbol PLUS 0x10000 whenever its low half is >= 0x8000. Use @h with ori.")
                    endif()
                endif()
            endif()
        endif()

        set(VAR_PREV "${VAR_T}")
    endforeach()
endforeach()

list(LENGTH VAR_PROBLEMS VAR_COUNT)
if(VAR_COUNT GREATER 0)
    string(REPLACE ";" "\n  " VAR_JOINED "${VAR_PROBLEMS}")
    message(FATAL_ERROR "PPC address-materialization check failed (${VAR_COUNT} problem(s)):\n  ${VAR_JOINED}\n")
endif()

message(STATUS "PPC address-materialization check: ${VAR_FILE_COUNT} asm file(s) clean")
