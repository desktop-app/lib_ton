# This file is part of Desktop App Toolkit,
# a set of libraries for developing nice desktop applications.
#
# For license and copyright information please follow this link:
# https://github.com/desktop-app/legal/blob/master/LEGAL

function(generate_ton_tl target_name script scheme_file)
    find_package(Python REQUIRED)

    set(gen_dst ${CMAKE_CURRENT_BINARY_DIR}/gen)
    file(MAKE_DIRECTORY ${gen_dst})

    set(gen_timestamp ${gen_dst}/ton_tl.timestamp)
    set(gen_files
        ${gen_dst}/ton_tl.cpp
        ${gen_dst}/ton_tl.h
        ${gen_dst}/ton_tl-conversion.cpp
        ${gen_dst}/ton_tl-conversion.h
    )

    add_custom_command(
    OUTPUT
        ${gen_timestamp}
    BYPRODUCTS
        ${gen_files}
    COMMAND
        ${Python_EXECUTABLE}
        ${script}
        -o${gen_dst}/ton_tl
        ${scheme_file}
    COMMENT "Generating scheme (${target_name})"
    DEPENDS
        ${script}
        ${submodules_loc}/lib_tl/tl/generate_tl.py
        ${scheme_file}
    )
    generate_target(${target_name} scheme ${gen_timestamp} "${gen_files}" ${gen_dst})
endfunction()
