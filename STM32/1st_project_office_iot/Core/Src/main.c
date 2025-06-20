/* USER CODE BEGIN Header */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "clcd.h"
#include "mfrc522.h"
#include "esp.h"
#include "dht.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define ROOM_NUMBER "101"
#define USER_ID "JTY"
#define USER_NAME "JEONG"
#define USER_CODE "8315D829"

#ifdef __GNUC__
/* With GCC, small printf (option LD Linker->Libraries->Small printf
 set to 'Yes') calls __io_putchar() */
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif /* __GNUC__ */
#define ARR_CNT 5
#define CMD_SIZE 50
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

UART_HandleTypeDef huart2;
UART_HandleTypeDef huart6;

/* USER CODE BEGIN PV */
int last_door_state = -1;  // 초기값은 존재하지 않는 값으로 설정

// esp
uint8_t rx2char;
extern cb_data_t cb_data;
extern volatile unsigned char rx2Flag;
extern volatile char rx2Data[50];

static int ret = 0;
DHT11_TypeDef dht11Data;
static char buff[30];

// room_info 구조체
typedef struct {
	int room_number;
	char user_name[20];
} ROOM_INFO;

typedef struct {
	char room_status[17]; // IN, LEC, VAC, MTG, BRK, OUT
} ROOM_STATUS;

ROOM_INFO room_info = { 101, "JEONG" };
ROOM_STATUS current_room_status = { "TagCard&PushBTN" };

// button
const uint16_t button_pins[6] = { BUTTON0_Pin, BUTTON1_Pin, BUTTON2_Pin,
BUTTON3_Pin, BUTTON4_Pin, BUTTON5_Pin };
int button_state[6] = { 0 };
int last_button_state[6] = { 0 };
int button_flag[6] = { 0 };

// lcd
char line1[18] = { '\0' };
char line2[18] = { '\0' };

// timer3
static volatile int tim3Flag1Sec = 0;
static volatile unsigned int tim3Sec = 0;
static volatile int tim3Flag5Sec = 0;

// RFID
uint8_t cardID[5];
char uid_str[16];
static volatile int authentication_flag = 0;
static volatile int auth_start_time = -1; // 인증 시작 시점의 tim3Sec 저장

// esp
static int i = 0;
static char *pToken;
static char *pArray[ARR_CNT] = { 0 };
static char sendBuf[MAX_UART_COMMAND_LEN] = { 0 };

typedef struct {
	char *client;
	char *door;
} BUFFF;

static BUFFF ppAArray = { 0 };

typedef struct {
	char *cclient;
	char *ssetroom;
	char *rroom;
	char *sstatus;
} BBUFFF;

static BBUFFF pppAAArray = { 0 };

// SERVO(DOOR LOCK)
int door_pulse = 0;
int door_state;  // 1: unlock 0: lock

// pir
static volatile int pir_flag = 0;

// dht
static int last_fanFlag = 0;
static int fanFlag = 0;

// ADC servo2
__IO uint16_t ADC1ConvertValue = 0;
__IO uint16_t adcFlag = 0;
static int curtainFlag = 0;
static int last_curtainFlag = 0;
static int curtain_pulse = 0;

// DHT
static char dhtbuff[30];
static int last_checked_sec = 0;
DHT11_TypeDef dht11Data;
/* USER CODE END PV */


/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART6_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM4_Init(void);
/* USER CODE BEGIN PFP */
// authenicatin timeout
void authentication_timeout();
// detect
void detect();

// ESP
void esp_init();

char strBuff[MAX_ESP_COMMAND_LEN];

void room_status_set();
void room_status_display();
void user_authentication();

void esp_event(char*);

void PIR_Init(void);

//dht
void dht_status_motor();

//cds
void cds_status_servo();
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

	/* USER CODE BEGIN 1 */
	// SERVO
	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */
	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */
	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_USART2_UART_Init();
	MX_ADC1_Init();
	MX_TIM3_Init();
	MX_USART6_UART_Init();
	MX_I2C1_Init();
	MX_SPI1_Init();
	MX_TIM4_Init();
	/* USER CODE BEGIN 2 */

	// TIMER 4
	if (HAL_TIM_Base_Start_IT(&htim4) != HAL_OK)
		Error_Handler();
	if (HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1) != HAL_OK)
		Error_Handler();
	if (HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2) != HAL_OK)
		Error_Handler();

	// ESP
	printf("Start main() - wifi\r\n");
	ret |= drv_uart_init();
	ret |= drv_esp_init();
	if (ret != 0) {
		printf("Esp response error\r\n");
		Error_Handler();
	}

	AiotClient_Init();
	// DHT
	DHT11_Init();

	if (HAL_TIM_Base_Start_IT(&htim3) != HAL_OK)
		Error_Handler();
	// LCD
	LCD_init(&hi2c1);
	LCD_writeStringXY(0, 0, "hello lcd");

	// RFID
	MFRC522_Init();                               // RC522 초기화
	uint8_t version = MFRC522_ReadRegister(0x37); // VersionReg
	printf("RC522 Version: 0x%02X\r\n", version);
	printf("카드를 인식해주세요\r\n");

	//PIR
	PIR_Init();

	//ADC
	if (HAL_ADC_Start_IT(&hadc1) != HAL_OK)
		Error_Handler();

	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {
		if (strstr((char*) cb_data.buf, "+IPD")
				&& cb_data.buf[cb_data.length - 1] == '\n') {
			//?��?��?���??  \r\n+IPD,15:[KSH_LIN]HELLO\n
			strcpy(strBuff, strchr((char*) cb_data.buf, '['));
			memset(cb_data.buf, 0x0, sizeof(cb_data.buf));
			cb_data.length = 0;
			esp_event(strBuff);
		}
		if (rx2Flag) {
			printf("recv2 : %s\r\n", rx2Data);
			rx2Flag = 0;
		}

		room_status_display();
		user_authentication();
		authentication_timeout();

		if (door_state != last_door_state) {
			if (door_state == 1) {
				door_pulse = 1500;
				printf("UNLOCK!!\r\n");
			} else {
				door_pulse = 500;
				printf("LOCK!!\r\n");
			}
			last_door_state = door_state;
		}
		__HAL_TIM_SetCompare(&htim4, TIM_CHANNEL_2, door_pulse - 1);

		room_status_set();
		detect();

		// DHT, DC
		if (door_state == 1) {

			if (tim3Sec % 5 == 0 && tim3Sec != last_checked_sec) //5초에 한번 실행되게
					{
				dht_status_motor(); // 한번 실행
				cds_status_servo();
				last_checked_sec = tim3Sec; // 재실행 방지용
			}
		}
	}
	/* USER CODE END WHILE */

	/* USER CODE BEGIN 3 */
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	/** Configure the main internal regulator output voltage
	 */
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	RCC_OscInitStruct.PLL.PLLM = 16;
	RCC_OscInitStruct.PLL.PLLN = 336;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
	RCC_OscInitStruct.PLL.PLLQ = 4;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief ADC1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_ADC1_Init(void) {

	/* USER CODE BEGIN ADC1_Init 0 */
	/* USER CODE END ADC1_Init 0 */

	ADC_ChannelConfTypeDef sConfig = { 0 };

	/* USER CODE BEGIN ADC1_Init 1 */
	/* USER CODE END ADC1_Init 1 */

	/** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
	 */
	hadc1.Instance = ADC1;
	hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV6;
	hadc1.Init.Resolution = ADC_RESOLUTION_12B;
	hadc1.Init.ScanConvMode = DISABLE;
	hadc1.Init.ContinuousConvMode = DISABLE;
	hadc1.Init.DiscontinuousConvMode = DISABLE;
	hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
	hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
	hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
	hadc1.Init.NbrOfConversion = 1;
	hadc1.Init.DMAContinuousRequests = DISABLE;
	hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
	if (HAL_ADC_Init(&hadc1) != HAL_OK) {
		Error_Handler();
	}

	/** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
	 */
	sConfig.Channel = ADC_CHANNEL_0;
	sConfig.Rank = 1;
	sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
	if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN ADC1_Init 2 */
	/* USER CODE END ADC1_Init 2 */

}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void) {

	/* USER CODE BEGIN I2C1_Init 0 */
	/* USER CODE END I2C1_Init 0 */

	/* USER CODE BEGIN I2C1_Init 1 */
	/* USER CODE END I2C1_Init 1 */
	hi2c1.Instance = I2C1;
	hi2c1.Init.ClockSpeed = 100000;
	hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
	hi2c1.Init.OwnAddress1 = 0;
	hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c1.Init.OwnAddress2 = 0;
	hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN I2C1_Init 2 */
	/* USER CODE END I2C1_Init 2 */

}

/**
 * @brief SPI1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI1_Init(void) {

	/* USER CODE BEGIN SPI1_Init 0 */

	/* USER CODE END SPI1_Init 0 */

	/* USER CODE BEGIN SPI1_Init 1 */

	/* USER CODE END SPI1_Init 1 */
	/* SPI1 parameter configuration*/
	hspi1.Instance = SPI1;
	hspi1.Init.Mode = SPI_MODE_MASTER;
	hspi1.Init.Direction = SPI_DIRECTION_2LINES;
	hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi1.Init.NSS = SPI_NSS_SOFT;
	hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
	hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi1.Init.CRCPolynomial = 10;
	if (HAL_SPI_Init(&hspi1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN SPI1_Init 2 */

	/* USER CODE END SPI1_Init 2 */

}

/**
 * @brief TIM3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM3_Init(void) {

	/* USER CODE BEGIN TIM3_Init 0 */
	/* USER CODE END TIM3_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
	TIM_MasterConfigTypeDef sMasterConfig = { 0 };

	/* USER CODE BEGIN TIM3_Init 1 */
	/* USER CODE END TIM3_Init 1 */
	htim3.Instance = TIM3;
	htim3.Init.Prescaler = 84 - 1;
	htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim3.Init.Period = 1000 - 1;
	htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim3) != HAL_OK) {
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM3_Init 2 */
	/* USER CODE END TIM3_Init 2 */

}

/**
 * @brief TIM4 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM4_Init(void) {

	/* USER CODE BEGIN TIM4_Init 0 */

	/* USER CODE END TIM4_Init 0 */

	TIM_MasterConfigTypeDef sMasterConfig = { 0 };
	TIM_OC_InitTypeDef sConfigOC = { 0 };

	/* USER CODE BEGIN TIM4_Init 1 */

	/* USER CODE END TIM4_Init 1 */
	htim4.Instance = TIM4;
	htim4.Init.Prescaler = 84 - 1;
	htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim4.Init.Period = 20000 - 1;
	htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_PWM_Init(&htim4) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 0;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_1)
			!= HAL_OK) {
		Error_Handler();
	}
	if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_2)
			!= HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM4_Init 2 */

	/* USER CODE END TIM4_Init 2 */
	HAL_TIM_MspPostInit(&htim4);

}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void) {

	/* USER CODE BEGIN USART2_Init 0 */
	/* USER CODE END USART2_Init 0 */

	/* USER CODE BEGIN USART2_Init 1 */
	/* USER CODE END USART2_Init 1 */
	huart2.Instance = USART2;
	huart2.Init.BaudRate = 115200;
	huart2.Init.WordLength = UART_WORDLENGTH_8B;
	huart2.Init.StopBits = UART_STOPBITS_1;
	huart2.Init.Parity = UART_PARITY_NONE;
	huart2.Init.Mode = UART_MODE_TX_RX;
	huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart2.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&huart2) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART2_Init 2 */
	/* USER CODE END USART2_Init 2 */

}

/**
 * @brief USART6 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART6_UART_Init(void) {

	/* USER CODE BEGIN USART6_Init 0 */
	/* USER CODE END USART6_Init 0 */

	/* USER CODE BEGIN USART6_Init 1 */
	/* USER CODE END USART6_Init 1 */
	huart6.Instance = USART6;
	huart6.Init.BaudRate = 38400;
	huart6.Init.WordLength = UART_WORDLENGTH_8B;
	huart6.Init.StopBits = UART_STOPBITS_1;
	huart6.Init.Parity = UART_PARITY_NONE;
	huart6.Init.Mode = UART_MODE_TX_RX;
	huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart6.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&huart6) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART6_Init 2 */
	/* USER CODE END USART6_Init 2 */

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	/* USER CODE BEGIN MX_GPIO_Init_1 */
	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOH_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOB, RC522_RST_Pin | GPIO_PIN_13 | GPIO_PIN_14,
			GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin : B1_Pin */
	GPIO_InitStruct.Pin = B1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : BUTTON0_Pin BUTTON1_Pin BUTTON2_Pin BUTTON3_Pin
	 BUTTON4_Pin BUTTON5_Pin BUTTON6_Pin DHT_Pin */
	GPIO_InitStruct.Pin = BUTTON0_Pin | BUTTON1_Pin | BUTTON2_Pin | BUTTON3_Pin
			| BUTTON4_Pin | BUTTON5_Pin | BUTTON6_Pin | DHT_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pin : PA4 */
	GPIO_InitStruct.Pin = GPIO_PIN_4;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pin : PIR_Pin */
	GPIO_InitStruct.Pin = PIR_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(PIR_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : RC522_RST_Pin PB13 PB14 */
	GPIO_InitStruct.Pin = RC522_RST_Pin | GPIO_PIN_13 | GPIO_PIN_14;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/*Configure GPIO pin : LED_Pin */
	GPIO_InitStruct.Pin = LED_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : PA9 PA10 */
	GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_10;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/* EXTI interrupt init*/
	HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI1_IRQn);

	/* USER CODE BEGIN MX_GPIO_Init_2 */
	/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) // 1ms 마다 호출
{
	static int tim3Cnt = 0;
	tim3Cnt++;
	if (tim3Cnt >= 1000) { // 1ms * 1000 = 1Sec
		tim3Flag1Sec = 1;
		tim3Sec++;
		tim3Cnt = 0;
	}
	if (tim3Sec % 5 == 0) {
		tim3Flag5Sec = 1;
	}
}

void room_status_set() {
	for (int i = 0; i < 6; i++) {
		button_state[i] = HAL_GPIO_ReadPin(GPIOC, button_pins[i]);

		if (last_button_state[i] == 0 && button_state[i] == 1
				&& authentication_flag == 1) {
			switch (i) {
			case 0:
				printf("button : %d\r\n", i);
				sprintf(current_room_status.room_status, "%s",
						"In Room        ");
				sprintf(sendBuf, "[PRJ_SQL]SETROOM@%s@IN\n", ROOM_NUMBER);
				esp_send_data(sendBuf);
				HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
				door_state = 1;
				break;
			case 1:
				printf("button : %d\r\n", i);
				sprintf(current_room_status.room_status, "%s",
						"Lecture        ");
				sprintf(sendBuf, "[PRJ_SQL]SETROOM@%s@LEC\n", ROOM_NUMBER);
				esp_send_data(sendBuf);
				HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
				door_state = 0;
				break;
			case 2:
				printf("button : %d\r\n", i);
				sprintf(current_room_status.room_status, "%s",
						"Vacation       ");
				sprintf(sendBuf, "[PRJ_SQL]SETROOM@%s@VAC\n", ROOM_NUMBER);
				esp_send_data(sendBuf);
				HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
				door_state = 0;
				break;
			case 3:
				printf("button : %d\r\n", i);
				sprintf(current_room_status.room_status, "%s",
						"Meeting        ");
				sprintf(sendBuf, "[PRJ_SQL]SETROOM@%s@MTG\n", ROOM_NUMBER);
				esp_send_data(sendBuf);
				HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
				door_state = 0;
				break;
			case 4:
				printf("button : %d\r\n", i);
				sprintf(current_room_status.room_status, "%s",
						"Break          ");
				sprintf(sendBuf, "[PRJ_SQL]SETROOM@%s@BRK\n", ROOM_NUMBER);
				esp_send_data(sendBuf);
				HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
				door_state = 0;
				break;
			case 5:
				printf("button : %d\r\n", i);
				sprintf(current_room_status.room_status, "%s",
						"Off Work       ");
				sprintf(sendBuf, "[PRJ_SQL]SETROOM@%s@OUT\n", ROOM_NUMBER);
				esp_send_data(sendBuf);
				HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
				door_state = 0;
				break;
			}
		}

		last_button_state[i] = button_state[i];
	}
}

void room_status_display() {

	sprintf(line1, "%s - %s", ROOM_NUMBER, USER_NAME);
	// sprintf(line2, "%s", "LEC");
	sprintf(line2, "%s", current_room_status.room_status);

	LCD_writeStringXY(0, 0, line1);
	LCD_writeStringXY(1, 0, line2);
}

//void user_authentication() {
//	if (MFRC522_Check(cardID) == MI_OK) {
//		sprintf(uid_str, "%02X%02X%02X%02X", cardID[0], cardID[1], cardID[2],
//				cardID[3]);
//		printf("Card UID: %s\r\n", uid_str);
//
//		if (strcmp(uid_str, USER_CODE) == 0) {
//			authentication_flag = 1;
//			auth_start_time = tim3Sec; // 현재 시간을 저장
//			printf("인증성공!\r\n");
//			sprintf(line2, "%s", "Success!!");
//			LCD_writeStringXY(1, 0, line2);
//		} else {
//			authentication_flag = 0;
//			auth_start_time = -1;
//			printf("인증실패!\r\n");
//			sprintf(line2, "%s", "Failed!!");
//			LCD_writeStringXY(1, 0, line2);
//		}
//	}
//}

void user_authentication() {
	if (MFRC522_Check(cardID) == MI_OK) {
		sprintf(uid_str, "%02X%02X%02X%02X", cardID[0], cardID[1], cardID[2],
				cardID[3]);
		printf("Card UID: %s\r\n", uid_str);

		// SQL 클라이언트로 인증 요청 전송
		sprintf(sendBuf, "[PRJ_SQL]CERT@%s@%s\n", ROOM_NUMBER, uid_str);
		esp_send_data(sendBuf);
		printf("Send to SQL client: %s\r\n", sendBuf);

		// 응답 대기 타이머 시작
		int wait_start_time = tim3Sec;
		int timeout_sec = 2;  // 최대 대기 시간 (초)

		while ((tim3Sec - wait_start_time) < timeout_sec) {
			if (strstr((char*) cb_data.buf, "[PRJ_SQL]CERT@OK")) {
				authentication_flag = 1;
				auth_start_time = tim3Sec;
				printf("인증성공!\r\n");
				sprintf(line2, "%s", "Success!!");
				LCD_writeStringXY(1, 0, line2);

				goto auth_done;
			} else if (strstr((char*) cb_data.buf, "[PRJ_SQL]CERT@NO")) {
				authentication_flag = 0;
				auth_start_time = -1;
				printf("인증실패!\r\n");
				sprintf(line2, "%s", "Failed!!");
				LCD_writeStringXY(1, 0, line2);

				goto auth_done;
			}
		}

		// 응답 없음 (타임아웃)
		authentication_flag = 0;
		auth_start_time = -1;
		printf("인증응답 없음!\r\n");
		sprintf(line2, "%s", "No Resp!!");
		LCD_writeStringXY(1, 0, line2);

		auth_done:
		// 수신 버퍼 정리
		memset(cb_data.buf, 0, sizeof(cb_data.buf));
		cb_data.length = 0;
	}
}

void esp_event(char *recvBuf) {
	int i = 0;
	char *pToken;
	char *pArray[ARR_CNT] = { 0 };
	char sendBuf[MAX_UART_COMMAND_LEN] = { 0 };

	strBuff[strlen(recvBuf) - 1] = '\0'; //'\n' cut
	printf("\r\nDebug recv : %s\r\n", recvBuf);

	pToken = strtok(recvBuf, "[@]");
	while (pToken != NULL) {
		pArray[i] = pToken;
		if (++i >= ARR_CNT)
			break;
		pToken = strtok(NULL, "[@]");
	}

	for (int j = 0; j < i; j++) {
		if (pArray[j] == NULL) {
			printf("Warning: NULL in pArray[%d]\r\n", j);
			return;
		}
	}

	if (i < 2) {
		printf("Parsing error: too few tokens (%d)\r\n", i);
		return;
	}

	if (i == 2) {
		ppAArray.client = pArray[0];
		ppAArray.door = pArray[1];

		if (!strcmp(pArray[1], "UNLOCK")) {
			sprintf(sendBuf, "[%s]%s\r\n", "PRJ_SQL", ppAArray.door);
			door_state = 1;
		}
	}

	else if (i == 4) {

		pppAAArray.cclient = pArray[0];
		pppAAArray.ssetroom = pArray[1];
		pppAAArray.rroom = pArray[2];
		pppAAArray.sstatus = pArray[3];

		if (!strcmp(pppAAArray.ssetroom, "SETROOM")
				&& !strcmp(pppAAArray.rroom, ROOM_NUMBER)) {

			if (!strcmp(pppAAArray.sstatus, "IN")) {
				sprintf(current_room_status.room_status, "%s",
						"In Room        ");
				HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
				door_state = 1;

			} else if (!strcmp(pppAAArray.sstatus, "LEC")) {
				sprintf(current_room_status.room_status, "%s",
						"Lecture        ");
				HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
				door_state = 0;

			} else if (!strcmp(pppAAArray.sstatus, "VAC")) {
				sprintf(current_room_status.room_status, "%s",
						"Vacation       ");
				HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
				door_state = 0;

			} else if (!strcmp(pppAAArray.sstatus, "MTG")) {
				sprintf(current_room_status.room_status, "%s",
						"Meeting        ");
				HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
				door_state = 0;

			} else if (!strcmp(pppAAArray.sstatus, "BRK")) {
				sprintf(current_room_status.room_status, "%s",
						"Break          ");
				HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
				door_state = 0;

			} else if (!strcmp(pppAAArray.sstatus, "OUT")) {
				printf("out!!!!!!!\r\n");
				sprintf(current_room_status.room_status, "%s",
						"Off Work       ");
				HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
				door_state = 0;
			} else {
				// 처리할 수 없는 상태일 경우 무시
				return;
			}

			sprintf(sendBuf, "[PRJ_SQL]%s@%s@%s@%s\r\n", pppAAArray.ssetroom,
					pppAAArray.rroom, pppAAArray.sstatus, pppAAArray.cclient);
			printf("client = %s\r\n", pppAAArray.cclient);
		}

	}

	else if (!strncmp(pArray[1], " New conn", 8)) {
		//	   printf("Debug : %s, %s\r\n",pArray[0],pArray[1]);
		return;
	} else if (!strncmp(pArray[1], " Already log", 8)) {
		// 	    printf("Debug : %s, %s\r\n",pArray[0],pArray[1]);
		esp_client_conn();
		return;
	} else
		return;

	esp_send_data(sendBuf);
	printf("Debug send : %s\r\n", sendBuf);
}

// pir
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if (GPIO_Pin == PIR_Pin) {

		pir_flag = 1;

	}
}

void PIR_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	__HAL_RCC_GPIOB_CLK_ENABLE();

	GPIO_InitStruct.Pin = PIR_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(PIR_GPIO_Port, &GPIO_InitStruct);

	HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI1_IRQn);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
	if (hadc->Instance == ADC1) {
		ADC1ConvertValue = HAL_ADC_GetValue(hadc);
		adcFlag = 1;                     // 메인 루프에 알림
	}
}

void dht_status_motor() {
	dht11Data = DHT11_readData();
	if (dht11Data.rh_byte1 != 255) {
		sprintf(dhtbuff, "h: %d%% t: %d.%d'C", dht11Data.rh_byte1,
				dht11Data.temp_byte1, dht11Data.temp_byte2);
		printf("%s\r\n", dhtbuff);
		printf("dht11Data.rh_byte1 = %d \r\n", dht11Data.rh_byte1);

		if (dht11Data.rh_byte1 >= 60 || dht11Data.temp_byte1 > 30)
			fanFlag = 1;
		else
			fanFlag = 0;

		if (fanFlag != last_fanFlag) {
			if (fanFlag == 1) {
				HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET);
				HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
				HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
				printf("fan on!!\r\n");
			} else if (fanFlag == 0) {
				HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);
				HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
				HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
				printf("fan off!!\r\n");
			}
			last_fanFlag = fanFlag;
		}
	} else
		printf("DHT11 response error\r\n");

}

void cds_status_servo() {
	//ADC
	if (adcFlag) {
		adcFlag = 0;
		if (ADC1ConvertValue > 1500) {
			curtainFlag = 1;
		} else {
			curtainFlag = 0;
		}
		printf("CDS : %d\r\n", ADC1ConvertValue);

		HAL_ADC_Start_IT(&hadc1);   // 다음 변환 시작
	}

	if (curtainFlag != last_curtainFlag) {
		if (curtainFlag == 1) {
			curtain_pulse = 500;
			printf("curtain on!!\r\n");
		} else if (curtainFlag == 0) {
			curtain_pulse = 1500;
			printf("curtain off!!\r\n");
		}
		last_curtainFlag = curtainFlag;
	}
	__HAL_TIM_SetCompare(&htim4, TIM_CHANNEL_1, curtain_pulse - 1);
}

void detect() {
	if (pir_flag == 1 && door_state == 0) {
		pir_flag = 0;
		printf("움직임 감지됨!\r\n");
		sprintf(sendBuf, "[PRJ_CEN]DETECTED@101\r\n");
		esp_send_data(sendBuf);
	}
}

void authentication_timeout() {

	// 10초 경과 확인
	if (authentication_flag == 1 && (tim3Sec - auth_start_time >= 10)) {
		authentication_flag = 0;
		auth_start_time = -1;
		printf("인증시간 초과\r\n");
		sprintf(line2, "%s", "Time Over");
		LCD_writeStringXY(1, 0, line2);
	}
}
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
