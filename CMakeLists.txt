# This file is part of Desktop App Toolkit,
# a set of libraries for developing nice desktop applications.
#
# For license and copyright information please follow this link:
# https://github.com/desktop-app/legal/blob/master/LEGAL

add_library(lib_ton OBJECT)
add_library(desktop-app::lib_ton ALIAS lib_ton)
init_target(lib_ton)

include(cmake/generate_ton_tl.cmake)
include(cmake/generate_ton_storage_tl.cmake)

get_filename_component(src_loc . REALPATH)

generate_ton_tl(lib_ton ${src_loc}/ton/details/ton_tl_generate.py ${libs_loc}/ton/tl/generate/scheme/tonlib_api.tl)
generate_ton_storage_tl(lib_ton ${src_loc}/ton/details/ton_storage_tl_generate.py ${src_loc}/ton/details/ton_storage.tl)

nice_target_sources(lib_ton ${libs_loc}/ton/tl/generate
PRIVATE
    scheme/tonlib_api.tl
)

target_precompile_headers(lib_ton PRIVATE ${src_loc}/ton/details/ton_pch.h)
nice_target_sources(lib_ton ${src_loc}
PRIVATE
    ton/details/ton_account_viewers.cpp
    ton/details/ton_account_viewers.h
    ton/details/ton_external.cpp
    ton/details/ton_external.h
    ton/details/ton_client.cpp
    ton/details/ton_client.h
    ton/details/ton_key_creator.cpp
    ton/details/ton_key_creator.h
    ton/details/ton_key_destroyer.cpp
    ton/details/ton_key_destroyer.h
    ton/details/ton_local_time_syncer.cpp
    ton/details/ton_local_time_syncer.h
    ton/details/ton_parse_state.cpp
    ton/details/ton_parse_state.h
    ton/details/ton_password_changer.cpp
    ton/details/ton_password_changer.h
    ton/details/ton_request_sender.cpp
    ton/details/ton_request_sender.h
    ton/details/ton_storage.cpp
    ton/details/ton_storage.h
    ton/details/ton_storage.tl
    ton/details/ton_tl_core.h
    ton/details/ton_tl_core_conversion.cpp
    ton/details/ton_tl_core_conversion.h
    ton/details/ton_web_loader.cpp
    ton/details/ton_web_loader.h
    ton/ton_account_viewer.cpp
    ton/ton_account_viewer.h
    ton/ton_result.h
    ton/ton_settings.h
    ton/ton_state.cpp
    ton/ton_state.h
    ton/ton_utility.cpp
    ton/ton_utility.h
    ton/ton_wallet.cpp
    ton/ton_wallet.h
)

target_include_directories(lib_ton
PUBLIC
    ${src_loc}
)

target_link_libraries(lib_ton
PUBLIC
    desktop-app::lib_base
    desktop-app::lib_tl
    desktop-app::lib_storage
    desktop-app::external_expected
PRIVATE
    desktop-app::external_ton
)
