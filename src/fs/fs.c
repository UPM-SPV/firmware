#include "fs/fs.h"
#include "fs/internal.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(fs, LOG_LEVEL_DBG);

struct rram_ctx_t spv_fs_superblock_regions[SPV_FS_SUPERBLOCK_COUNT] = {
	(struct rram_ctx_t) {
		.start	= (volatile char*) FS_START,
		.end	= (volatile char*) FS_SUMMARY_START
	},
	(struct rram_ctx_t) {
		.start	= (volatile char*) FS_SUMMARY_END,
		.end	= (volatile char*) FS_DATA_START,
	},
	(struct rram_ctx_t) {
		.start	= (volatile char*) FS_DATA_END,
		.end	= (volatile char*) (FS_DATA_END + sizeof(struct fs_superblock_t))
	}
};
struct rram_ctx_t spv_fs_summary_region = {
	.start	= (volatile char*) FS_SUMMARY_START,
	.end	= (volatile char*) FS_SUMMARY_END
};
struct rram_ctx_t spv_fs_data_region = {
	.start	= (volatile char*) FS_DATA_START,
	.end	= (volatile char*) FS_DATA_END,
};


/**
 * @brief Current status.
 */
static struct fs_status_t fs_status;


static size_t fs_restore_active_sector_count(size_t index);

/*
 *
 * Public interface.
 *
 */

int fs_init() {
	int err;
	const struct fs_superblock_t *sblock;
	err = fs_int_superblock_get(&sblock);
	if(err) {
		LOG_WRN("Error obtaining superblocks, filesystem corrupted, wiping...");
		fs_status.active_sector_index = 0;
		fs_status.current_data_count = 0;
		fs_status.fresh_start = true;
		fs_int_init_wipe();

		return 0;
	}

	fs_status.active_sector_index = sblock->active_sector_index;
	fs_status.valid_sectors = sblock->valid_sectors;
	fs_status.current_data_count = fs_restore_active_sector_count(sblock->active_sector_index);
	fs_status.fresh_start = false;

	LOG_DBG("Found valid filesystem:");
	LOG_DBG("	Active sector:	%zu", fs_status.active_sector_index);
	LOG_DBG("	Valid sectors:	%zu", fs_status.valid_sectors);
	LOG_DBG("	Data count:		%zu", fs_status.current_data_count);
	return 0;
}

void fs_sector_new(const struct fs_summary_t *sector_summary) {
	size_t next_sector;
	bool update_superblock;

	LOG_INF("Allocating new FS sector");

	// If this is a fresh start, use the first sector.
	if(fs_status.fresh_start) {
		LOG_DBG("	Found fresh filesystem");
		fs_status.fresh_start = false;
		next_sector = 0;
		update_superblock = true;
	} else {
		// If the current sector is empty, reuse it.
		if(fs_status.current_data_count == 0) {
			LOG_DBG("	Found empty sector, reusing");
			next_sector = fs_status.active_sector_index;
			update_superblock = false;
		} else {
			// Otherwise, commit the current sector and advance to the next.
			next_sector = (fs_status.active_sector_index + 1) % SECTOR_COUNT;
			update_superblock = true;

			struct fs_summary_t *current_summary = fs_int_summary_get_idx(fs_status.active_sector_index);
			struct fs_summary_t updated_summary;
			memcpy(&updated_summary, current_summary, sizeof(updated_summary));
			updated_summary.final_count = fs_status.current_data_count;
			fs_int_summary_set(current_summary, &updated_summary);

			LOG_DBG("	Commited old data sector:");
			LOG_DBG("		Index:		%zu", fs_status.active_sector_index);
			LOG_DBG("		Data count:	%zu", updated_summary.final_count);
		}
	}

	LOG_DBG("	Wiping sector %zu", next_sector);
	fs_int_sector_wipe(next_sector);
	struct fs_summary_t *new_summary = fs_int_summary_get_idx(next_sector);
	fs_int_summary_set(new_summary, sector_summary);

	fs_status.valid_sectors = max(next_sector + 1, fs_status.valid_sectors);
	fs_status.active_sector_index = next_sector;
	fs_status.current_data_count = 0;

	if(update_superblock) {
		struct fs_superblock_t sblock = {
			.magic = SPV_FS_SUPERBLOCK_MAGIC,
			.active_sector_index = next_sector,
			.valid_sectors = fs_status.valid_sectors,
		};
		LOG_DBG("	Updating superblocks");
		fs_int_superblock_save_all(&sblock);
	}

	LOG_DBG("	Sector rotation complete");
}

int fs_reading_append(const struct fs_data_entry_t *entry) {
	if(fs_current_sector_remaining_space() == 0) {
		return -ENOSPC;
	}

	struct fs_data_entry_t *rram_entry = fs_int_data_get_idx(
		fs_status.active_sector_index,
		fs_status.current_data_count
	);
	fs_int_data_set(rram_entry, entry);

	fs_status.current_data_count++;

	LOG_DBG("Appended data entry to data sector");
	LOG_DBG("	Available blocks:	%zu", fs_current_sector_remaining_space());

	return 0;
}

size_t fs_current_sector_remaining_space() {
	return FS_DATA_PER_SECTOR - fs_status.current_data_count;
}

size_t fs_get_valid_sector_count() {
	return fs_status.valid_sectors;
}

void fs_summary_get_all(const struct fs_summary_t **summaries, size_t *count) {
	LOG_DBG("Obtaining filesystem summaries");

	*count = fs_status.valid_sectors;
	*summaries = fs_int_summary_get_idx(0);

	// Since the active summary sector's count isn't reflected in non-volatile
	// memory until the full sector is commmitted, the user would wrongly
	// obtain '0 readings'.

	struct fs_summary_t *current_summary = fs_int_summary_get_idx(fs_status.active_sector_index);
	if(current_summary->final_count == fs_status.current_data_count) {
		return;
	}

	// Update the summary here before returning it to the user.

	// `current_summary` is in RRAM. Writing to it would throw an MPU exception (!).
	struct fs_summary_t updated_summary;
	memcpy(&updated_summary, current_summary, sizeof(updated_summary));
	updated_summary.final_count = fs_status.current_data_count;
	fs_int_summary_set(current_summary, &updated_summary);

	LOG_DBG("	Updated summary data count to %zu", updated_summary.final_count);
}

int fs_summary_get(const struct fs_summary_t **summary, size_t index) {
	if(index >= fs_status.valid_sectors) {
		return -EINVAL;
	}

	*summary = fs_int_summary_get_idx(index);
	return 0;
}

int fs_sector_get_all(const struct fs_data_entry_t **entries, size_t period_index) {
	if(period_index >= fs_status.valid_sectors) {
		return -EINVAL;
	}

	*entries = fs_int_data_get_idx(period_index, 0);
	return 0;
}

int fs_try_recover_time(time_t *time) {
	LOG_DBG("Attempting filesystem time recovery");
	if(fs_status.valid_sectors == 0) {
		LOG_DBG("	No available data");
		return -ENODATA;
	}

	const struct fs_summary_t *current_summary = fs_int_summary_get_idx(fs_status.active_sector_index);
	*time =
		current_summary->unix_time_start +
		current_summary->sampling_rate_seconds * fs_status.current_data_count;

	LOG_DBG("	Recovered time: %s", ctime(time));
	return 0;
}


/*
 *
 * Internal functions.
 *
 */

/*
 * To improve RRAM endurance, a sector's `final_count` is not saved until it is
 * complete (or a read is performed), which means system crashes will leave
 * them marked as empty.
 *
 * Look into the sector to find the last valid datapoint.
 * - A fresh data block contains only 16 0xFF bytes.
 * - A populated block will contain different values; in case of sensor errors
 * 	will contain 0xFFFFE or 0xFFFFFFFE depending on the type.
 */
static size_t fs_restore_active_sector_count(size_t sector_index) {
	LOG_INF("Starting sector count recovery");
	size_t start = 0;
	size_t end = FS_DATA_PER_SECTOR; // Allows the search to return 256.
									 // Note that this will never actually be read.

	struct fs_data_entry_t invalid_entry;
	memset(&invalid_entry, 0xFF, sizeof(invalid_entry));
	struct fs_data_entry_t *current_sector = fs_int_data_get_idx(sector_index, 0);

	while(start < end) {
		size_t index = start + (end - start) / 2;
		struct fs_data_entry_t *current = &current_sector[index];
		LOG_DBG("	Start, index, end: (%zu, %zu, %zu)", start, index, end);

		if(memcmp(&invalid_entry, current, sizeof(invalid_entry)) == 0) {
			// Invalid entry, second half is invalid.
			//
			// Note that we're not looking for a particular element but for the
			// boundary separating valid and invalid data (!).
			LOG_DBG("		Found invalid block, moving end: %zu -> %zu", end, index);
			end = index;
		} else {
			// Valid entry, entire first half is OK.
			LOG_DBG("		Found valid block, moving start: %zu -> %zu", start, index + 1);
			start = index + 1;
		}
	}

	// Start and end now point to the first invalid block or one more than the
	// last entry of the sector.
	LOG_DBG("	Found %zu active blocks", start);
	return start;
}
