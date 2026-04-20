set(MCU_VARIANT stm32u595xx)
set(JLINK_DEVICE stm32u595ri)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    STM32U595xx
    )
endfunction()
