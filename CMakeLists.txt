add_executable(
  picofract
  picofract.cpp
)

# Pull in pico libraries that we need
target_link_libraries(picofract pico_stdlib pico_multicore hardware_spi hardware_pwm hardware_dma pico_display)

# create map/bin/hex file etc.
pico_add_extra_outputs(picofract)