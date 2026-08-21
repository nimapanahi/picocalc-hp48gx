cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2_w CACHE STRING "Target board")
include(pico_sdk_import.cmake)

project(hp48gx_picocalc C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

pico_sdk_init()
find_package(Python3 REQUIRED COMPONENTS Interpreter)

if(NOT ROM_TEMPLATE_BUILD)
  if(NOT HP48_ROM)
    message(FATAL_ERROR "Pass your HP 48GX revision-R ROM with -DHP48_ROM=/full/path/to/gxrom-r")
  endif()
  get_filename_component(HP48_ROM_ABS "${HP48_ROM}" ABSOLUTE)
  if(NOT EXISTS "${HP48_ROM_ABS}")
    message(FATAL_ERROR "ROM not found: ${HP48_ROM_ABS}")
  endif()
endif()

set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(ROM_BIN "${GENERATED_DIR}/hp48gx_r.unpacked.bin")
set(ROM_ASM "${GENERATED_DIR}/hp48gx_r_blob.S")
if(ROM_TEMPLATE_BUILD)
  add_custom_command(
    OUTPUT "${ROM_BIN}" "${ROM_ASM}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${GENERATED_DIR}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/prepare_rom.py"
            --template "${ROM_BIN}" "${ROM_ASM}"
    DEPENDS "${CMAKE_SOURCE_DIR}/tools/prepare_rom.py"
    VERBATIM
  )
else()
  add_custom_command(
    OUTPUT "${ROM_BIN}" "${ROM_ASM}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${GENERATED_DIR}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/prepare_rom.py"
            "${HP48_ROM_ABS}" "${ROM_BIN}" "${ROM_ASM}"
    DEPENDS "${HP48_ROM_ABS}" "${CMAKE_SOURCE_DIR}/tools/prepare_rom.py"
    VERBATIM
  )
endif()
set_source_files_properties("${ROM_ASM}" PROPERTIES GENERATED TRUE)

if(HP48_BUILD_UNIX_TIME)
  set(BUILD_UNIX_TIME "${HP48_BUILD_UNIX_TIME}")
else()
  string(TIMESTAMP BUILD_UNIX_TIME "%s" UTC)
endif()

add_executable(hp48gx_picocalc
  src/main.c
  src/core/actions.c
  src/core/core_runtime.c
  src/core/device.c
  src/core/display.c
  src/core/emulate.c
  src/core/memory.c
  src/core/register.c
  src/core/timer.c
  src/platform/keyboard.c
  src/platform/lcd.c
  src/platform/platform_ui.c
  src/platform/state.c
  "${ROM_ASM}"
)

target_include_directories(hp48gx_picocalc PRIVATE
  "${CMAKE_SOURCE_DIR}/src/core"
  "${CMAKE_SOURCE_DIR}/src/platform"
)
target_compile_definitions(hp48gx_picocalc PRIVATE
  BUILD_UNIX_TIME=${BUILD_UNIX_TIME}
  PICO_FLASH_SIZE_BYTES=4194304
)
target_compile_options(hp48gx_picocalc PRIVATE
  -O3 -ffunction-sections -fdata-sections
  -Wno-format -Wno-unused-parameter -Wno-unused-variable
)
target_link_libraries(hp48gx_picocalc
  pico_stdlib
  hardware_clocks
  hardware_flash
  hardware_gpio
  hardware_i2c
  hardware_pio
  hardware_sync
)

pico_generate_pio_header(hp48gx_picocalc
  "${CMAKE_CURRENT_LIST_DIR}/src/platform/lcd_spi.pio"
)

pico_enable_stdio_usb(hp48gx_picocalc 1)
pico_enable_stdio_uart(hp48gx_picocalc 0)
pico_add_extra_outputs(hp48gx_picocalc)
