/*!
 * @file init.h
 * @author Gabriel Gutiérrez Fuentes (me@gab.gf)
 * @brief Initialization framework.
 *
 * @copyright Copyright (c) 2026
 */

#ifndef SPV_INIT_INIT_H_
#define SPV_INIT_INIT_H_

#include <zephyr/kernel.h>

/**
 * @brief Callback for initialization functions. See `INIT_CB_DEF`.
 */
typedef void (*init_cb_func_t)(void);

/**
 * @brief Struct for storing initialization callbacks. Should be declared using
 * `INIT_CB_DEF`.
 */
struct init_cb_t {
    /**
     * @brief Called when `init_start()` is called. May be *not* NULL.
     */
    const init_cb_func_t on_init;

	/**
	 * @brief NULL-terminated array of dependencies. The callback will only
	 * be called after all dependencies are met. May be NULL to specify no
	 * dependencies.
	 */
	const char * const *depends;

	/**
	 * @brief NULL-terminated array of dependencies this meets. Should be
	 * globally unique. May be NULL.
	 */
	const char * const *provides;
};

/**
 * @brief Subscribes the specified callback to initialization updates.
 *
 * @note Callbacks will only be called when their dependencies are met, but
 * should not assume any ordering.
 *
 * ## Example
 *
 @code
 ```
 * void my_init_function() {
 * 	printk("Init called!");
 * 	// Your init code here...
 * }
 *
 * INIT_CB_DEF(my_init) = {
 * 	.on_init	= my_init_function,
 * 	// Only called after a provider of "sensor_init" is called.
 * 	.depends	= (const char*[]){ "sensor_init", 0 },
 * 	.provides	= (const char*[]){ "my_init", 0 },
 * };
 *
 ```
 @endcode
 */
#define INIT_CB_DEF(_name) \
	STRUCT_SECTION_ITERABLE(init_cb_t, _name)


/**
 * @brief Calls all registered init callbacks.
 */
void init_start();

#endif
