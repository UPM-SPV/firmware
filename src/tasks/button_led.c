#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

#include "ble/advertise.h"
#include "mpsl/mpsl.h"

LOG_MODULE_REGISTER(button_led, LOG_LEVEL_DBG);

#define BTN_PRESSED_SLEEP_MS 	CONFIG_SPV_UX_BTN_PRESSED_SLEEP_MS

#define PAIRING_THRESHOLD_MS	 CONFIG_SPV_UX_BTN_PAIRING_THRESHOLD_START_MS
#define PAIRING_MAX_THRESHOLD_MS CONFIG_SPV_UX_BTN_PAIRING_THRESHOLD_CANCEL_MS
#define PAIRING_DURATION_MS		 CONFIG_SPV_UX_PAIRING_DURATION_MS
#define PAIRING_BLINK_MS		 CONFIG_SPV_UX_LED_PAIRING_BLINK_MS

#define RESET_THRESHOLD_START_MS	CONFIG_SPV_UX_BTN_RESET_THRESHOLD_START_MS
#define RESET_THRESHOLD_END_MS 		CONFIG_SPV_UX_BTN_RESET_THRESHOLD_END_MS
#define RESET_POST_WAIT_MS			CONFIG_SPV_UX_RESET_POST_WAIT_MS
#define RESET_BLINK_MS				CONFIG_SPV_UX_LED_RESET_BLINK_INITIAL_MS
#define RESET_BLINK_DECREMENT_DIV	CONFIG_SPV_UX_LED_RESET_BLINK_DECREMENT_DIV



#define LED_NODE DT_ALIAS(usr_led)
#define BUTTON_NODE DT_ALIAS(usr_button)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);

static void button_task(void*, void*, void*);
static void pairing_task(void*, void*, void*);
static void reset_task(void*, void*, void*);
static void on_radio_notify();
static K_THREAD_STACK_DEFINE(button_thread_stack, 1024);
static K_THREAD_STACK_DEFINE(pairing_thread_stack, 1024);
static K_THREAD_STACK_DEFINE(reset_thread_stack, 1024);
static struct k_thread button_thread_data;
static struct k_thread pairing_thread_data;
static struct k_thread reset_thread_data;

#define BIT_BTN_WAKE	BIT(0)
static K_EVENT_DEFINE(button_wake_event);
#define BIT_BTN_INIT		BIT(0)
#define BIT_BTN_PRESSED		BIT(1)
#define BIT_BTN_RELEASED	BIT(2)
static K_EVENT_DEFINE(button_event);

static K_MUTEX_DEFINE(button_mutex);

MPSL_CB_DEF(button_led_radio_cb) = {
	.on_active = on_radio_notify
};


int button_led_task_start() {
	k_tid_t id;

	id = k_thread_create(
		&button_thread_data, button_thread_stack,
		K_THREAD_STACK_SIZEOF(button_thread_stack),
		button_task,
		NULL, NULL, NULL,
		14, 0, K_NO_WAIT
	);
	k_thread_name_set(id, "button_led polling");

	id = k_thread_create(
		&pairing_thread_data, pairing_thread_stack,
		K_THREAD_STACK_SIZEOF(pairing_thread_stack),
		pairing_task,
		NULL, NULL, NULL,
		14, 0, K_NO_WAIT
	);
	k_thread_name_set(id, "button_led pairing");

	id = k_thread_create(
		&reset_thread_data, reset_thread_stack,
		K_THREAD_STACK_SIZEOF(reset_thread_stack),
		reset_task,
		NULL, NULL, NULL,
		14, 0, K_NO_WAIT
	);
	k_thread_name_set(id, "button_led reset");

	return 0;
}


/**
 * @brief Short for k_event_wait.
 */
static int32_t wait_btn(uint32_t events, uint64_t timeout_ms) {
	return k_event_wait(&button_event, events, 0, K_MSEC(timeout_ms));
}

static int32_t wait_btn_forever(uint32_t events) {
	return k_event_wait(&button_event, events, 0, K_FOREVER);
}

static void on_radio_notify() {
	k_event_set(&button_wake_event, BIT_BTN_WAKE);
}


/**
 * Initializes the button and led, and emulates button interrupts.
 *
 * TODO: Rewrite when the button is reasigned to an interruptible pin.
 */
static void button_task(void*, void*, void*) {
	int err;

	if(!gpio_is_ready_dt(&led) || !gpio_is_ready_dt(&button)) {
		LOG_ERR("Button and LED GPIO are not ready.");
		k_panic();
	}

	err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	err |= gpio_pin_configure_dt(&button, (GPIO_INPUT | GPIO_PULL_UP));
	if(err) {
		LOG_ERR("Error configuring button and LED GPIO");
		k_panic();
	}

	k_event_set(&button_event, BIT_BTN_INIT | BIT_BTN_RELEASED);
	gpio_pin_set_dt(&led, 1);
	k_msleep(1000);
	gpio_pin_set_dt(&led, 0);

	bool last_button_pressed = false;

	while(true) {
		bool current_button_pressed = gpio_pin_get_dt(&button);

		switch(current_button_pressed) {
			case true:
				if(!last_button_pressed) {
					k_event_set(&button_event, BIT_BTN_INIT | BIT_BTN_PRESSED);
				}
				k_msleep(BTN_PRESSED_SLEEP_MS);
				break;


			case false:
				if(last_button_pressed) {
					k_event_set(&button_event, BIT_BTN_INIT | BIT_BTN_RELEASED);
				}
				k_event_wait(&button_wake_event, BIT_BTN_WAKE, true, K_FOREVER);
				break;
		}

		last_button_pressed = current_button_pressed;
	}
}

static void pairing_task(void*, void*, void*) {
	wait_btn_forever(BIT_BTN_INIT);

	uint32_t events = 0;
	while(true) {
		// Wait until button is pressed.
		wait_btn_forever(BIT_BTN_PRESSED);

		events = wait_btn(BIT_BTN_RELEASED, PAIRING_THRESHOLD_MS);
		// If button has been released before the threshold, ignore the press.
		if(events) {
			continue;
		}
		// Advise the user to release the button if they wish to pair.
		gpio_pin_set_dt(&led, 1);

		// If the button was not released early enough, ignore the press.
		events = wait_btn(BIT_BTN_RELEASED, PAIRING_MAX_THRESHOLD_MS - PAIRING_THRESHOLD_MS);
		if(!events) {
			// Inform the user that pairing will not occur.
			gpio_pin_set_dt(&led, 0);
			wait_btn_forever(BIT_BTN_RELEASED);
			continue;
		}

		// Enable pairing.
		// Don't do anything if another operation is ongoing (reset).
		int result = k_mutex_lock(&button_mutex, K_NO_WAIT);
		if(!result) {
			ble_enable_pairing();
			int64_t time_left = PAIRING_DURATION_MS;

			while(time_left > 0) {
				gpio_pin_toggle_dt(&led);
				k_msleep(PAIRING_BLINK_MS);
				time_left -= PAIRING_BLINK_MS;
			}

			gpio_pin_set_dt(&led, 0);
			ble_disable_pairing();
			k_mutex_unlock(&button_mutex);
		}
	}
}

static void reset_task(void*, void*, void*) {
	wait_btn_forever(BIT_BTN_INIT);

	uint32_t events = 0;
	while(true) {
		// Wait until button is pressed.
		wait_btn_forever(BIT_BTN_PRESSED);

		events = wait_btn(BIT_BTN_RELEASED, RESET_THRESHOLD_START_MS);
		// If button has been released before the threshold, ignore the press.
		if(events) {
			continue;
		}

		// Start a reset. If the button is released early, cancel it.
		// Don't do anything if another operation is ongoing (pairing).
		int result = k_mutex_lock(&button_mutex, K_NO_WAIT);
		if(!result) {
			int64_t time_left = RESET_THRESHOLD_END_MS - RESET_THRESHOLD_START_MS;
			int64_t blink_period = RESET_BLINK_MS;

			// Set the LED to blink quicker and quicker as the timer runs out.
			// If the user releases the button early, abort the reset.
			while(time_left > 0) {
				gpio_pin_toggle_dt(&led);
				events = wait_btn(BIT_BTN_RELEASED, blink_period);
				if(events) {
					break;
				}

				blink_period -= blink_period / RESET_BLINK_DECREMENT_DIV;
				time_left -= blink_period;

				if(blink_period <= 16) {
					time_left = 0;
				}
			}

			if(time_left > 0) {
				gpio_pin_set_dt(&led, 0);
				k_mutex_unlock(&button_mutex);
				continue;
			}

			gpio_pin_set_dt(&led, 1);

			// Reset BLE bonds.
			bt_unpair(BT_ID_DEFAULT, BT_ADDR_LE_ANY);

			k_msleep(RESET_POST_WAIT_MS);
			k_mutex_unlock(&button_mutex);

			gpio_pin_set_dt(&led, 0);
			sys_reboot(SYS_REBOOT_COLD);
			sys_reboot(SYS_REBOOT_WARM);
			LOG_WRN("Error rebooting (?)");
		}
	}
}
