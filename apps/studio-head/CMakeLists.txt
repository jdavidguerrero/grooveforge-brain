# GrooveForge Brain · Studio head — UI de 800×480 en LVGL 8 sobre Linux (o simulador SDL en macOS).
# Arquitectura: apps/docs/variants/studio/theory/01-studio-head-architecture.md · ADR-002.
cmake_minimum_required(VERSION 3.16)
project(studio_head C CXX)

# FetchContent_Populate con detalles declarados (CMake ≥ 3.30) y el install() de LVGL (CMake ≥ 3.31).
if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()
if(POLICY CMP0177)
  cmake_policy(SET CMP0177 NEW)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Backend de pantalla: SDL (Mac/escritorio) o FBDEV (Raspberry Pi, framebuffer + touch evdev).
set(STUDIO_DISPLAY "SDL" CACHE STRING "Backend de pantalla: SDL o FBDEV")
set_property(CACHE STUDIO_DISPLAY PROPERTY STRINGS SDL FBDEV)

include(FetchContent)
# Misma versión que resolvió el firmware del ESP32 (apps/firmware-esp32/.pio), para compartir widgets.
FetchContent_Declare(lvgl
  GIT_REPOSITORY https://github.com/lvgl/lvgl.git
  GIT_TAG v8.4.0
  GIT_SHALLOW TRUE)
FetchContent_Declare(lv_drivers
  GIT_REPOSITORY https://github.com/lvgl/lv_drivers.git
  GIT_TAG v8.3.0
  GIT_SHALLOW TRUE)
FetchContent_GetProperties(lvgl)
if(NOT lvgl_POPULATED)
  FetchContent_Populate(lvgl)
endif()
FetchContent_GetProperties(lv_drivers)
if(NOT lv_drivers_POPULATED)
  FetchContent_Populate(lv_drivers)
endif()

set(LV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/include/lv_conf.h CACHE STRING "" FORCE)
add_subdirectory(${lvgl_SOURCE_DIR} ${lvgl_BINARY_DIR} EXCLUDE_FROM_ALL)
target_include_directories(lvgl SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

option(STUDIO_PERF "Mostrar el monitor de fps/CPU de LVGL" OFF)
if(STUDIO_PERF)
  target_compile_definitions(lvgl PUBLIC STUDIO_PERF=1)
endif()

# Solo los drivers que usamos (lv_drivers trae muchos más).
if(STUDIO_DISPLAY STREQUAL "SDL")
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
  set(DRV_SOURCES ${lv_drivers_SOURCE_DIR}/sdl/sdl.c ${lv_drivers_SOURCE_DIR}/sdl/sdl_common.c)
  set(DRV_DEFS STUDIO_DISPLAY_SDL=1 USE_SDL=1)
elseif(STUDIO_DISPLAY STREQUAL "FBDEV")
  # El framebuffer lo maneja src/hal/fb.cpp (convierte RGB565 → formato real); de lv_drivers solo evdev.
  set(DRV_SOURCES ${lv_drivers_SOURCE_DIR}/indev/evdev.c)
  set(DRV_DEFS STUDIO_DISPLAY_FBDEV=1 USE_EVDEV=1)
else()
  message(FATAL_ERROR "STUDIO_DISPLAY debe ser SDL o FBDEV")
endif()

add_library(studio_drivers STATIC ${DRV_SOURCES})
target_include_directories(studio_drivers PUBLIC ${lv_drivers_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(studio_drivers PUBLIC ${DRV_DEFS})
target_link_libraries(studio_drivers PUBLIC lvgl)
if(STUDIO_DISPLAY STREQUAL "SDL")
  target_link_libraries(studio_drivers PUBLIC PkgConfig::SDL2)
endif()

file(GLOB FONT_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/fonts/*.c)
add_executable(studio-head
  src/main.cpp
  src/hal/tick.c
  src/hal/display.cpp
  src/sim/demo_source.cpp
  src/state/fx_catalog.cpp
  src/state/osc_catalog.cpp
  src/ui/theme.cpp
  src/ui/top_bar.cpp
  src/ui/bottom_bar.cpp
  src/ui/screen_boot.cpp
  src/ui/screen_perform.cpp
  src/ui/screen_ai.cpp
  src/ui/screen_spectrum.cpp
  src/ui/screen_env.cpp
  src/ui/screen_fx.cpp
  src/ui/screen_engine.cpp
  src/ui/screen_presets.cpp
  src/ui/screen_error.cpp
  src/ui/overlays.cpp
  src/ui/painter.cpp
  src/ui/engines.cpp
  src/ui/fx_viz.cpp
  src/ui/edit_draw.cpp
  src/ui/edit_cells.cpp
  src/ui/screen_osc.cpp
  src/ui/screen_panel.cpp
  src/ui/screen_lfo.cpp
  src/ui/screen_sfx.cpp
  src/ui/input.cpp
  src/ui/app.cpp
  src/link/bridge_link.cpp
  ${FONT_SOURCES})
# protocol.h es el mismo header de los dos firmwares (02-bridge-protocol.md); en la Pi se copia con
# rsync manteniendo apps/bridge-protocol al lado de apps/studio-head.
target_include_directories(studio-head PRIVATE src include ${CMAKE_CURRENT_SOURCE_DIR}/../bridge-protocol/include)
find_package(Threads REQUIRED)
if(STUDIO_DISPLAY STREQUAL "FBDEV")
  target_sources(studio-head PRIVATE src/hal/fb.cpp)
endif()
target_link_libraries(studio-head PRIVATE studio_drivers Threads::Threads)  # lvgl llega a través de studio_drivers
target_compile_options(studio-head PRIVATE -Wall -Wextra)
