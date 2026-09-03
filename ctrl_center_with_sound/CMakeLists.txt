cmake_minimum_required(VERSION 3.14)

project(ctrl_center_with_sound
        LANGUAGES CXX
        VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# ==============================================================================
# 第三方依赖
# ==============================================================================
find_package(CURL REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
set(WEBSOCKETPP_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/third_party/websocketpp)

# ==============================================================================
# 主程序
# ==============================================================================
add_executable(my_ctrl_center
               src/main.cpp
               src/xiaozhi_control.cpp
               src/ws_client.cpp
               src/udp_endpoint.cpp
               src/http.cpp
               src/uuid.cpp
               src/AlsaAudioBase.cpp
               src/AlsaCapture.cpp
               src/AlsaPlayback.cpp
               src/OpusWrapper.cpp)

target_include_directories(my_ctrl_center
                           PRIVATE
                           ${PROJECT_SOURCE_DIR}/include
                           ${CURL_INCLUDE_DIR}
                           ${Boost_INCLUDE_DIRS}
                           ${OPENSSL_INCLUDE_DIR}
                           ${WEBSOCKETPP_INCLUDE_DIR})

target_link_libraries(my_ctrl_center
                      PRIVATE
                      ${CURL_LIBRARIES}
                      ${Boost_LIBRARIES}
                      OpenSSL::SSL
                      OpenSSL::Crypto
                      asound
                      opus
                      speexdsp
                      pthread)

