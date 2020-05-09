/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

osThreadId defaultTaskHandle;
osTimerId secondTickHandle;
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
void StartDefaultTask(void const * argument);
void secondTickCallback(void const * argument);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define NCOL 80
#define NROW 23

#define SCREEN_MAIN_MENU 0
#define SCREEN_INPUT_SIZE 1
#define SCREEN_PLAY 2
#define SCREEN_PAUSE 3
#define SCREEN_HOW_TO_PLAY 4
#define SCREEN_CONGRATULATION 5

#define CHAR_LEFT 'a'
#define CHAR_RIGHT 'd'
#define CHAR_UP 'w'
#define CHAR_DOWN 's'
#define CHAR_ENTER 13
#define CHAR_BACKSPACE 8
#define CHAR_ESC '\033'
#define CHAR_SPACE ' '

int curr_time = 0;

char screen[NROW + 1][NCOL + 1];
char input;

int max(int a, int b) {
	return (a > b) ? a : b;
}

int randint(int s, int e) {
	return rand() % (e - s + 1) + s;
}

void swap(int* a, int* b) {
	int temp = *a;
	*a = *b;
	*b = temp;
}

/*
 * ============== CONSTANTS ==============
 */

char* MAIN_MENU[] = {"START", "HOW TO PLAY"};
char* PAUSE_MENU[] = {"RESUME", "RESTART", "EXIT"};

char* HOW_TO_PLAY[] = {
	"Press A, D to move left / right",
	"Press SPACE to toggle swap mode",
	"Press ESC to pause or restart"
};

/*
 ============== SCREEN ==============
 */

int curr_screen = SCREEN_MAIN_MENU;

void printline(char s[]) {
	HAL_UART_Transmit(&huart2, s, sizeof(s), 1000000);
	//HAL_UART_Transmit(&huart2, "\n", sizeof("\n"), 1000000);
}

void printscreen() {
	//char s[NROW * NCOL + 2 * NROW + 1];
	HAL_UART_Transmit(&huart2, "\033[H\033[J", sizeof("\033[H\033[J"), 1000000);
	int k = 0;
	for (int i = 0; i < NROW; i++) {
		HAL_UART_Transmit(&huart2, &screen[i], NCOL, 1000000);
		for (int j = 0; j < NCOL; j++) {
			//s[k++] = screen[i][j];
			//HAL_UART_Transmit(&huart2, &screen[i][j], sizeof(screen[i][j]), 1000000);
		}
		//s[k++] = '\r';
		//s[k++] = '\n';
		HAL_UART_Transmit(&huart2, "\r\n", sizeof("\r\n"), 1000000);
	}
	//s[k] = 0;
	//HAL_UART_Transmit(&huart2, s, sizeof(s), 1000000);
}

void printscreenbak() {
	for (int i = 0; i < NROW; i++) {
		printline(screen[i]);
	}
}

void resetscreen() {
	for (int i = 0; i < NROW; i++) {
		screen[i][NCOL] = 0;
		for (int j = 0; j < NCOL; j++) {
			screen[i][j] = ' ';
		}
	}
}

void fillstring(char *s, int r, int c) {
	int len = strlen(s);
	for (int i = 0; i < len; i++) {
		screen[r][c + i] = s[i];
	}
}

void fillstringcenter(char *s, int r, int isactive) {
	int len = strlen(s);
	fillstring(s, r, (NCOL-len)/2);
	if (isactive) {
		screen[r][(NCOL-len)/2 - 2] = '>';
		screen[r][(NCOL-len)/2 + len + 1] = '<';
	}
}

/*
 * ============== MENU ==============
 */

int menu_activei = 0;
int menu_maxi = 0;

void menu_up() {
	menu_activei--;
	if (menu_activei < 0) menu_activei = 0;
}

void menu_down() {
	menu_activei++;
	if (menu_activei > menu_maxi) {
		menu_activei = menu_maxi;
	}
}

void fillmenu(char** menudata, int menulen, int activei) {
	int LOGO_ROW = 8;
	int START_ROW = 12;

	if (menulen > 2) {
		LOGO_ROW -= 2;
		START_ROW -= 2;
	}

	char* TITLE;
	if (curr_time % 2 == 0) {
		TITLE = "SWAPSORT";
	} else {
		TITLE = "SORTSWAP";
	}

	fillstringcenter(TITLE, LOGO_ROW, 0);

	for(int i = 0;i<menulen;i++) {
		int ROW = START_ROW + i*2;

		fillstringcenter(menudata[i], ROW, i == activei);
	}

	fillstringcenter("Press W to Up, S to Down, ENTER to Choose", NROW-1, 0);
}

/*
 ============== STICK NUMBERS ==============
 */

int numbers[100];
int number_len = 0;
int number_active_i = 0;
int number_swap_mode = 0;

int scan_count = 0;
int swap_count = 0;

// Score
char swap_score_text[100];
char scan_score_text[100];

int play_start_time = 0;
char play_time_text[100];

int is_playing = 0;

void makestick(int x, int height, char c) {
	for (int i = NROW - 1; i >= max(NROW - height, 0); i--) {
		screen[i][x] = c;
	}
}

void initstick(int len) {
	number_len = len;
	for (int i = 0; i < number_len; i++) {
		numbers[i] = randint(1, NROW - 4);
	}
	number_active_i = 0;
	number_swap_mode = 0;
	play_start_time = curr_time;
	scan_count = 0;
	swap_count = 0;
	is_playing = 1;
}

void drawstick() {
	int stickwidth = NCOL / number_len;
	int stickmargin = (NCOL % number_len) / 2;
	for (int i = stickmargin, j = 0; j < number_len; i += stickwidth, j++) {
		makestick(i, numbers[j], (number_active_i == j) ? (number_swap_mode ? '#' : 'X') : '|');
	}

	sprintf(swap_score_text, "SWAP %d", swap_count);
	sprintf(scan_score_text, "SCAN %d", scan_count);

	fillstring(swap_score_text, 0, 0);
	fillstring(scan_score_text, 1, 0);

	// Play time
	int play_min = (curr_time - play_start_time) / 60;
	int play_second = (curr_time - play_start_time) % 60;

	sprintf(play_time_text, "%02d:%02d", play_min, play_second);

	fillstring(play_time_text, 0, NCOL-5);
}

/*
 * ============== TEXT INPUT ==============
 */
char text_input[256];
int text_p = 0;
int text_max_len = 2;
char* text_input_hint = "";

void clear_text_input() {
	for(int i = 0;i<text_p;i++) {
		text_input[i] = 0;
	}
	text_p = 0;
}

void text_input_append(char c) {
	if (text_p >= text_max_len) return;
	text_input[text_p++] = c;
}

void text_input_backspace() {
	if(text_p == 0) return;
	text_input[--text_p] = 0;
}

int text_input_read_int() {
	return atoi(text_input);
}

void fill_text_input(int r, int c) {
	fillstring(text_input, r, c);
}

void fill_text_input_center(int r) {
	fillstringcenter(text_input, r, 0);
}

void render_text_input_screen(char* title) {
	int TITLE_ROW = 8;
	int INPUT_ROW = 12;
	int HINT_ROW = 13;
	int ENTER_ROW = 16;

	fillstringcenter(title, TITLE_ROW, 0);
	fill_text_input_center(INPUT_ROW);
	fillstringcenter(text_input_hint, HINT_ROW, 0);
	fillstringcenter("Press ENTER to Submit, ESC to Back", ENTER_ROW, 0);
}

/*
 * ============== HOW TO PLAY ==============
 */
void render_how_to_play() {
	int TITLE_ROW = 5;
	int CONTENT_ROW = 9;
	int CONTINUE_ROW = 17;

	fillstringcenter("HOW TO PLAY", TITLE_ROW, 0);

	for(int i = 0; i < 3; i++) {
		fillstringcenter(HOW_TO_PLAY[i], CONTENT_ROW + i*2, 0);
	}

	fillstringcenter("Press any key to continue...", CONTINUE_ROW, 0);
}

/*
 * ============== CONGRATULATION ==============
 */
int finish_time = 0;

void render_congratulation() {
	int TITLE_ROW = 5;
	int CONTENT_ROW = 11;
	int CONTINUE_ROW = 18;

	fillstringcenter("CONGRATULATION", TITLE_ROW, 0);
	fillstringcenter("YOU HAVE SORTED IT USING", TITLE_ROW + 1, 0);

	int play_min = (finish_time - play_start_time) / 60;
	int play_second = (finish_time - play_start_time) % 60;

	sprintf(swap_score_text, "SWAP %d", swap_count);
	sprintf(scan_score_text, "SCAN %d", scan_count);
	sprintf(play_time_text, "TIME %02d:%02d", play_min, play_second);

	fillstring(swap_score_text, CONTENT_ROW, 35);
	fillstring(scan_score_text, CONTENT_ROW + 1, 35);
	fillstring(play_time_text, CONTENT_ROW + 2, 35);

	fillstringcenter("Press ENTER to continue...", CONTINUE_ROW, 0);
}

/*
 * ============== SCREEN CONTROLLER ==============
 */

void change_screen(int name) {
	curr_screen = name;
	switch(name) {
	case SCREEN_MAIN_MENU:
		menu_activei = 0;
		menu_maxi = 1;
		break;
	case SCREEN_INPUT_SIZE:
		clear_text_input();
		text_max_len = 2;
		break;
	case SCREEN_PAUSE:
		menu_activei = 0;
		menu_maxi = 2;
		break;
	case SCREEN_CONGRATULATION:
		is_playing = 0;
		finish_time = curr_time;
		break;
	}
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
	change_screen(SCREEN_MAIN_MENU);
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
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* USER CODE BEGIN RTOS_MUTEX */
	/* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
	/* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* Create the timer(s) */
  /* definition and creation of secondTick */
  osTimerDef(secondTick, secondTickCallback);
  secondTickHandle = osTimerCreate(osTimer(secondTick), osTimerPeriodic, NULL);
  osTimerStart(secondTickHandle, 1000);

  /* USER CODE BEGIN RTOS_TIMERS */
	/* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
	/* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
	/* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* Start scheduler */
  osKernelStart();
  
  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	}
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage 
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the CPU, AHB and APB busses clocks 
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
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 921600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	int old_number_active_i;

	switch (curr_screen) {
	case SCREEN_MAIN_MENU:
		switch(input) {
		case CHAR_DOWN:
			menu_down();
			break;
		case CHAR_UP:
			menu_up();
			break;
		case CHAR_ENTER:
			if (menu_activei == 0) {
				change_screen(SCREEN_INPUT_SIZE);
			} else if (menu_activei == 1) {
				change_screen(SCREEN_HOW_TO_PLAY);
			}
			break;
		}
		break;
	case SCREEN_INPUT_SIZE:
		if (input >= '0' && input <= '9') {
			text_input_append(input);
		} else if (input == CHAR_ENTER) {
			if (text_p == 0) {
				text_input_hint = "Cannot empty";
			} else {
				int size = text_input_read_int();
				if (size > 0 && size <= 80) {
					initstick(size);
					change_screen(SCREEN_PLAY);
				} else {
					text_input_hint = "Must be 1 - 80";
				}
			}
		} else if (input == CHAR_BACKSPACE) {
			text_input_backspace();
		} else if (input == CHAR_ESC) {
			change_screen(SCREEN_MAIN_MENU);
		}
		break;

	case SCREEN_PLAY:
		old_number_active_i = number_active_i;
		switch(input) {
		case CHAR_LEFT:
			number_active_i--;
			if (number_active_i < 0) number_active_i = number_len - 1;
			if (number_swap_mode) {
				swap(&numbers[old_number_active_i], &numbers[number_active_i]);
				swap_count++;
			} else {
				scan_count++;
			}
			break;
		case CHAR_RIGHT:
			number_active_i++;
			if (number_active_i >= number_len) number_active_i = 0;
			if (number_swap_mode) {
				swap(&numbers[old_number_active_i], &numbers[number_active_i]);
				swap_count++;
			} else {
				scan_count++;
			}
			break;
		case CHAR_SPACE:
			number_swap_mode = !number_swap_mode;
			break;
		case CHAR_ESC:
			change_screen(SCREEN_PAUSE);
			break;
		}
		break;

	case SCREEN_PAUSE:
		switch(input) {
		case CHAR_DOWN:
			menu_down();
			break;
		case CHAR_UP:
			menu_up();
			break;
		case CHAR_ENTER:
			if (menu_activei == 0) {
				change_screen(SCREEN_PLAY);
			} else if (menu_activei == 1) {
				is_playing = 0;
				change_screen(SCREEN_INPUT_SIZE);
			} else if (menu_activei == 2) {
				is_playing = 0;
				change_screen(SCREEN_MAIN_MENU);
			}
			break;
		case CHAR_ESC:
			change_screen(SCREEN_PLAY);
			break;
		}
		break;

	case SCREEN_HOW_TO_PLAY:
	case SCREEN_CONGRATULATION:
		if (input == CHAR_ENTER) {
			change_screen(SCREEN_MAIN_MENU);
		}
		break;
	}

	HAL_UART_Receive_IT(&huart2, &input, sizeof(input));
}
/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  /* USER CODE BEGIN 5 */
	HAL_UART_Receive_IT(&huart2, &input, sizeof(input));
	/* Infinite loop */
	for (;;) {
		resetscreen();

		// Check for play sorted
		if (is_playing) {
			int is_sorted = 1;
			for(int i = 0;i<number_len-1;i++) {
				if (numbers[i] > numbers[i+1]) {
					is_sorted = 0;
					break;
				}
			}

			if (is_sorted) {
				change_screen(SCREEN_CONGRATULATION);
			}
		}

		switch(curr_screen) {
		case SCREEN_MAIN_MENU:
			fillmenu(MAIN_MENU, 2, menu_activei);
			break;
		case SCREEN_INPUT_SIZE:
			render_text_input_screen("Input array size (Maximum 80)");
			break;
		case SCREEN_PLAY:
			drawstick();
			break;
		case SCREEN_PAUSE:
			fillmenu(PAUSE_MENU, 3, menu_activei);
			break;
		case SCREEN_HOW_TO_PLAY:
			render_how_to_play();
			break;
		case SCREEN_CONGRATULATION:
			render_congratulation();
			break;
		}

		int not_used = rand();

		printscreen();
		osDelay(200);
	}
  /* USER CODE END 5 */ 
}

/* secondTickCallback function */
void secondTickCallback(void const * argument)
{
  /* USER CODE BEGIN secondTickCallback */
	curr_time++;
  /* USER CODE END secondTickCallback */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */

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
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
