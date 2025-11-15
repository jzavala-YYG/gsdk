cmake_minimum_required(VERSION 3.5)
project(GSDK_CPP)

# if no build type specified, default to Debug
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# ---- CURL paths (Windows only) ----
if(WIN32)
    set(CURL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/libcurl-vc15-x64-Release-dll-ssl-dll-ipv6-sspi")
    set(CURL_INCLUDE_DIR "${CURL_ROOT}/include")
    # use the actual file name in the lib folder:
    set(CURL_LIBRARY "${CURL_ROOT}/lib/libcurl.lib")
endif()

# ---- One SHARED library (DLL) that contains GSDK + your wrapper ----
add_library(GSDK_CPP SHARED
    "cppsdk/gsdk.cpp"
    "cppsdk/gsdkConfig.cpp"
    "cppsdk/gsdkLog.cpp"
    "cppsdk/gsdkUtils.cpp"
    "cppsdk/jsoncpp.cpp"
    "cppsdk/ManualResetEvent.cpp"
    "gsdkWrapper.cpp"
)

target_include_directories(GSDK_CPP PRIVATE
    cppsdk
    cppsdk/include
    ${CURL_INCLUDE_DIR}
)

target_link_libraries(GSDK_CPP PRIVATE
    ${CURL_LIBRARY}
)

set_target_properties(GSDK_CPP PROPERTIES
    CXX_STANDARD 14
)

if(UNIX)
    set_target_properties(GSDK_CPP PROPERTIES OUTPUT_NAME "GSDK_CPP_Linux")
    target_compile_options(GSDK_CPP PRIVATE -DGSDK_LINUX)
elseif(WIN32)
    # Final DLL name: PlayFabGsdkWrapper.dll
    set_target_properties(GSDK_CPP PROPERTIES OUTPUT_NAME "PlayFabGsdkWrapper")
    target_compile_options(GSDK_CPP PRIVATE -DGSDK_WINDOWS)
endif()



