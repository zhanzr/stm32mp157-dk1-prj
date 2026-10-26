#include "stm32mp1xx_hal.h"
#include "openamp.h"
#include "virt_uart.h"
#include "stm32mp15xx_disco.h"
#include "coremark.h"
#include <stdio.h>
#include <string.h>

IPCC_HandleTypeDef hipcc;
VIRT_UART_HandleTypeDef hvirt0;
VIRT_UART_HandleTypeDef hvirt1;

void SystemClock_Config(void);
static void MX_IPCC_Init(void);
void Error_Handler(void);

extern MAIN_RETURN_TYPE coremark_main(void);
extern char system_log_buf[];

static size_t log_sent;

static void flush_log(void)
{
	size_t len = strlen(system_log_buf);

	while (log_sent < len) {
		size_t chunk = len - log_sent;

		if (chunk > 200)
			chunk = 200;
		VIRT_UART_Transmit(&hvirt0,
				   (uint8_t *)system_log_buf + log_sent,
				   (uint16_t)chunk);
		log_sent += chunk;
		HAL_Delay(5);
	}
}

int main(void)
{
#ifndef N_RUNS
#define N_RUNS 3
#endif

	HAL_Init();

	if (IS_ENGINEERING_BOOT_MODE())
		SystemClock_Config();

	__HAL_RCC_HSEM_CLK_ENABLE();

	BSP_LED_Init(LED7);
	BSP_LED_On(LED7);

	MX_IPCC_Init();
	MX_OPENAMP_Init(RPMSG_REMOTE, NULL);

	if (VIRT_UART_Init(&hvirt0) != VIRT_UART_OK)
		Error_Handler();
	if (VIRT_UART_Init(&hvirt1) != VIRT_UART_OK)
		Error_Handler();

	printf("\r\nCoreMark M4 runner: Cortex-M4 @ 208878 kHz "
	       "(%s, %s)\r\n", COMPILER_VERSION, COMPILER_FLAGS);
	printf("iterations=%d runs=%d mem=%s\r\n", ITERATIONS, N_RUNS,
	       MEM_LOCATION);
	flush_log();

	for (int r = 1; r <= N_RUNS; r++) {
		MAIN_RETURN_TYPE ret;

		BSP_LED_On(LED7);
		printf("--- CoreMark run %d on Cortex-M4 ---\r\n", r);
		ret = coremark_main();
		printf("--- run %d done (ret %d) ---\r\n", r, ret);
		flush_log();
		BSP_LED_Off(LED7);
		HAL_Delay(3000);
	}

	printf("CoreMark M4 runner: all %d runs complete\r\n", N_RUNS);
	flush_log();
	while (1) {
		OPENAMP_check_for_message();
		BSP_LED_Toggle(LED7);
		HAL_Delay(200);
	}
}

void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	HAL_PWR_EnableBkUpAccess();
	__HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_MEDIUMHIGH);

	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI |
					   RCC_OSCILLATORTYPE_HSE |
					   RCC_OSCILLATORTYPE_LSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS_DIG;
	RCC_OscInitStruct.LSEState = RCC_LSE_ON;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = 16;
	RCC_OscInitStruct.HSIDivValue = RCC_HSI_DIV1;

	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLL12SOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 3;
	RCC_OscInitStruct.PLL.PLLN = 81;
	RCC_OscInitStruct.PLL.PLLP = 1;
	RCC_OscInitStruct.PLL.PLLQ = 1;
	RCC_OscInitStruct.PLL.PLLR = 1;
	RCC_OscInitStruct.PLL.PLLFRACV = 0x800;
	RCC_OscInitStruct.PLL.PLLMODE = RCC_PLL_FRACTIONAL;
	RCC_OscInitStruct.PLL.RPDFN_DIS = RCC_RPDFN_DIS_DISABLED;
	RCC_OscInitStruct.PLL.TPDFN_DIS = RCC_TPDFN_DIS_DISABLED;

	RCC_OscInitStruct.PLL2.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL2.PLLSource = RCC_PLL12SOURCE_HSE;
	RCC_OscInitStruct.PLL2.PLLM = 3;
	RCC_OscInitStruct.PLL2.PLLN = 66;
	RCC_OscInitStruct.PLL2.PLLP = 2;
	RCC_OscInitStruct.PLL2.PLLQ = 1;
	RCC_OscInitStruct.PLL2.PLLR = 1;
	RCC_OscInitStruct.PLL2.PLLFRACV = 0x1400;
	RCC_OscInitStruct.PLL2.PLLMODE = RCC_PLL_FRACTIONAL;
	RCC_OscInitStruct.PLL2.RPDFN_DIS = RCC_RPDFN_DIS_DISABLED;
	RCC_OscInitStruct.PLL2.TPDFN_DIS = RCC_TPDFN_DIS_DISABLED;

	RCC_OscInitStruct.PLL3.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL3.PLLSource = RCC_PLL3SOURCE_HSE;
	RCC_OscInitStruct.PLL3.PLLM = 2;
	RCC_OscInitStruct.PLL3.PLLN = 34;
	RCC_OscInitStruct.PLL3.PLLP = 2;
	RCC_OscInitStruct.PLL3.PLLQ = 17;
	RCC_OscInitStruct.PLL3.PLLR = 37;
	RCC_OscInitStruct.PLL3.PLLRGE = RCC_PLL3IFRANGE_1;
	RCC_OscInitStruct.PLL3.PLLFRACV = 0x1A04;
	RCC_OscInitStruct.PLL3.PLLMODE = RCC_PLL_FRACTIONAL;
	RCC_OscInitStruct.PLL3.RPDFN_DIS = RCC_RPDFN_DIS_DISABLED;
	RCC_OscInitStruct.PLL3.TPDFN_DIS = RCC_TPDFN_DIS_DISABLED;

	RCC_OscInitStruct.PLL4.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL4.PLLSource = RCC_PLL4SOURCE_HSE;
	RCC_OscInitStruct.PLL4.PLLM = 4;
	RCC_OscInitStruct.PLL4.PLLN = 99;
	RCC_OscInitStruct.PLL4.PLLP = 6;
	RCC_OscInitStruct.PLL4.PLLQ = 8;
	RCC_OscInitStruct.PLL4.PLLR = 8;
	RCC_OscInitStruct.PLL4.PLLRGE = RCC_PLL4IFRANGE_0;
	RCC_OscInitStruct.PLL4.PLLFRACV = 0;
	RCC_OscInitStruct.PLL4.PLLMODE = RCC_PLL_INTEGER;
	RCC_OscInitStruct.PLL4.RPDFN_DIS = RCC_RPDFN_DIS_DISABLED;
	RCC_OscInitStruct.PLL4.TPDFN_DIS = RCC_TPDFN_DIS_DISABLED;

	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
		Error_Handler();

	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_ACLK |
		RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
		RCC_CLOCKTYPE_PCLK3 | RCC_CLOCKTYPE_PCLK4 |
		RCC_CLOCKTYPE_PCLK5 | RCC_CLOCKTYPE_MPU;
	RCC_ClkInitStruct.MPUInit.MPU_Clock = RCC_MPUSOURCE_PLL1;
	RCC_ClkInitStruct.MPUInit.MPU_Div = RCC_MPU_DIV2;
	RCC_ClkInitStruct.AXISSInit.AXI_Clock = RCC_AXISSOURCE_PLL2;
	RCC_ClkInitStruct.AXISSInit.AXI_Div = RCC_AXI_DIV1;
	RCC_ClkInitStruct.MCUInit.MCU_Clock = RCC_MCUSSOURCE_PLL3;
	RCC_ClkInitStruct.MCUInit.MCU_Div = RCC_MCU_DIV1;
	RCC_ClkInitStruct.APB4_Div = RCC_APB4_DIV2;
	RCC_ClkInitStruct.APB5_Div = RCC_APB5_DIV4;
	RCC_ClkInitStruct.APB1_Div = RCC_APB1_DIV2;
	RCC_ClkInitStruct.APB2_Div = RCC_APB2_DIV2;
	RCC_ClkInitStruct.APB3_Div = RCC_APB3_DIV2;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct) != HAL_OK)
		Error_Handler();

	__HAL_RCC_RTC_HSEDIV(24);
}

static void MX_IPCC_Init(void)
{
	hipcc.Instance = IPCC;
	if (HAL_IPCC_Init(&hipcc) != HAL_OK)
		Error_Handler();
}

void Error_Handler(void)
{
	BSP_LED_Off(LED7);
	while (1) {
	}
}
