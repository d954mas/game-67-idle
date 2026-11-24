# cmake -DFEATURE=<features/garment-cloth> -DSTUDIO=<Studio root> -DOUT=<header> -P runtime_stamp.cmake
#
# Writes OUT with GH_RUNTIME_SHA256, the hash of the runtime sources a garment_host library is compiled from, by the
# rule of garment.host.runtime_sha256 (the same files in the same order, text hashed with LF line ends), so the library
# proves by content which runtime it steps. host.py refuses a library whose stamp is not the current sources' hash.
file(GLOB sources "${FEATURE}/src/*.c")
file(GLOB headers "${FEATURE}/include/garment/*.h")
list(SORT sources)
list(SORT headers)
set(joined "")
foreach(path IN LISTS sources headers ITEMS "${FEATURE}/tools/host/garment_host.c")
    file(READ "${path}" text)
    string(REPLACE "\r\n" "\n" text "${text}")
    string(SHA256 digest "${text}")
    file(RELATIVE_PATH relative "${STUDIO}" "${path}")
    string(APPEND joined "${relative}${digest}")
endforeach()
string(SHA256 runtime "${joined}")
set(header "#define GH_RUNTIME_SHA256 \"${runtime}\"\n")
set(old "")
if(EXISTS "${OUT}")
    file(READ "${OUT}" old)
endif()
if(NOT "${old}" STREQUAL "${header}")
    file(WRITE "${OUT}" "${header}")
endif()
