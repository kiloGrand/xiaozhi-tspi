# 1. 收集测试源文件（包含test_active_device.cpp、test_uuid.cpp等）
file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# 2. 生成测试可执行文件
add_executable(run_tests
               ${TEST_SOURCES}
               ${PROJECT_SOURCE_DIR}/src/uuid.cpp  # 链接uuid.cpp
               ${PROJECT_SOURCE_DIR}/src/http.cpp
               ${PROJECT_SOURCE_DIR}/src/ipc_udp.cpp) 

# 3. 指定头文件路径（保留curl头文件依赖）
target_include_directories(run_tests
                           PRIVATE 
                           ${PROJECT_SOURCE_DIR}/include
                           ${gtest_SOURCE_DIR}/include
                           ${CURL_INCLUDE_DIR})

# 4. 链接依赖库（保留GTest、curl、pthread）
target_link_libraries(run_tests
                      PRIVATE 
                      gtest 
                      gtest_main 
                      ${CURL_LIBRARIES}
                      pthread)

# 5. 注册测试
enable_testing()
add_test(NAME AllTests COMMAND run_tests) # COMMAND改为run_tests，测试名简化为AllTests