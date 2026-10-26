CROSS ?= arm-none-eabi-
CC := $(CROSS)gcc
SIZE := $(CROSS)size
TARGET := OpenAMP_TTY_echo

CPUFLAGS := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
DEFS := -DMETAL_MAX_DEVICE_REGIONS=2 -DUSE_HAL_DRIVER -DSTM32MP157Cxx \
        -D__LOG_TRACE_IO_ -DCORE_CM4 -DDEBUG -DNO_ATOMIC_64_SUPPORT \
        -DMETAL_INTERNAL -DVIRTIO_SLAVE_ONLY
INC := -IInc -IApplication/User \
       -IDrivers/STM32MP1xx_HAL_Driver/Inc \
       -IDrivers/STM32MP1xx_HAL_Driver/Inc/Legacy \
       -IDrivers/CMSIS/Include \
       -IDrivers/CMSIS/Device/ST/STM32MP1xx/Include \
       -IDrivers/BSP/STM32MP15xx_DISCO \
       -IMiddlewares/OpenAMP/open-amp/lib/include \
       -IMiddlewares/OpenAMP/libmetal/lib/include \
       -IMiddlewares/OpenAMP/virtual_driver
CFLAGS := $(CPUFLAGS) -Og -g3 -Wall -fdata-sections -ffunction-sections \
          $(DEFS) $(INC)
ASFLAGS := $(CPUFLAGS) -x assembler-with-cpp $(INC)
LDFLAGS := $(CPUFLAGS) -TSTM32MP157CAAX_RAM.ld --specs=nano.specs \
           -Wl,-Map=$(TARGET).map,--cref -Wl,--gc-sections -lnosys

APP_SRCS := Src/main.c Src/mbox_ipcc.c Src/openamp.c Src/openamp_log.c \
            Src/rsc_table.c Src/lock_resource.c Src/stm32mp1xx_hal_msp.c \
            Src/stm32mp1xx_it.c Src/system_stm32mp1xx.c \
            Application/User/syscalls.c \
            Drivers/BSP/STM32MP15xx_DISCO/stm32mp15xx_disco.c \
            Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal.c \
            Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_cortex.c \
            Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_dma.c \
            Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_dma_ex.c \
            Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_gpio.c \
            Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_hsem.c \
            Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_i2c.c \
            Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_i2c_ex.c \
            Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_ipcc.c \
            Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_pwr.c \
            Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_pwr_ex.c \
            Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_rcc.c \
            Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_rcc_ex.c
MW_SRCS := Middlewares/OpenAMP/open-amp/lib/remoteproc/remoteproc_virtio.c \
           Middlewares/OpenAMP/open-amp/lib/rpmsg/rpmsg.c \
           Middlewares/OpenAMP/open-amp/lib/rpmsg/rpmsg_virtio.c \
           Middlewares/OpenAMP/open-amp/lib/virtio/virtio.c \
           Middlewares/OpenAMP/open-amp/lib/virtio/virtqueue.c \
           Middlewares/OpenAMP/libmetal/lib/device.c \
           Middlewares/OpenAMP/libmetal/lib/init.c \
           Middlewares/OpenAMP/libmetal/lib/io.c \
           Middlewares/OpenAMP/libmetal/lib/log.c \
           Middlewares/OpenAMP/libmetal/lib/shmem.c \
           Middlewares/OpenAMP/libmetal/lib/system/generic/condition.c \
           Middlewares/OpenAMP/libmetal/lib/system/generic/generic_device.c \
           Middlewares/OpenAMP/libmetal/lib/system/generic/generic_init.c \
           Middlewares/OpenAMP/libmetal/lib/system/generic/generic_io.c \
           Middlewares/OpenAMP/libmetal/lib/system/generic/generic_shmem.c \
           Middlewares/OpenAMP/libmetal/lib/system/generic/time.c \
           Middlewares/OpenAMP/libmetal/lib/system/generic/template/sys.c \
           Middlewares/OpenAMP/virtual_driver/virt_uart.c
SRCS := $(APP_SRCS) $(MW_SRCS)
OBJS := $(patsubst %.c,build/%.o,$(SRCS)) build/Startup/startup_stm32mp157caax.o

all: $(TARGET).elf
	$(SIZE) $(TARGET).elf

build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

build/Startup/%.o: Startup/%.s
	@mkdir -p $(dir $@)
	$(CC) -c $(ASFLAGS) $< -o $@

$(TARGET).elf: $(OBJS) STM32MP157CAAX_RAM.ld
	$(CC) $(OBJS) $(LDFLAGS) -o $@

clean:
	rm -rf build $(TARGET).elf $(TARGET).map

.PHONY: all clean
