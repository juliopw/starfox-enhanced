function(starfox_add_switch_nro target)
    set(nacp "${CMAKE_CURRENT_BINARY_DIR}/starfox-enhanced.nacp")
    set(nro "${CMAKE_CURRENT_BINARY_DIR}/starfox-enhanced.nro")
    nx_generate_nacp(
        OUTPUT "${nacp}"
        NAME "Star Fox Enhanced"
        AUTHOR "Star Fox Enhanced contributors"
        VERSION "${PROJECT_VERSION}")
    nx_create_nro(starfox_switch
        TARGET "${target}"
        OUTPUT "${nro}"
        NACP "${nacp}")

    set(package_dir
        "${CMAKE_CURRENT_BINARY_DIR}/sdcard/switch/starfox-enhanced")
    set(package_outputs "${package_dir}/starfox-enhanced.nro")
    set(package_dependencies starfox_switch "${nro}")
    set(package_commands
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${package_dir}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${package_dir}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${nro}" "${package_dir}/starfox-enhanced.nro")

    if(NOT STARFOX_RUNTIME_ASSETS_EMBEDDED
       AND EXISTS "${STARFOX_ROM_FILE}"
       AND EXISTS "${STARFOX_SYMBOLS_FILE}")
        list(APPEND package_outputs
            "${package_dir}/SF.SFC"
            "${package_dir}/SYMBOLS.TXT")
        list(APPEND package_dependencies
            "${STARFOX_ROM_FILE}"
            "${STARFOX_SYMBOLS_FILE}")
        list(APPEND package_commands
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${STARFOX_ROM_FILE}" "${package_dir}/SF.SFC"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${STARFOX_SYMBOLS_FILE}" "${package_dir}/SYMBOLS.TXT")
        if(EXISTS "${STARFOX_EX_ROM_FILE}"
           AND EXISTS "${STARFOX_EX_SYMBOLS_FILE}")
            list(APPEND package_outputs
                "${package_dir}/SFES.SFC"
                "${package_dir}/SFES-SYMBOLS.TXT")
            list(APPEND package_dependencies
                "${STARFOX_EX_ROM_FILE}"
                "${STARFOX_EX_SYMBOLS_FILE}")
            list(APPEND package_commands
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${STARFOX_EX_ROM_FILE}" "${package_dir}/SFES.SFC"
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${STARFOX_EX_SYMBOLS_FILE}"
                    "${package_dir}/SFES-SYMBOLS.TXT")
        endif()
    endif()

    # The runtime loads the soundtrack companion from beside the NRO, so the
    # SD-card payload needs its own copy. It is always external, independent
    # of whether the ROM and symbols were embedded above.
    if(STARFOX_PACKAGE_MSU1_MUSIC AND STARFOX_MSU1_PACK)
        list(APPEND package_outputs "${package_dir}/Starfox-MSU1.PAK")
        list(APPEND package_dependencies starfox_msu1_pack "${STARFOX_MSU1_PACK}")
        list(APPEND package_commands
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${STARFOX_MSU1_PACK}" "${package_dir}/Starfox-MSU1.PAK")
    endif()

    add_custom_target(starfox_switch_package ALL
        ${package_commands}
        DEPENDS ${package_dependencies}
        BYPRODUCTS ${package_outputs}
        VERBATIM)
endfunction()
