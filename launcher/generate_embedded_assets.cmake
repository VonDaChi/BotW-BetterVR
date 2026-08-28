if(NOT DEFINED OUTPUT_HEADER OR NOT DEFINED OUTPUT_SOURCE OR NOT DEFINED OUTPUT_RC OR NOT DEFINED INPUT_DLL OR NOT DEFINED INPUT_JSON OR NOT DEFINED GRAPHIC_PACK_DIR OR NOT DEFINED BOTW_GRAPHICS_DIR OR NOT DEFINED VERSION)
    message(FATAL_ERROR "Missing required generator arguments")
endif()

get_filename_component(output_dir "${OUTPUT_HEADER}" DIRECTORY)
file(MAKE_DIRECTORY "${output_dir}")

function(write_if_different path contents)
    if(EXISTS "${path}")
        file(READ "${path}" existing_contents)
        if(existing_contents STREQUAL contents)
            return()
        endif()
    endif()
    file(WRITE "${path}" "${contents}")
endfunction()

get_filename_component(header_name "${OUTPUT_HEADER}" NAME)

set(header_contents [=[#pragma once

#include <cstddef>

namespace EmbeddedAssets {
    enum class AssetKind {
        Runtime,
        GraphicPack,
        DownloadedGraphics
    };

    struct Asset {
        const char* relativePath;
        AssetKind kind;
        int resourceId;
    };

    extern const char* const Version;
    extern const Asset Assets[];
    extern const size_t AssetCount;
}
]=])

set(source_contents "#include \"${header_name}\"\n\nnamespace EmbeddedAssets {\n    const char* const Version = \"${VERSION}\";\n\n    const Asset Assets[] = {\n")
set(rc_contents "")
set(asset_index 1000)

function(append_asset input_path relative_path kind)
    if(NOT EXISTS "${input_path}")
        message(FATAL_ERROR "Embedded asset input not found: ${input_path}")
    endif()

    string(REPLACE "\\" "/" relative_path_normalized "${relative_path}")
    string(REPLACE "/" "\\\\" rc_input_path "${input_path}")

    string(APPEND source_contents "        { \"${relative_path_normalized}\", AssetKind::${kind}, ${asset_index} },\n")
    string(APPEND rc_contents "${asset_index} RCDATA \"${rc_input_path}\"\n")

    math(EXPR next_asset_index "${asset_index} + 1")
    set(asset_index ${next_asset_index} PARENT_SCOPE)
    set(source_contents "${source_contents}" PARENT_SCOPE)
    set(rc_contents "${rc_contents}" PARENT_SCOPE)
endfunction()

append_asset("${INPUT_DLL}" "BetterVR_Layer.dll" "Runtime")
append_asset("${INPUT_JSON}" "BetterVR_Layer.json" "Runtime")

file(GLOB_RECURSE graphic_pack_files RELATIVE "${GRAPHIC_PACK_DIR}" "${GRAPHIC_PACK_DIR}/*")
list(SORT graphic_pack_files)

list(FILTER graphic_pack_files EXCLUDE REGEX "(^|/)CemuPatchCompiler-release\\.exe$")

if(EXCLUDE_DEBUG_PATCHES)
    list(FILTER graphic_pack_files EXCLUDE REGEX "(^|/)patch_debug_[^/]+\\.asm$")
endif()

foreach(graphic_pack_file IN LISTS graphic_pack_files)
    set(full_path "${GRAPHIC_PACK_DIR}/${graphic_pack_file}")
    if(NOT IS_DIRECTORY "${full_path}")
        append_asset("${full_path}" "BreathOfTheWild_BetterVR/${graphic_pack_file}" "GraphicPack")
    endif()
endforeach()

file(GLOB_RECURSE botw_graphics_files RELATIVE "${BOTW_GRAPHICS_DIR}" "${BOTW_GRAPHICS_DIR}/*")
list(SORT botw_graphics_files)

foreach(botw_graphics_file IN LISTS botw_graphics_files)
    set(full_path "${BOTW_GRAPHICS_DIR}/${botw_graphics_file}")
    if(NOT IS_DIRECTORY "${full_path}")
        append_asset("${full_path}" "BreathOfTheWild_Graphics/${botw_graphics_file}" "DownloadedGraphics")
    endif()
endforeach()

string(APPEND source_contents "    };\n\n    const size_t AssetCount = sizeof(Assets) / sizeof(Assets[0]);\n}\n")

write_if_different("${OUTPUT_HEADER}" "${header_contents}")
write_if_different("${OUTPUT_SOURCE}" "${source_contents}")
write_if_different("${OUTPUT_RC}" "${rc_contents}")
