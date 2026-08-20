/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
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
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define WBUS_RX_BUFFER_SIZE          32U
#define WBUS_FRAME_SIZE              25U
#define WBUS_EXPECTED_START_BYTE     0x0FU
#define WBUS_EXPECTED_END_BYTE       0x00U
#define WBUS_FLAGS_INDEX             23U
#define WBUS_FLAG_FRAME_LOST         0x04U
#define WBUS_FLAG_FAILSAFE           0x08U
#define WBUS_LINK_TIMEOUT_MS         100U

#define DRIVE_CONTROL_PERIOD_MS      10U
#define DRIVE_STICK_CENTER           1024
#define DRIVE_STICK_DEADZONE         80
#define DRIVE_STICK_SPAN             670
#define DRIVE_COMMAND_FULL_SCALE     1000
#define DRIVE_SLEW_STEP              5
#define DRIVE_MAX_DUTY_PERCENT       50U
#define DRIVE_ENABLE_THRESHOLD       1024U
#define DRIVE_THROTTLE_CHANNEL       2U
#define DRIVE_TURN_CHANNEL           0U
#define DRIVE_ENABLE_CHANNEL         4U
#define DRIVE_LEFT_MOTOR_POLARITY    1
#define DRIVE_RIGHT_MOTOR_POLARITY   (-1)

#define ENCODER_SPEED_SAMPLE_MS      100U

#define DEBUG_UART_PERIOD_MS         100U
#define DEBUG_UART_HEADER_INTERVAL   50U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static uint8_t wbus_dma_buffer[WBUS_RX_BUFFER_SIZE];

/*
 * These volatile variables are intentionally easy to inspect in a debugger.
 * W.BUS input, safety state, and drive outputs remain visible to the debugger.
 */
volatile uint8_t wbus_last_frame[WBUS_RX_BUFFER_SIZE];
volatile uint16_t wbus_last_length = 0U;
volatile uint8_t wbus_first_byte = 0U;
volatile uint8_t wbus_last_byte = 0U;
volatile uint8_t wbus_header_ok = 0U;
volatile uint8_t wbus_candidate_frame = 0U;
volatile uint8_t wbus_frame_valid = 0U;
volatile uint8_t wbus_flags = 0U;
volatile uint8_t wbus_frame_lost = 0U;
volatile uint8_t wbus_failsafe = 0U;
volatile uint8_t wbus_digital_channel_17 = 0U;
volatile uint8_t wbus_digital_channel_18 = 0U;
volatile uint8_t wbus_link_alive = 0U;
volatile uint8_t wbus_rx_needs_restart = 0U;
volatile uint16_t wbus_channels[16];
volatile uint32_t wbus_last_tick = 0U;
volatile uint32_t wbus_rx_event_count = 0U;
volatile uint32_t wbus_valid_frame_count = 0U;
volatile uint32_t wbus_invalid_event_count = 0U;
volatile uint32_t wbus_uart_error_count = 0U;
volatile uint32_t wbus_rx_restart_error_count = 0U;
volatile uint32_t wbus_last_uart_error = HAL_UART_ERROR_NONE;

/*
 * Drive diagnostic values.  Command range is -1000..1000.  PWM output is
 * additionally limited to DRIVE_MAX_DUTY_PERCENT for the first track test.
 */
volatile int16_t drive_throttle_command = 0;
volatile int16_t drive_turn_command = 0;
volatile int16_t drive_left_target = 0;
volatile int16_t drive_right_target = 0;
volatile int16_t drive_left_applied = 0;
volatile int16_t drive_right_applied = 0;
volatile int16_t drive_left_output_command = 0;
volatile int16_t drive_right_output_command = 0;
volatile uint16_t drive_left_pwm = 0U;
volatile uint16_t drive_right_pwm = 0U;
volatile uint8_t drive_link_ok = 0U;
volatile uint8_t drive_enable_requested = 0U;
volatile uint8_t drive_enable_seen_low = 0U;
volatile uint8_t drive_previous_enable_request = 0U;
volatile uint8_t drive_armed = 0U;

/*
 * Left quadrature encoder diagnostics.  Position is in x4 edge counts.
 * Conversion to shaft RPM will be added after the encoder counts per
 * revolution are measured or confirmed from the motor specification.
 */
volatile int32_t left_encoder_position = 0;
volatile int32_t left_encoder_delta_100ms = 0;
volatile int32_t left_encoder_counts_per_second = 0;
volatile uint32_t left_encoder_edge_count = 0U;
volatile uint32_t left_encoder_invalid_transition_count = 0U;
volatile uint8_t left_encoder_a_level = 0U;
volatile uint8_t left_encoder_b_level = 0U;
volatile uint8_t left_encoder_previous_state = 0U;

static char debug_uart_buffer[192];
static uint8_t debug_uart_header_countdown = 0U;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static HAL_StatusTypeDef wbus_start_receive(void);
static void wbus_decode_channels(void);
static void wbus_store_frame(uint16_t size);
static int16_t drive_normalize_stick(uint16_t raw);
static int16_t drive_clamp_command(int32_t command);
static int16_t drive_slew(int16_t current, int16_t target);
static void drive_force_stop(void);
static void drive_write_motor(TIM_HandleTypeDef *timer,
                              uint32_t rpwm_channel,
                              uint32_t lpwm_channel,
                              int16_t command,
                              volatile uint16_t *pwm_monitor);
static void drive_control_update(void);
static void left_encoder_init(void);
static void left_encoder_update_from_edge(void);
static void left_encoder_update_speed(void);
static void debug_uart_send_snapshot(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static HAL_StatusTypeDef wbus_start_receive(void)
{
  HAL_StatusTypeDef status;

  status = HAL_UARTEx_ReceiveToIdle_DMA(&huart3,
                                        wbus_dma_buffer,
                                        WBUS_RX_BUFFER_SIZE);
  if ((status == HAL_OK) && (huart3.hdmarx != NULL))
  {
    /*
     * An S.BUS/W.BUS frame is shorter than this buffer.  The USART idle event
     * marks the frame boundary, so a half-transfer interrupt is unnecessary.
     */
    __HAL_DMA_DISABLE_IT(huart3.hdmarx, DMA_IT_HT);
  }

  return status;
}

static void wbus_decode_channels(void)
{
  uint8_t channel;

  for (channel = 0U; channel < 16U; ++channel)
  {
    uint16_t bit_index = (uint16_t)channel * 11U;
    uint16_t byte_index = 1U + (bit_index / 8U);
    uint8_t bit_offset = (uint8_t)(bit_index % 8U);
    uint32_t packed;

    packed = (uint32_t)wbus_last_frame[byte_index];
    packed |= (uint32_t)wbus_last_frame[byte_index + 1U] << 8U;
    if ((byte_index + 2U) < WBUS_FLAGS_INDEX)
    {
      packed |= (uint32_t)wbus_last_frame[byte_index + 2U] << 16U;
    }

    wbus_channels[channel] = (uint16_t)((packed >> bit_offset) & 0x07FFU);
  }
}

static void wbus_store_frame(uint16_t size)
{
  uint16_t copy_size;
  uint16_t index;

  copy_size = size;
  if (copy_size > WBUS_RX_BUFFER_SIZE)
  {
    copy_size = WBUS_RX_BUFFER_SIZE;
  }

  for (index = 0U; index < copy_size; ++index)
  {
    wbus_last_frame[index] = wbus_dma_buffer[index];
  }

  for (; index < WBUS_RX_BUFFER_SIZE; ++index)
  {
    wbus_last_frame[index] = 0U;
  }

  wbus_last_length = size;
  wbus_first_byte = (copy_size > 0U) ? wbus_last_frame[0] : 0U;
  wbus_last_byte = (copy_size > 0U) ? wbus_last_frame[copy_size - 1U] : 0U;
  wbus_header_ok = (wbus_first_byte == WBUS_EXPECTED_START_BYTE) ? 1U : 0U;
  wbus_candidate_frame =
      ((wbus_header_ok != 0U) &&
       (size == WBUS_FRAME_SIZE) &&
       (wbus_last_byte == WBUS_EXPECTED_END_BYTE)) ? 1U : 0U;

  if (wbus_candidate_frame != 0U)
  {
    wbus_flags = wbus_last_frame[WBUS_FLAGS_INDEX];
    wbus_frame_lost =
        ((wbus_flags & WBUS_FLAG_FRAME_LOST) != 0U) ? 1U : 0U;
    wbus_failsafe =
        ((wbus_flags & WBUS_FLAG_FAILSAFE) != 0U) ? 1U : 0U;
    wbus_digital_channel_17 = (wbus_flags & 0x01U) ? 1U : 0U;
    wbus_digital_channel_18 = (wbus_flags & 0x02U) ? 1U : 0U;

    wbus_decode_channels();

    if ((wbus_frame_lost == 0U) && (wbus_failsafe == 0U))
    {
      wbus_frame_valid = 1U;
      wbus_link_alive = 1U;
      ++wbus_valid_frame_count;
    }
    else
    {
      /*
       * A complete frame carrying a real receiver failsafe/lost-frame flag
       * must stop the drive immediately.
       */
      wbus_frame_valid = 0U;
      wbus_link_alive = 0U;
    }

    wbus_last_tick = HAL_GetTick();
  }
  else
  {
    /*
     * UART idle detection can occasionally split a frame.  Ignore that
     * incomplete event and let the 100 ms valid-frame timeout decide whether
     * the receiver is actually offline.
     */
    ++wbus_invalid_event_count;
  }

  ++wbus_rx_event_count;
}

static int16_t drive_normalize_stick(uint16_t raw)
{
  int32_t delta = (int32_t)raw - DRIVE_STICK_CENTER;
  int32_t magnitude;
  int32_t normalized;

  if ((delta >= -DRIVE_STICK_DEADZONE) &&
      (delta <= DRIVE_STICK_DEADZONE))
  {
    return 0;
  }

  if (delta > 0)
  {
    magnitude = delta - DRIVE_STICK_DEADZONE;
  }
  else
  {
    magnitude = -delta - DRIVE_STICK_DEADZONE;
  }

  normalized =
      (magnitude * DRIVE_COMMAND_FULL_SCALE) /
      (DRIVE_STICK_SPAN - DRIVE_STICK_DEADZONE);
  if (normalized > DRIVE_COMMAND_FULL_SCALE)
  {
    normalized = DRIVE_COMMAND_FULL_SCALE;
  }

  return (delta > 0) ? (int16_t)normalized : (int16_t)-normalized;
}

static int16_t drive_clamp_command(int32_t command)
{
  if (command > DRIVE_COMMAND_FULL_SCALE)
  {
    return DRIVE_COMMAND_FULL_SCALE;
  }
  if (command < -DRIVE_COMMAND_FULL_SCALE)
  {
    return -DRIVE_COMMAND_FULL_SCALE;
  }
  return (int16_t)command;
}

static int16_t drive_slew(int16_t current, int16_t target)
{
  int32_t difference;

  /* A direction change must ramp to zero before the opposite PWM is used. */
  if (((current > 0) && (target < 0)) ||
      ((current < 0) && (target > 0)))
  {
    target = 0;
  }

  difference = (int32_t)target - current;

  if (difference > DRIVE_SLEW_STEP)
  {
    return (int16_t)(current + DRIVE_SLEW_STEP);
  }
  if (difference < -DRIVE_SLEW_STEP)
  {
    return (int16_t)(current - DRIVE_SLEW_STEP);
  }
  return target;
}

static void drive_force_stop(void)
{
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0U);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 0U);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, 0U);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, 0U);
  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, 0U);
  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2, 0U);

  drive_left_target = 0;
  drive_right_target = 0;
  drive_left_applied = 0;
  drive_right_applied = 0;
  drive_left_output_command = 0;
  drive_right_output_command = 0;
  drive_left_pwm = 0U;
  drive_right_pwm = 0U;
}

static void drive_write_motor(TIM_HandleTypeDef *timer,
                              uint32_t rpwm_channel,
                              uint32_t lpwm_channel,
                              int16_t command,
                              volatile uint16_t *pwm_monitor)
{
  uint32_t magnitude;
  uint32_t period_counts;
  uint32_t pulse;

  magnitude = (command >= 0) ? (uint32_t)command : (uint32_t)-command;
  period_counts = __HAL_TIM_GET_AUTORELOAD(timer) + 1U;
  pulse = (period_counts * magnitude * DRIVE_MAX_DUTY_PERCENT) /
          (DRIVE_COMMAND_FULL_SCALE * 100U);

  if (command > 0)
  {
    /* Forward: LPWM is forced low before RPWM is enabled. */
    __HAL_TIM_SET_COMPARE(timer, lpwm_channel, 0U);
    __HAL_TIM_SET_COMPARE(timer, rpwm_channel, pulse);
  }
  else if (command < 0)
  {
    /* Reverse: RPWM is forced low before LPWM is enabled. */
    __HAL_TIM_SET_COMPARE(timer, rpwm_channel, 0U);
    __HAL_TIM_SET_COMPARE(timer, lpwm_channel, pulse);
  }
  else
  {
    __HAL_TIM_SET_COMPARE(timer, rpwm_channel, 0U);
    __HAL_TIM_SET_COMPARE(timer, lpwm_channel, 0U);
  }

  *pwm_monitor = (uint16_t)pulse;
}

static void drive_control_update(void)
{
  uint32_t now;
  uint32_t last_frame_tick;
  uint32_t interrupt_state;
  uint16_t throttle_raw;
  uint16_t turn_raw;
  uint16_t enable_raw;
  uint8_t frame_valid;
  uint8_t enable_request;
  int16_t throttle;
  int16_t turn;

  now = HAL_GetTick();

  interrupt_state = __get_PRIMASK();
  __disable_irq();
  last_frame_tick = wbus_last_tick;
  frame_valid = wbus_frame_valid;
  throttle_raw = wbus_channels[DRIVE_THROTTLE_CHANNEL];
  turn_raw = wbus_channels[DRIVE_TURN_CHANNEL];
  enable_raw = wbus_channels[DRIVE_ENABLE_CHANNEL];
  if (interrupt_state == 0U)
  {
    __enable_irq();
  }

  drive_link_ok =
      ((frame_valid != 0U) &&
       ((now - last_frame_tick) <= WBUS_LINK_TIMEOUT_MS)) ? 1U : 0U;
  enable_request =
      ((drive_link_ok != 0U) &&
       (enable_raw > DRIVE_ENABLE_THRESHOLD)) ? 1U : 0U;
  drive_enable_requested = enable_request;

  throttle = drive_normalize_stick(throttle_raw);
  turn = drive_normalize_stick(turn_raw);
  drive_throttle_command = throttle;
  drive_turn_command = turn;

  if (drive_link_ok == 0U)
  {
    drive_armed = 0U;
    drive_enable_seen_low = 0U;
    drive_previous_enable_request = 0U;
    drive_force_stop();
    return;
  }

  if (enable_request == 0U)
  {
    drive_armed = 0U;
    drive_enable_seen_low = 1U;
    drive_previous_enable_request = 0U;
    drive_force_stop();
    return;
  }

  /*
   * Arming is accepted on the SA-up to SA-down transition, regardless of
   * stick position.  SA-up always disarms and forces both motor outputs low.
   */
  if (drive_armed == 0U)
  {
    if ((drive_enable_seen_low != 0U) &&
        (drive_previous_enable_request == 0U))
    {
      drive_armed = 1U;
    }
    else
    {
      drive_previous_enable_request = 1U;
      drive_force_stop();
      return;
    }
  }

  drive_previous_enable_request = 1U;
  /* Differential tracked-vehicle mixing. */
  drive_left_target =
      drive_clamp_command((int32_t)throttle + (int32_t)turn);
  drive_right_target =
      drive_clamp_command((int32_t)throttle - (int32_t)turn);

  drive_left_applied =
      drive_slew(drive_left_applied, drive_left_target);
  drive_right_applied =
      drive_slew(drive_right_applied, drive_right_target);

  /*
   * Reversed vehicle-forward wiring: left C3, right C6.
   */
  drive_left_output_command =
      (int16_t)(drive_left_applied * DRIVE_LEFT_MOTOR_POLARITY);
  drive_right_output_command =
      (int16_t)(drive_right_applied * DRIVE_RIGHT_MOTOR_POLARITY);

  drive_write_motor(&htim1,
                    TIM_CHANNEL_3,
                    TIM_CHANNEL_4,
                    drive_left_output_command,
                    &drive_left_pwm);

  drive_write_motor(&htim8,
                    TIM_CHANNEL_1,
                    TIM_CHANNEL_2,
                    drive_right_output_command,
                    &drive_right_pwm);
}

static void left_encoder_init(void)
{
  uint8_t state;

  left_encoder_a_level =
      (HAL_GPIO_ReadPin(LEFT_ENCODER_A_GPIO_Port,
                        LEFT_ENCODER_A_Pin) == GPIO_PIN_SET) ? 1U : 0U;
  left_encoder_b_level =
      (HAL_GPIO_ReadPin(LEFT_ENCODER_B_GPIO_Port,
                        LEFT_ENCODER_B_Pin) == GPIO_PIN_SET) ? 1U : 0U;
  state = (uint8_t)((left_encoder_a_level << 1U) |
                    left_encoder_b_level);
  left_encoder_previous_state = state;
}

static void left_encoder_update_from_edge(void)
{
  /*
   * Quadrature transition table indexed by:
   *   previous A/B state (two high bits) + current A/B state (two low bits)
   */
  static const int8_t transition_delta[16] =
  {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
  };
  uint8_t current_state;
  uint8_t transition;
  int8_t delta;

  left_encoder_a_level =
      (HAL_GPIO_ReadPin(LEFT_ENCODER_A_GPIO_Port,
                        LEFT_ENCODER_A_Pin) == GPIO_PIN_SET) ? 1U : 0U;
  left_encoder_b_level =
      (HAL_GPIO_ReadPin(LEFT_ENCODER_B_GPIO_Port,
                        LEFT_ENCODER_B_Pin) == GPIO_PIN_SET) ? 1U : 0U;
  current_state = (uint8_t)((left_encoder_a_level << 1U) |
                            left_encoder_b_level);
  transition = (uint8_t)((left_encoder_previous_state << 2U) |
                         current_state);
  delta = transition_delta[transition];

  if (delta != 0)
  {
    left_encoder_position += delta;
  }
  else if (current_state != left_encoder_previous_state)
  {
    ++left_encoder_invalid_transition_count;
  }

  left_encoder_previous_state = current_state;
  ++left_encoder_edge_count;
}

static void left_encoder_update_speed(void)
{
  static int32_t previous_position = 0;
  int32_t current_position;

  current_position = left_encoder_position;
  left_encoder_delta_100ms = current_position - previous_position;
  left_encoder_counts_per_second =
      left_encoder_delta_100ms * (1000 / ENCODER_SPEED_SAMPLE_MS);
  previous_position = current_position;
}

static void debug_uart_send_snapshot(void)
{
  static const char header[] =
      "ms,thr_raw,turn_raw,sa_raw,link,enable,armed,thr_cmd,left_cmd,"
      "rpwm,lpwm,enc_pos,enc_cps,enc_invalid,"
      "right_cmd,right_rpwm,right_lpwm\r\n";
  uint32_t interrupt_state;
  uint32_t now;
  uint16_t throttle_raw;
  uint16_t turn_raw;
  uint16_t enable_raw;
  uint8_t link_ok;
  uint8_t enable_requested;
  uint8_t armed;
  int16_t throttle_command;
  int16_t left_output_command;
  int16_t right_output_command;
  uint32_t rpwm;
  uint32_t lpwm;
  uint32_t right_rpwm;
  uint32_t right_lpwm;
  int32_t encoder_position;
  int32_t encoder_counts_per_second;
  uint32_t encoder_invalid;
  int length;

  if (debug_uart_header_countdown == 0U)
  {
    (void)HAL_UART_Transmit(&huart6,
                            (uint8_t *)header,
                            (uint16_t)(sizeof(header) - 1U),
                            20U);
    debug_uart_header_countdown = DEBUG_UART_HEADER_INTERVAL;
  }
  --debug_uart_header_countdown;

  now = HAL_GetTick();
  interrupt_state = __get_PRIMASK();
  __disable_irq();
  throttle_raw = wbus_channels[DRIVE_THROTTLE_CHANNEL];
  turn_raw = wbus_channels[DRIVE_TURN_CHANNEL];
  enable_raw = wbus_channels[DRIVE_ENABLE_CHANNEL];
  link_ok = drive_link_ok;
  enable_requested = drive_enable_requested;
  armed = drive_armed;
  throttle_command = drive_throttle_command;
  left_output_command = drive_left_output_command;
  right_output_command = drive_right_output_command;
  encoder_position = left_encoder_position;
  encoder_counts_per_second = left_encoder_counts_per_second;
  encoder_invalid = left_encoder_invalid_transition_count;
  if (interrupt_state == 0U)
  {
    __enable_irq();
  }

  rpwm = __HAL_TIM_GET_COMPARE(&htim1, TIM_CHANNEL_3);
  lpwm = __HAL_TIM_GET_COMPARE(&htim1, TIM_CHANNEL_4);
  right_rpwm = __HAL_TIM_GET_COMPARE(&htim8, TIM_CHANNEL_1);
  right_lpwm = __HAL_TIM_GET_COMPARE(&htim8, TIM_CHANNEL_2);

  length = snprintf(debug_uart_buffer,
                    sizeof(debug_uart_buffer),
                    "%lu,%u,%u,%u,%u,%u,%u,%d,%d,%lu,%lu,%ld,%ld,%lu,%d,%lu,%lu\r\n",
                    (unsigned long)now,
                    (unsigned int)throttle_raw,
                    (unsigned int)turn_raw,
                    (unsigned int)enable_raw,
                    (unsigned int)link_ok,
                    (unsigned int)enable_requested,
                    (unsigned int)armed,
                    (int)throttle_command,
                    (int)left_output_command,
                    (unsigned long)rpwm,
                    (unsigned long)lpwm,
                    (long)encoder_position,
                    (long)encoder_counts_per_second,
                    (unsigned long)encoder_invalid,
                    (int)right_output_command,
                    (unsigned long)right_rpwm,
                    (unsigned long)right_lpwm);

  if ((length > 0) && ((size_t)length < sizeof(debug_uart_buffer)))
  {
    (void)HAL_UART_Transmit(&huart6,
                            (uint8_t *)debug_uart_buffer,
                            (uint16_t)length,
                            20U);
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
  MX_DMA_Init();
  MX_USART3_UART_Init();
  MX_TIM8_Init();
  MX_TIM1_Init();
  MX_USART6_UART_Init();
  /* USER CODE BEGIN 2 */
  /*
   * Safety: all motor PWM compare values start at zero.  PWM channels run,
   * but drive_control_update keeps them low until W.BUS and SA are valid.
   */
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0U);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 0U);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, 0U);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, 0U);

  /*
   * C5/C6 use TIM8_CH1/CH2.  CubeMX originally configured TIM8 for 50 Hz
   * servo output; override it here to match TIM1's 20 kHz motor PWM.
   */
  htim8.Instance->PSC = 0U;
  htim8.Instance->ARR = 8399U;
  htim8.Instance->CNT = 0U;
  htim8.Instance->EGR = TIM_EGR_UG;
  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, 0U);
  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2, 0U);
  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, 0U);

  if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }

  left_encoder_init();

  if (wbus_start_receive() != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t drive_last_control_tick = HAL_GetTick();
  uint32_t encoder_last_speed_tick = HAL_GetTick();
  uint32_t debug_uart_last_tick = HAL_GetTick();

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if ((wbus_link_alive != 0U) &&
        ((HAL_GetTick() - wbus_last_tick) > WBUS_LINK_TIMEOUT_MS))
    {
      wbus_link_alive = 0U;
    }

    if (wbus_rx_needs_restart != 0U)
    {
      (void)HAL_UART_AbortReceive(&huart3);
      if (wbus_start_receive() == HAL_OK)
      {
        wbus_rx_needs_restart = 0U;
      }
      else
      {
        ++wbus_rx_restart_error_count;
      }
    }

    if ((HAL_GetTick() - drive_last_control_tick) >=
        DRIVE_CONTROL_PERIOD_MS)
    {
      drive_last_control_tick += DRIVE_CONTROL_PERIOD_MS;

      drive_control_update();
    }

    if ((HAL_GetTick() - encoder_last_speed_tick) >=
        ENCODER_SPEED_SAMPLE_MS)
    {
      encoder_last_speed_tick += ENCODER_SPEED_SAMPLE_MS;
      left_encoder_update_speed();
    }

    if ((HAL_GetTick() - debug_uart_last_tick) >= DEBUG_UART_PERIOD_MS)
    {
      debug_uart_last_tick += DEBUG_UART_PERIOD_MS;
      debug_uart_send_snapshot();
    }

    HAL_Delay(1U);
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 6;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t gpio_pin)
{
  if ((gpio_pin == LEFT_ENCODER_A_Pin) ||
      (gpio_pin == LEFT_ENCODER_B_Pin))
  {
    left_encoder_update_from_edge();
  }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
  if (huart->Instance == USART3)
  {
    wbus_store_frame(size);

    if (wbus_start_receive() != HAL_OK)
    {
      wbus_rx_needs_restart = 1U;
      ++wbus_rx_restart_error_count;
    }
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART3)
  {
    wbus_last_uart_error = HAL_UART_GetError(huart);
    ++wbus_uart_error_count;
    wbus_link_alive = 0U;
    wbus_rx_needs_restart = 1U;
    drive_armed = 0U;
    drive_force_stop();
  }
}

/* USER CODE END 4 */

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
#ifdef USE_FULL_ASSERT
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
