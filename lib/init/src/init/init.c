#include "init/init.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(init, LOG_LEVEL_DBG);


static bool was_executed(const struct init_cb_t *executed_array[], struct init_cb_t *callback);
static bool all_dependencies_met(const char *providers[], const struct init_cb_t *callback);

static void update_providers(const char *providers[], const struct init_cb_t *callback);
static void update_executed(const struct init_cb_t *executed_array[], struct init_cb_t *callback);

#ifdef CONFIG_SPV_INIT_PRINT_DETAILED_ERROR
static void print_initialized_modules(const char *modules[]);
static void print_uninitialized_modules(
	const char *started_providers[],
	const struct init_cb_t *started_modules[],
	const struct init_cb_t all_modules[],
	size_t all_modules_count
);
#endif

extern const struct init_cb_t *spv_init_module_state[];
extern char spv_init_module_count[];

/*
 * Use a naïve iterative algorithm to run all callbacks respecting their
 * dependencies.
 */
void init_start() {
	LOG_INF("Starting init...");

	const char *started_providers[CONFIG_SPV_INIT_MAX_TRACKED_DEPENDENCIES + 1] = { 0 };
	// NULL-terminated.
	const struct init_cb_t **started = spv_init_module_state;

	[[maybe_unused]]
	size_t max_callback_count = (uint32_t) &spv_init_module_count;
	size_t callback_count;
	STRUCT_SECTION_COUNT(init_cb_t, &callback_count);
	__ASSERT(
		max_callback_count >= callback_count,
		"INTERNAL ERROR: too many init callbacks\n"
		"	Reserved linker region cannot fit requested init modules\n"
		"		Actual:		%zu\n"
		"		Reserved:	%zu",
		callback_count, max_callback_count
	);

	memset(started, 0, (callback_count + 1) * sizeof(*started));

	size_t callbacks_left = callback_count;
	while(callbacks_left) {
		size_t callbacks_executed_this_iteration = 0;

		STRUCT_SECTION_FOREACH(init_cb_t, cb) {
			if(was_executed(started, cb)) {
				continue;
			}
			if(!all_dependencies_met(started_providers, cb)) {
				continue;
			}

			cb->on_init();

			update_providers(started_providers, cb);
			update_executed(started, cb);

			callbacks_left--;
			callbacks_executed_this_iteration++;
		}

		if(callbacks_executed_this_iteration == 0) {

			LOG_ERR("Error initialization functions exist with unmeetable dependencies");
			#ifdef CONFIG_SPV_INIT_PRINT_DETAILED_ERROR
			const struct init_cb_t *all_callbacks = STRUCT_SECTION_START(init_cb_t);
			print_initialized_modules(started_providers);
			print_uninitialized_modules(
				started_providers,
				started,
				all_callbacks,
				callback_count
			);
			#endif
			k_panic();
		}
	}
	LOG_INF("All modules initialized");
}


static bool all_dependencies_met(const char *providers[], const struct init_cb_t *callback) {

	if(callback->depends == NULL) {
		return true;
	}

	size_t dependency_count = 0;
	for(const char * const *dependency = callback->depends; *dependency != NULL; dependency++) {
		dependency_count++;
	}

	size_t unmet_dependencies = dependency_count;
	for(const char **provider = providers; *provider != NULL && unmet_dependencies > 0; provider++) {
		for(const char * const *dependency = callback->depends; *dependency != NULL; dependency++) {
			if(strcmp(*dependency, *provider) == 0) {
				unmet_dependencies--;
				break;
			}
		}
	}

	return unmet_dependencies == 0;
}

static void update_providers(const char *providers[], const struct init_cb_t *callback) {

	if(callback->provides == NULL) {
		return;
	}

	// Count callback's `provides`.
	size_t new_providers = 0;
	for(const char * const *new_provider = callback->provides; *new_provider != NULL; new_provider++) {
		new_providers++;

		// Check for duplicates.
		for(const char **saved_provider = providers; *saved_provider != NULL; saved_provider++) {
			if(strcmp(*new_provider, *saved_provider) == 0) {
				LOG_ERR("Duplicate provider found for `%s`", *new_provider);
			}
		}
	}

	// Count currently saved providers and store the last one.
	size_t current_providers = 0;
	const char **last_provider;
	for(last_provider = providers; *last_provider != NULL; last_provider++) {
		current_providers++;
	}

	if(new_providers + current_providers > CONFIG_SPV_INIT_MAX_TRACKED_DEPENDENCIES) {
		LOG_ERR("Too many tracked dependencies.");
		LOG_ERR("	Maximum:	%zu", CONFIG_SPV_INIT_MAX_TRACKED_DEPENDENCIES);
		LOG_ERR("	Current:	%zu", current_providers + new_providers);
		LOG_ERR("Increase CONFIG_SPV_INIT_MAX_TRACKED_DEPENDENCIES");
		k_panic();
	}


	for(const char * const *provider = callback->provides; *provider != NULL; provider++, last_provider++) {
		*last_provider = *provider;
	}
}


static void update_executed(const struct init_cb_t *executed_array[], struct init_cb_t *callback) {

	const struct init_cb_t **last_executed;
	for(last_executed = executed_array; *last_executed != NULL; last_executed++);
	*last_executed = callback;
}

static bool was_executed(const struct init_cb_t *executed_array[], struct init_cb_t *callback) {
	for(const struct init_cb_t **executed = executed_array; *executed != NULL; executed++) {
		if(*executed == callback) {
			return true;
		}
	}

	return false;
}


/*
 * Unmet dependency printing.
 */

#ifdef CONFIG_SPV_INIT_PRINT_DETAILED_ERROR

static void print_initialized_modules(const char *modules[]) {
	LOG_ERR("Successfully started modules:");
	for(const char **current = modules; *current != NULL; current++) {
		LOG_ERR("	%s", *current);
	}
}

static const char* str_search(const char *needle, const char * const haystack[]) {
	for(const char * const*current = haystack; *current != NULL; current++) {
		if(strcmp(*current, needle) == 0) {
			return *current;
		}
	}

	return NULL;
}

static bool str_equals(const void *a, const void *b) {
	return strcmp((const char *)a, (const char *)b) == 0;
}

static bool module_equals(const void *a, const void *b) {
	return a == b;
}

static void append_element(
	const void *element_list[],
	const void *element,
	bool (*equals)(const void*, const void*)
) {
	size_t insert_index;
	for(
		insert_index = 0;

		element_list[insert_index] != NULL &&
		insert_index < CONFIG_SPV_INIT_MAX_TRACKED_DEPENDENCIES;

		insert_index++
	) {
		if(equals(element_list[insert_index], element)) {
			return;
		}
	}

	if(insert_index == CONFIG_SPV_INIT_MAX_TRACKED_DEPENDENCIES) {
		LOG_ERR("Too many tracked dependencies.");
		LOG_ERR("	Maximum:	%zu", CONFIG_SPV_INIT_MAX_TRACKED_DEPENDENCIES);
		LOG_ERR("	Current:	%zu", insert_index);
		LOG_ERR("Increase CONFIG_SPV_INIT_MAX_TRACKED_DEPENDENCIES");
		k_panic();
	}

	element_list[insert_index] = element;
}

static void print_uninitialized_modules(
	const char *started_providers[],
	const struct init_cb_t *started_modules[],
	const struct init_cb_t all_modules[],
	size_t all_modules_count
) {
	const char *uninitialized_dependencies[CONFIG_SPV_INIT_MAX_TRACKED_DEPENDENCIES + 1] = { 0 };
	const struct init_cb_t *uninitialized_modules[CONFIG_SPV_INIT_MAX_TRACKED_DEPENDENCIES + 1] = { 0 };

	for(size_t mod_index = 0; mod_index < all_modules_count; mod_index++) {
		const struct init_cb_t *module = &all_modules[mod_index];

		bool is_started = false;
		for(const struct init_cb_t **started_current = started_modules; *started_current != NULL; started_current++) {
			if(*started_current == module) {
				is_started = true;
				break;
			}
		}
		if(is_started) {
			continue;
		}
		append_element((const void **)uninitialized_modules, module, module_equals);

		if(module->provides == NULL) {
			continue;
		}
		for(const char * const *prov_current = module->provides; *prov_current != NULL; prov_current++) {
			if(!str_search(*prov_current, started_providers)) {
				append_element((const void**)uninitialized_dependencies, *prov_current, str_equals);
			}
		}
	}

	LOG_ERR("Uninitialized modules:");
	for(const char **dep_current = uninitialized_dependencies; *dep_current != NULL; dep_current++) {
		const char *dependency = *dep_current;
		LOG_ERR("	%s required by:", dependency);

		for(const struct init_cb_t **mod_current = uninitialized_modules; *mod_current != NULL; mod_current++) {
			const struct init_cb_t *module = *mod_current;
			if(module->depends == NULL) {
				continue;
			}

			if(str_search(dependency, module->depends)) {
				if(module->provides == NULL) {
					LOG_ERR("		(unnamed) %p", module);
				}
				for(const char * const *name = module->provides; *name != NULL; name++) {
					LOG_ERR("		%s", *name);
				}
			}
		}
	}
}

#endif
