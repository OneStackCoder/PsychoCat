cmake_minimum_required(VERSION 3.21)
project(PsychoCat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Slint 包查找路径：
# - 本地：使用 ../third_party/slint 下的预编译包
# - CI：通过 -DSLINT_PREFIX=<path> 指定（GitHub Actions 下载解压的位置）
if(SLINT_PREFIX)
    list(APPEND CMAKE_PREFIX_PATH "${SLINT_PREFIX}")
else()
    file(GLOB SLINT_LOCAL_PKG "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/slint/*")
    list(APPEND CMAKE_PREFIX_PATH ${SLINT_LOCAL_PKG})
endif()

find_package(Slint REQUIRED)

add_executable(PsychoCat
    src/main.cpp
)

# 把 ui/main.slint 编译成 C++，并生成头文件 main.h
slint_target_sources(PsychoCat ui/main.slint)

# 默认情况下 Slint 在运行时从磁盘加载 @image-url 引用的图片，散包后图片会丢失；
# 改为把图片原始文件直接内嵌进可执行文件，产物只需 exe + 运行库即可独立运行
set_target_properties(PsychoCat PROPERTIES SLINT_EMBED_RESOURCES embed-files)

target_link_libraries(PsychoCat PRIVATE Slint::Slint)

# Windows 下默认是控制台子系统，双击 exe 会额外弹出一个黑框；
# 设为 GUI 子系统程序，并让 MSVC 仍以 main() 作为入口（否则会找不到 WinMain）
if(WIN32)
    set_target_properties(PsychoCat PROPERTIES WIN32_EXECUTABLE TRUE)
    if(MSVC)
        target_link_options(PsychoCat PRIVATE "/ENTRY:mainCRTStartup")
    endif()
endif()

# Linux 下用 Xlib 修复中文窗口标题乱码
if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    target_include_directories(PsychoCat PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(PsychoCat PRIVATE ${X11_LIBRARIES})
endif()
