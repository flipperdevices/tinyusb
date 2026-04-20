MCU_VARIANT = stm32u595xx
CFLAGS += \
  -DSTM32U595xx \

# All source paths should be relative to the top level.
LD_FILE = ${FAMILY_PATH}/linker/STM32U595xx_FLASH.ld

# For flash-jlink target
JLINK_DEVICE = stm32u595zi
