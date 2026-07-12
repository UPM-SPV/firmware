#include "fs/fs.h"
#include "fs/internal.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(fs_internal, LOG_LEVEL_DBG);


bool fs_int_superblock_validate(const struct fs_superblock_t *sblock) {
	return
		sblock->magic == SPV_FS_SUPERBLOCK_MAGIC &&
		sblock->active_sector_index < SECTOR_COUNT &&
		sblock->valid_sectors < SECTOR_COUNT;
}

struct fs_superblock_t* fs_int_superblock_get_idx(size_t index) {
	struct rram_ctx_t *region = &spv_fs_superblock_regions[index];
	return (struct fs_superblock_t*) region->start;
}

struct fs_summary_t* fs_int_summary_get_idx(size_t index) {
	struct fs_summary_t *summaries = (struct fs_summary_t*) spv_fs_summary_region.start;
	return &summaries[index];
}

struct fs_data_entry_t* fs_int_data_get_idx(size_t sector_index, size_t data_offset) {
	struct fs_data_entry_t *entries = (struct fs_data_entry_t*) spv_fs_data_region.start;
	struct fs_data_entry_t *sector = &entries[sector_index * FS_DATA_PER_SECTOR];
	return &sector[data_offset];
}


void fs_int_superblock_set_idx(size_t index, const struct fs_superblock_t *data) {
	struct fs_superblock_t *current = fs_int_superblock_get_idx(index);
	rram_memcpy_no_irq(&spv_fs_superblock_regions[index], current, data, sizeof(*current));
}

void fs_int_summary_set(struct fs_summary_t *rram_ptr, const struct fs_summary_t *data) {
	rram_memcpy_no_irq(&spv_fs_summary_region, rram_ptr, data, sizeof(*data));
}

void fs_int_data_set(struct fs_data_entry_t *rram_ptr, const struct fs_data_entry_t *data) {
	rram_memcpy_no_irq(&spv_fs_data_region, rram_ptr, data, sizeof(*data));
}


void fs_int_superblock_save_all(const struct fs_superblock_t *sblock) {
	for(size_t i = 0; i < SPV_FS_SUPERBLOCK_COUNT; i++) {
		fs_int_superblock_set_idx(i, sblock);
	}
}

void fs_int_sector_wipe(size_t index) {
	struct fs_summary_t *summary = fs_int_summary_get_idx(index);
	struct fs_data_entry_t *data = fs_int_data_get_idx(index, 0);

	char delete_buff[SPV_FS_ALIGN];
	memset(delete_buff, 0xFF, sizeof(delete_buff));

	rram_memcpy_no_irq(&spv_fs_summary_region, summary, delete_buff, sizeof(delete_buff));
	for(size_t i = 0; i < SPV_FS_DATA_SECTOR_SIZE / SPV_FS_ALIGN; i++) {
		rram_memcpy_no_irq(&spv_fs_data_region, &data[i], delete_buff, sizeof(delete_buff));
	}
}

int fs_int_superblock_get(const struct fs_superblock_t **superblock) {
	struct fs_superblock_t *sblocks[SPV_FS_SUPERBLOCK_COUNT];
	for(size_t i = 0; i < SPV_FS_SUPERBLOCK_COUNT; i++) {
		sblocks[i] = fs_int_superblock_get_idx(i);
	}

	// Use the Boyer-More algorithm to find a majority.
	const struct fs_superblock_t *candidate = NULL;
	size_t vote_count = 0;
	for(size_t i = 0; i < SPV_FS_SUPERBLOCK_COUNT; i++) {
		const struct fs_superblock_t *current = sblocks[i];

		if(!fs_int_superblock_validate(current)) {
			continue;
		}

		if(vote_count == 0) {
			candidate = current;
			vote_count++;
		} else {
			if(memcmp(current, candidate, sizeof(*current)) == 0) {
				vote_count++;
			} else {
				vote_count--;
			}
		}
	}

	if(candidate == NULL) {
		return -EINVAL;
	}

	// The algorithm does not guarantee a correct result if there is no actual
	// majority. It requires a second pass to actually check.
	vote_count = 0;
	for(size_t i = 0; i < SPV_FS_SUPERBLOCK_COUNT; i++) {
		const struct fs_superblock_t *current = sblocks[i];
		if(
			fs_int_superblock_validate(current) &&
			memcmp(current, candidate, sizeof(*current)) == 0
		) {
			vote_count++;
		}
	}

	if(vote_count <= SPV_FS_SUPERBLOCK_COUNT / 2) {
		// No actual majority.
		return -EINVAL;
	}

	// Repair corrupted superblocks.
	size_t corrupted_blocks = SPV_FS_SUPERBLOCK_COUNT - vote_count;
	for(size_t i = 0; i < SPV_FS_SUPERBLOCK_COUNT && corrupted_blocks; i++) {
		const struct fs_superblock_t *current = sblocks[i];
		if(memcmp(current, candidate, sizeof(*current)) != 0) {
			fs_int_superblock_set_idx(i, candidate);
			corrupted_blocks--;
		}
	}

	*superblock = candidate;
	return 0;
}

void fs_int_init_wipe() {
	const struct fs_superblock_t clean_sblock = {
		.magic = SPV_FS_SUPERBLOCK_MAGIC,
		.active_sector_index = 0,
		.valid_sectors = 0,
	};

	fs_int_superblock_save_all(&clean_sblock);
}
