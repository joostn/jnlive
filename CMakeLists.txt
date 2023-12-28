cmake_minimum_required(VERSION 3.28)
project(jnlive)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 23)

add_executable (jnlive 
    source/main.cpp
    source/utils.cpp
)

#target_sources(jnlive PUBLIC FILE_SET CXX_MODULES FILES
#    source/utils.cpp
#)

target_include_directories (jnlive PRIVATE 
    "./source"
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(JACK REQUIRED jack)
pkg_check_modules(LILV REQUIRED lilv-0)

target_link_libraries(jnlive ${JACK_LIBRARIES} ${LILV_LIBRARIES})

target_include_directories(jnlive PRIVATE ${JACK_INCLUDE_DIRS} ${LILV_INCLUDE_DIRS})

target_link_options(jnlive PRIVATE ${JACK_LDFLAGS_OTHER} ${LILV_LDFLAGS_OTHER})
#target_compile_options(jnlive PRIVATE -stdlib=libc++)
#target_link_options(jnlive PRIVATE -stdlib=libc++)
# -fbuiltin-module-map -fimplicit-module-maps -fprebuilt-implicit-modules)