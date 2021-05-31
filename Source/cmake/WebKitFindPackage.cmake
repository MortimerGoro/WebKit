# This file overrides the behavior of find_package for WebKit projects.
#
# CMake provides Find modules that are used within WebKit. However there are ports
# where the default behavior does not work and they need to explicitly set their
# values. There are also targets available in later versions of CMake which are
# nicer to work with.
#
# The purpose of this file is to make the behavior of find_package consistent
# across ports and CMake versions.

# CMake provided targets. Remove wrappers whenever the minimum version is bumped.
#
# CURL::libcurl : since 3.12
# ICU::<C> : since 3.7
# Freetype::Freetype: since 3.10
# LibXml2::LibXml2: since 3.12
# LibXslt::LibXslt: since never
# JPEG::JPEG: since 3.12
# OpenSSL::SSL: Since 3.4
# PNG::PNG : since 3.4
# Threads::Threads: since 3.1
# ZLIB::ZLIB: Since 3.1

macro(find_package package)
    set(_found_package OFF)

    # Apple internal builds for Windows need to use _DEBUG_SUFFIX so manually
    # find all third party libraries that have a corresponding Find module within
    # CMake's distribution.
    if (WTF_PLATFORM_APPLE_WIN)
        set(_found_package ON)

        if ("${package}" STREQUAL "ICU")
            find_path(ICU_INCLUDE_DIR NAMES unicode/utypes.h)

            find_library(ICU_I18N_LIBRARY NAMES libicuin${DEBUG_SUFFIX})
            find_library(ICU_UC_LIBRARY NAMES libicuuc${DEBUG_SUFFIX})
            # AppleWin does not provide a separate data library
            find_library(ICU_DATA_LIBRARY NAMES libicuuc${DEBUG_SUFFIX})

            if (NOT ICU_INCLUDE_DIR OR NOT ICU_I18N_LIBRARY OR NOT ICU_UC_LIBRARY)
                message(FATAL_ERROR "Could not find ICU")
            endif ()

            set(ICU_INCLUDE_DIRS ${ICU_INCLUDE_DIR})
            set(ICU_LIBRARIES ${ICU_I18N_LIBRARY} ${ICU_UC_LIBRARY})
            set(ICU_FOUND ON)
        elseif ("${package}" STREQUAL "LibXml2")
            find_path(LIBXML2_INCLUDE_DIR NAMES libxml/xpath.h)
            find_library(LIBXML2_LIBRARY NAMES libxml2${DEBUG_SUFFIX})

            if (NOT LIBXML2_INCLUDE_DIR OR NOT LIBXML2_LIBRARY)
                message(FATAL_ERROR "Could not find LibXml2")
            endif ()

            set(LIBXML2_INCLUDE_DIRS ${LIBXML2_INCLUDE_DIR})
            set(LIBXML2_LIBRARIES ${LIBXML2_LIBRARY})
            set(LIBXML2_FOUND ON)
        elseif ("${package}" STREQUAL "LibXslt")
            find_path(LIBXSLT_INCLUDE_DIR NAMES libxslt/xslt.h)
            find_library(LIBXSLT_LIBRARY NAMES libxslt${DEBUG_SUFFIX})

            if (NOT LIBXSLT_INCLUDE_DIR OR NOT LIBXSLT_LIBRARY)
                message(FATAL_ERROR "Could not find LibXslt")
            endif ()

            find_library(LIBXSLT_EXSLT_LIBRARY NAMES libexslt${DEBUG_SUFFIX})

            set(LIBXSLT_INCLUDE_DIRS ${LIBXSLT_INCLUDE_DIR})
            set(LIBXSLT_LIBRARIES ${LIBXSLT_LIBRARY})
            set(LIBXSLT_FOUND ON)
        elseif ("${package}" STREQUAL "ZLIB")
            find_path(ZLIB_INCLUDE_DIR NAMES zlib.h PATH_SUFFIXES zlib)
            find_library(ZLIB_LIBRARY NAMES zdll${DEBUG_SUFFIX})

            if (NOT ZLIB_INCLUDE_DIR OR NOT ZLIB_LIBRARY)
                message(FATAL_ERROR "Could not find ZLIB")
            endif ()

            set(ZLIB_INCLUDE_DIRS ${ZLIB_INCLUDE_DIR})
            set(ZLIB_LIBRARIES ${ZLIB_LIBRARY})
            set(ZLIB_FOUND ON)
        else ()
           set(_found_package OFF)
        endif ()
    else ()
        if ("${package}" STREQUAL "ZLIB")
          find_package(PkgConfig)
          pkg_check_modules(PC_ZLIB zlib)
          set(ZLIB_INCLUDE_DIRS ${PC_ZLIB_INCLUDE_DIRS})
          set(ZLIB_LIBRARY ${PC_ZLIB_LINK_LIBRARIES})
          if (ZLIB_INCLUDE_DIRS AND ZLIB_LIBRARY)
            set(ZLIB_FOUND ON)
            set(_found_package ON)
          endif ()
        elseif("${package}" STREQUAL "ICU")
          find_package(PkgConfig)
          pkg_search_module(ICU_UC icu-uc)
          set(ICU_UC_LIBRARY ${ICU_UC_LIBRARY_DIRS})
          set(ICU_INCLUDE_DIRS ${ICU_UC_INCLUDE_DIRS})

          if (ICU_UC_LIBRARY AND ICU_INCLUDE_DIRS)
              set(ICU_I18N_LIBRARY ${ICU_UC_LIBRARY}/libicui18n.so)
              set(ICU_DATA_LIBRARY ${ICU_UC_LIBRARY}/libicudata.so)
              set(ICU_UC_LIBRARY ${ICU_UC_LIBRARY}/libicuuc.so)
              set(ICU_LIBRARIES ${ICU_UC_LIBRARY})
              set(ICU_FOUND ON)
              set(_found_package ON)
          endif ()
        elseif ("${package}" STREQUAL "Freetype")
          find_package(PkgConfig)
          pkg_check_modules(PC_FREETYPE freetype2)
          set(FREETYPE_INCLUDE_DIRS ${PC_FREETYPE_INCLUDE_DIRS})
          set(FREETYPE_LIBRARY ${PC_FREETYPE_LINK_LIBRARIES})
          if (FREETYPE_INCLUDE_DIRS AND FREETYPE_LIBRARY)
            set(FREETYPE_FOUND ON)
            set(_found_package ON)
          endif ()
        elseif ("${package}" STREQUAL "JPEG")
          find_package(PkgConfig)
          pkg_check_modules(PC_JPEG libjpeg)
          set(JPEG_INCLUDE_DIRS ${PC_JPEG_INCLUDE_DIRS})
          set(JPEG_LIBRARY ${PC_JPEG_LINK_LIBRARIES})
          if (JPEG_INCLUDE_DIRS AND JPEG_LIBRARY)
            set(JPEG_FOUND ON)
            set(_found_package ON)
          endif ()
        elseif ("${package}" STREQUAL "PNG")
          find_package(PkgConfig)
          pkg_check_modules(PC_PNG libpng16)
          set(PNG_INCLUDE_DIRS ${PC_PNG_INCLUDE_DIRS})
          set(PNG_LIBRARY ${PC_PNG_LIBRARY_DIRS}/libpng.so)
          if (PNG_INCLUDE_DIRS AND PNG_LIBRARY)
            set(PNG_FOUND ON)
            set(_found_package ON)
          endif ()
        elseif ("${package}" STREQUAL "WebP")
          find_package(PkgConfig)
          pkg_check_modules(PC_WEBP libwebp)
          set(WEBP_INCLUDE_DIRS ${PC_WEBP_INCLUDE_DIRS})
          set(WEBP_LIBRARY ${PC_WEBP_LINK_LIBRARIES})
          pkg_check_modules(PC_WEBP_DECODER libwebpdecoder)
          set(WEBP_DECODER_INCLUDE_DIRS ${PC_WEBP_DECODER_INCLUDE_DIRS})
          set(WEBP_DECODER_LIBRARY ${PC_WEBP_DECODER_LINK_LIBRARIES})
          if (WEBP_INCLUDE_DIRS AND WEBP_LIBRARY AND WEBP_DECODER_INCLUDE_DIRS AND WEBP_DECODER_LIBRARY)
            set(WEBP_FOUND ON)
            set(_found_package ON)
          endif ()
        elseif ("${package}" STREQUAL "LibGcrypt")
          find_package(PkgConfig)
          pkg_check_modules(PC_LIBGCRYPT libgcrypt)
          set(LIBGCRYPT_INCLUDE_DIRS ${PC_LIBGCRYPT_INCLUDE_DIRS})
          pkg_check_modules(PC_WEBP libwebp)
          set(LIBGCRYPT_LIBRARY ${PC_WEBP_LIBRARY_DIRS}/libgcrypt.so)
          if (LILBGCRYPT_INCLUDE_DIRS AND LIBGCRYPT_LIBRARY)
            set(LIBGCRYPT_FOUND ON)
            set(_found_package ON)
          endif ()
        endif ()
    endif ()

    # Apple builds have a unique location for ICU
    if (USE_APPLE_ICU AND "${package}" STREQUAL "ICU")
        set(_found_package ON)

        set(ICU_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/ICU/Headers)

        # Apple just has a single tbd/dylib for ICU.
        find_library(ICU_I18N_LIBRARY icucore)
        find_library(ICU_UC_LIBRARY icucore)
        find_library(ICU_DATA_LIBRARY icucore)

        set(ICU_LIBRARIES ${ICU_UC_LIBRARY})
        set(ICU_FOUND ON)
        message(STATUS "Found ICU: ${ICU_LIBRARIES}")
    endif ()

    if (NOT _found_package)
        _find_package(${ARGV})
    endif ()

    # Create targets that are present in later versions of CMake or are referenced above
    if ("${package}" STREQUAL "CURL")
        if (CURL_FOUND AND NOT TARGET CURL::libcurl)
            add_library(CURL::libcurl UNKNOWN IMPORTED)
            set_target_properties(CURL::libcurl PROPERTIES
                IMPORTED_LOCATION "${CURL_LIBRARIES}"
                INTERFACE_INCLUDE_DIRECTORIES "${CURL_INCLUDE_DIRS}"
            )
        endif ()
    elseif ("${package}" STREQUAL "ICU")
        if (ICU_FOUND AND NOT TARGET ICU::data)
            add_library(ICU::data UNKNOWN IMPORTED)
            set_target_properties(ICU::data PROPERTIES
                IMPORTED_LOCATION "${ICU_DATA_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${ICU_INCLUDE_DIRS}"
                IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
            )

            add_library(ICU::i18n UNKNOWN IMPORTED)
            set_target_properties(ICU::i18n PROPERTIES
                IMPORTED_LOCATION "${ICU_I18N_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${ICU_INCLUDE_DIRS}"
                IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
            )

            add_library(ICU::uc UNKNOWN IMPORTED)
            set_target_properties(ICU::uc PROPERTIES
                IMPORTED_LOCATION "${ICU_UC_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${ICU_INCLUDE_DIRS}"
                IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
            )
        endif ()
    elseif ("${package}" STREQUAL "LibXml2")
        if (LIBXML2_FOUND AND NOT TARGET LibXml2::LibXml2)
            add_library(LibXml2::LibXml2 UNKNOWN IMPORTED)
            set_target_properties(LibXml2::LibXml2 PROPERTIES
                IMPORTED_LOCATION "${LIBXML2_LIBRARIES}"
                INTERFACE_INCLUDE_DIRECTORIES "${LIBXML2_INCLUDE_DIRS}"
                INTERFACE_COMPILE_OPTIONS "${LIBXML2_DEFINITIONS}"
            )
        endif ()
    elseif ("${package}" STREQUAL "LibXslt")
        if (LIBXSLT_FOUND AND NOT TARGET LibXslt::LibXslt)
            add_library(LibXslt::LibXslt UNKNOWN IMPORTED)
            set_target_properties(LibXslt::LibXslt PROPERTIES
                IMPORTED_LOCATION "${LIBXSLT_LIBRARIES}"
                INTERFACE_INCLUDE_DIRECTORIES "${LIBXSLT_INCLUDE_DIR}"
                INTERFACE_COMPILE_OPTIONS "${LIBXSLT_DEFINITIONS}"
            )
        endif ()
        if (LIBXSLT_EXSLT_LIBRARY AND NOT TARGET LibXslt::LibExslt)
            add_library(LibXslt::LibExslt UNKNOWN IMPORTED)
            set_target_properties(LibXslt::LibExslt PROPERTIES
                IMPORTED_LOCATION "${LIBXSLT_EXSLT_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${LIBXSLT_INCLUDE_DIR}"
            )
        endif ()
    elseif ("${package}" STREQUAL "JPEG")
        if (JPEG_FOUND AND NOT TARGET JPEG::JPEG)
            add_library(JPEG::JPEG UNKNOWN IMPORTED)
            set_target_properties(JPEG::JPEG PROPERTIES
                IMPORTED_LOCATION "${JPEG_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${JPEG_INCLUDE_DIR}"
            )
        endif ()
    elseif ("${package}" STREQUAL "ZLIB")
        if (ZLIB_FOUND AND NOT TARGET ZLIB::ZLIB)
            add_library(ZLIB::ZLIB UNKNOWN IMPORTED)
            set_target_properties(ZLIB::ZLIB PROPERTIES
                IMPORTED_LOCATION "${ZLIB_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIRS}"
            )
        endif ()
    elseif ("${package}" STREQUAL "Freetype")
        if (FREETYPE_FOUND AND NOT TARGET Freetype::Freetype)
            add_library(Freetype::Freetype UNKNOWN IMPORTED)
            set_target_properties(Freetype::Freetype PROPERTIES
                IMPORTED_LOCATION "${FREETYPE_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${FREETYPE_INCLUDE_DIRS}"
            )
        endif ()
    elseif ("${package}" STREQUAL "PNG")
        if (PNG_FOUND AND NOT TARGET PNG::PNG)
            add_library(PNG::PNG UNKNOWN IMPORTED)
            set_target_properties(PNG::PNG PROPERTIES
                IMPORTED_LOCATION "${PNG_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${PNG_INCLUDE_DIRS}"
            )
        endif ()
    elseif ("${package}" STREQUAL "WebP")
        if (WEBP_FOUND AND NOT TARGET WebP::WebP)
            add_library(WebP::WebP UNKNOWN IMPORTED)
            set_target_properties(WebP::WebP PROPERTIES
                IMPORTED_LOCATION "${WEBP_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${WEBP_INCLUDE_DIRS}"
            )
            add_library(WebPDecoder::WebPDecoder UNKNOWN IMPORTED)
            set_target_properties(WebPDecoder::WebPDecoder PROPERTIES
                IMPORTED_LOCATION "${WEBP_DECODER_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${WEBP_DECODER_INCLUDE_DIRS}"
            )
        endif ()
    elseif ("${package}" STREQUAL "LibGcrypt")
        if (LIBGCRYPT_FOUND AND NOT TARGET LibGcrypt::LibGcrypt)
            add_library(LibGcrypt::LibGcrypt UNKNOWN IMPORTED)
            set_target_properties(LibGcrypt::LibGcrypt PROPERTIES
                IMPORTED_LOCATION "${LIBGCRYPT_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${LIBGCRYPT_INCLUDE_DIRS}"
            )
        endif ()
    endif ()
endmacro()
