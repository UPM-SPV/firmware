/*!
 * @file button_led.h
 * @author Gabriel Gutiérrez Fuentes (me@gab.gf)
 * @brief Interactions with the button+led task.
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef SPV_TASKS_BUTTON_LED_H_
#define SPV_TASKS_BUTTON_LED_H_

/**
 * @brief Starts the button+led thread.
 *
 * @return int 0 if successful.
 */
[[nodiscard]]
int button_led_task_start();

#endif
