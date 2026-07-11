/*
 * Statically reserve space for tracking each
 * module's init state.
 */

_spv_init_bytes_size = _init_cb_t_list_end - _init_cb_t_list_start;
spv_init_module_count = _spv_init_bytes_size / SPV_INIT_CB_STRUCT_SIZE;
_spv_init_module_state_size = spv_init_module_count * SPV_INIT_CB_STRUCT_PTR_SIZE;

SECTION_PROLOGUE(.spv_init_ram_buffer, (NOLOAD), ALIGN(4)) {
	spv_init_module_state = .;
	. += _spv_init_module_state_size + SPV_INIT_CB_STRUCT_PTR_SIZE;
} GROUP_DATA_LINK_IN(RAMABLE_REGION, ROMABLE_REGION)
