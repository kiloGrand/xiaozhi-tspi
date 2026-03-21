# 1. 收集测试源文件（包含test_record.cpp等）
file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# 2. 生成测试可执行文件
add_executable(run_tests
               ${TEST_SOURCES}
               ${PROJECT_SOURCE_DIR}/src/record.cpp  # 链接record.cpp
               ${PROJECT_SOURCE_DIR}/src/aplay.cpp
               ${PROJECT_SOURCE_DIR}/src/opus.cpp
               ) 

# 3. 指定头文件路径（保留curl头文件依赖）
target_include_directories(run_tests
                           PRIVATE 
                           ${PROJECT_SOURCE_DIR}/include
                           ${gtest_SOURCE_DIR}/include
                           ${CURL_INCLUDE_DIR})

# 4. 链接依赖库
target_link_libraries(run_tests
                      PRIVATE 
                      gtest 
                      gtest_main 
                      asound    # ALSA音频库（-lasound）
                      opus      # opus编码库（-lopus） 
                      speexdsp  # speexdsp音频处理库（-lspeexdsp），降噪
                      pthread)

# 5. 注册测试
enable_testing()
add_test(NAME AllTests COMMAND run_tests) # COMMAND改为run_tests，测试名简化为AllTests