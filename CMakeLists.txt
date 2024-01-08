cmake_minimum_required(VERSION 3.28)
project(jnlive)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 23)

add_executable (jnlive 
    source/main.cpp
    source/utils.cpp
    source/jackutils.cpp
    source/lilvutils.cpp
    source/lv2_evbuf.c
    source/komplete.cpp
    source/project.cpp
    source/engine.cpp
    source/ringbuf.cpp
    source/realtimethread.cpp
    source/log.cpp
    source/schedule.cpp
    source/gui.cpp
)


target_include_directories (jnlive PRIVATE 
    "./source"
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(JACK REQUIRED jack)
pkg_check_modules(LILV REQUIRED lilv-0)
pkg_check_modules(USB REQUIRED libusb-1.0)
pkg_check_modules(HID REQUIRED hidapi-libusb)
pkg_check_modules(JSON REQUIRED jsoncpp)
pkg_check_modules(ZIX REQUIRED zix-0)
pkg_check_modules(SUIL REQUIRED suil-0)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(GTKMM REQUIRED gtkmm-3.0)

target_link_libraries(jnlive ${JACK_LIBRARIES} ${LILV_LIBRARIES} ${USB_LIBRARIES} ${HID_LIBRARIES} ${JSON_LIBRARIES}  ${ZIX_LIBRARIES} ${SUIL_LIBRARIES} ${GTK3_LIBRARIES} ${GTKMM_LIBRARIES})

target_include_directories(jnlive PRIVATE ${JACK_INCLUDE_DIRS} ${LILV_INCLUDE_DIRS} ${USB_INCLUDE_DIRS} ${HID_INCLUDE_DIRS} ${JSON_INCLUDE_DIRS}  ${ZIX_INCLUDE_DIRS} ${SUIL_INCLUDE_DIRS} ${GTK3_INCLUDE_DIRS} ${GTKMM_INCLUDE_DIRS})

target_link_options(jnlive PRIVATE ${JACK_LDFLAGS_OTHER} ${LILV_LDFLAGS_OTHER} ${USB_LDFLAGS_OTHER} ${HID_LDFLAGS_OTHER} ${JSON_LDFLAGS_OTHER}  ${ZIX_LDFLAGS_OTHER} ${SUIL_LDFLAGS_OTHER} ${GTK3_LDFLAGS_OTHER} ${GTKMM_LDFLAGS_OTHER})
#target_compile_options(jnlive PRIVATE -stdlib=libc++)
#target_link_options(jnlive PRIVATE -stdlib=libc++)
# -fbuiltin-module-map -fimplicit-module-maps -fprebuilt-implicit-modules)