#include "init/init.h"

#include <zephyr/toolchain.h>

void spv_init_gen_linker_symbols() {
	GEN_ABSOLUTE_SYM(SPV_INIT_CB_STRUCT_SIZE, sizeof(struct init_cb_t));
	GEN_ABSOLUTE_SYM(SPV_INIT_CB_STRUCT_PTR_SIZE, sizeof(struct init_cb_t*));
}
