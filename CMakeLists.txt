cmake_minimum_required(VERSION 3.10)
project(HydrologyModel VERSION 1.0 LANGUAGES CXX)

# 设置 C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 开启编译优化（Release 模式）
set(CMAKE_BUILD_TYPE Release)

# 查找 GDAL 库（必需）
find_package(GDAL REQUIRED)
if(NOT GDAL_FOUND)
    message(FATAL_ERROR "GDAL library not found. Please install libgdal-dev.")
endif()

# 打印 GDAL 版本信息
message(STATUS "Found GDAL: ${GDAL_LIBRARY} (version ${GDAL_VERSION})")
message(STATUS "GDAL include dirs: ${GDAL_INCLUDE_DIR}")

# 收集所有源文件（排除自动生成的备份等）
file(GLOB SOURCES
    "*.cpp"
    # 如果有额外子目录，可以添加，但当前所有 cpp 都在根目录
)

# 添加可执行文件
add_executable(hydro_model ${SOURCES})

# 链接 GDAL 库
target_link_libraries(hydro_model ${GDAL_LIBRARY})

# 包含 GDAL 头文件目录
target_include_directories(hydro_model PRIVATE ${GDAL_INCLUDE_DIR})

# 添加当前目录为头文件搜索路径（用于项目内部头文件）
target_include_directories(hydro_model PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 可选：添加编译定义（如需特定宏）
# target_compile_definitions(hydro_model PRIVATE -D_GLIBCXX_USE_CXX11_ABI=0)