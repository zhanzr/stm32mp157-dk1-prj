M4BASE := ../../a7_m4_hello/m4
CROSS ?= arm-none-eabi-
CC := $(CROSS)gcc
SIZE := $(CROSS)size
TARGET := coremark_m4

CPUFLAGS := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
DEFS := -DMETAL_MAX_DEVICE_REGIONS=2 -DUSE_HAL_DRIVER -DSTM32MP157Cxx \
        -D__LOG_TRACE_IO_ -DCORE_CM4 -DNO_ATOMIC_64_SUPPORT \
        -DMETAL_INTERNAL -DVIRTIO_SLAVE_ONLY \
        -DPERFORMANCE_RUN=1 -DITERATIONS=10000 -DN_RUNS=3
INC := -I. -Icoremark \
       -I$(M4BASE)/Inc \
       -I$(M4BASE)/Drivers/STM32MP1xx_HAL_Driver/Inc \
       -I$(M4BASE)/Drivers/STM32MP1xx_HAL_Driver/Inc/Legacy \
       -I$(M4BASE)/Drivers/CMSIS/Include \
       -I$(M4BASE)/Drivers/CMSIS/Device/ST/STM32MP1xx/Include \
       -I$(M4BASE)/Drivers/BSP/STM32MP15xx_DISCO \
       -I$(M4BASE)/Middlewares/OpenAMP/open-amp/lib/include \
       -I$(M4BASE)/Middlewares/OpenAMP/libmetal/lib/include \
       -I$(M4BASE)/Middlewares/OpenAMP/virtual_driver
CFLAGS := $(CPUFLAGS) -O2 -g -Wall -fdata-sections -ffunction-sections \
          $(DEFS) $(INC)
BENCHOPT := -Ofast -ffp-contract=fast -funroll-all-loops
ASFLAGS := $(CPUFLAGS) -x assembler-with-cpp $(INC)
LDFLAGS := $(CPUFLAGS) -T$(M4BASE)/STM32MP157CAAX_RAM.ld --specs=nano.specs -u _printf_float \
           -Wl,-Map=$(TARGET).map,--cref -Wl,--gc-sections -lnosys

BENCH_SRCS := main.c core_portme.c \
              coremark/core_list_join.c coremark/core_main.c \
              coremark/core_matrix.c coremark/core_state.c \
              coremark/core_util.c
VENDOR_SRCS := $(M4BASE)/Src/mbox_ipcc.c \
               $(M4BASE)/Src/openamp.c \
               $(M4BASE)/Src/openamp_log.c \
               $(M4BASE)/Src/rsc_table.c \
               $(M4BASE)/Src/lock_resource.c \
               $(M4BASE)/Src/stm32mp1xx_hal_msp.c \
               $(M4BASE)/Src/stm32mp1xx_it.c \
               $(M4BASE)/Src/system_stm32mp1xx.c \
               $(M4BASE)/Application/User/syscalls.c \
               $(M4BASE)/Drivers/BSP/STM32MP15xx_DISCO/stm32mp15xx_disco.c \
               $(M4BASE)/Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal.c \
               $(M4BASE)/Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_cortex.c \
               $(M4BASE)/Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_dma.c \
               $(M4BASE)/Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_dma_ex.c \
               $(M4BASE)/Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_gpio.c \
               $(M4BASE)/Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_hsem.c \
               $(M4BASE)/Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_i2c.c \
               $(M4BASE)/Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_i2c_ex.c \
               $(M4BASE)/Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_ipcc.c \
               $(M4BASE)/Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_pwr.c \
               $(M4BASE)/Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_pwr_ex.c \
               $(M4BASE)/Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_rcc.c \
               $(M4BASE)/Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_rcc_ex.c
MW_SRCS := $(M4BASE)/Middlewares/OpenAMP/open-amp/lib/remoteproc/remoteproc_virtio.c \
           $(M4BASE)/Middlewares/OpenAMP/open-amp/lib/rpmsg/rpmsg.c \
           $(M4BASE)/Middlewares/OpenAMP/open-amp/lib/rpmsg/rpmsg_virtio.c \
           $(M4BASE)/Middlewares/OpenAMP/open-amp/lib/virtio/virtio.c \
           $(M4BASE)/Middlewares/OpenAMP/open-amp/lib/virtio/virtqueue.c \
           $(M4BASE)/Middlewares/OpenAMP/libmetal/lib/device.c \
           $(M4BASE)/Middlewares/OpenAMP/libmetal/lib/init.c \
           $(M4BASE)/Middlewares/OpenAMP/libmetal/lib/io.c \
           $(M4BASE)/Middlewares/OpenAMP/libmetal/lib/log.c \
           $(M4BASE)/Middlewares/OpenAMP/libmetal/lib/shmem.c \
           $(M4BASE)/Middlewares/OpenAMP/libmetal/lib/system/generic/condition.c \
           $(M4BASE)/Middlewares/OpenAMP/libmetal/lib/system/generic/generic_device.c \
           $(M4BASE)/Middlewares/OpenAMP/libmetal/lib/system/generic/generic_init.c \
           $(M4BASE)/Middlewares/OpenAMP/libmetal/lib/system/generic/generic_io.c \
           $(M4BASE)/Middlewares/OpenAMP/libmetal/lib/system/generic/generic_shmem.c \
           $(M4BASE)/Middlewares/OpenAMP/libmetal/lib/system/generic/time.c \
           $(M4BASE)/Middlewares/OpenAMP/libmetal/lib/system/generic/template/sys.c \
           $(M4BASE)/Middlewares/OpenAMP/virtual_driver/virt_uart.c
SRCS := $(BENCH_SRCS) $(VENDOR_SRCS) $(MW_SRCS)
OBJS := $(patsubst %.c,build/%.o,$(SRCS)) build/startup.o

build/main.o build/core_portme.o: CFLAGS += $(BENCHOPT)
build/coremark/%.o: CFLAGS += $(BENCHOPT)

all: $(TARGET).elf
	$(SIZE) $(TARGET).elf

build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

build/startup.o: $(M4BASE)/Startup/startup_stm32mp157caax.s
	@mkdir -p build
	$(CC) -c $(ASFLAGS) $< -o $@

$(TARGET).elf: $(OBJS) $(M4BASE)/STM32MP157CAAX_RAM.ld
	$(CC) $(OBJS) $(LDFLAGS) -o $@

clean:
	rm -rf build $(TARGET).elf $(TARGET).map

.PHONY: all clean
