/*!
 * @file internal.h
 * @author Gabriel Gutiérrez Fuentes (me@gab.gf)
 * @brief Embedded Sensor Data Filesystem internals.
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef SPV_FS_INTERNAL_H_
#define SPV_FS_INTERNAL_H_

#include "fs/fs.h"
#include "rram/rram.h"

#define SPV_FS_SUPERBLOCK_MAGIC	0xE5DFFD5E	// ESDF FDSE
#define SPV_FS_SUPERBLOCK_COUNT	3

#define	FS_SIZE 	FIXED_PARTITION_SIZE(sensor_data_partition)
#define FS_DATA_PER_SECTOR	SPV_FS_DATA_SECTOR_SIZE / sizeof(struct fs_data_entry_t)
#define SECTOR_COUNT	(\
	(FS_SIZE - sizeof(struct fs_superblock_t) * SPV_FS_SUPERBLOCK_COUNT)					\
		/																		\
	(sizeof(struct fs_summary_t) + SPV_FS_DATA_SECTOR_SIZE)							\
)
#define FS_START			FIXED_PARTITION_ADDRESS(sensor_data_partition)
#define FS_SUMMARY_START	(FS_START + sizeof(struct fs_superblock_t))
#define FS_SUMMARY_END		(FS_SUMMARY_START + sizeof(struct fs_summary_t) * SECTOR_COUNT)
#define FS_DATA_START		(FS_SUMMARY_END + sizeof(struct fs_superblock_t))
#define FS_DATA_END			(FS_DATA_START + SPV_FS_DATA_SECTOR_SIZE * SECTOR_COUNT)


/**
 * @brief Contents of a superblock.
 */
struct fs_superblock_t {
	/**
	 * @brief Always 0xE5DFFD5E
	 */
	uint32_t magic;

	/**
	 * @brief Stores the currently active sector.
	 */
	uint32_t active_sector_index;

	/**
	 * @brief How many sectors contain valid data. Starting from the 0th.
	 */
	uint32_t valid_sectors;

	char _reserved[4];
};

/**
 * @brief Stores the current in-memory state of the filesystem.
 */
struct fs_status_t {
	/**
	 * @brief Current active sector.
	 */
	size_t active_sector_index;

	/**
	 * @brief How many sectors contain valid data. Starting from the 0th.
	 */
	size_t valid_sectors;

	/**
	 * @brief Amount of data in the current active sector.
	 */
	size_t current_data_count;

	/**
	 * @brief Indicates whether the entire filesystem is fresh, and no sector
	 * contains any data.
	 */
	bool fresh_start:1;
};


/**
 * @brief List of all superblock locations.
 */
extern struct rram_ctx_t spv_fs_superblock_regions[SPV_FS_SUPERBLOCK_COUNT];

/**
 * @brief Location of summary blocks.
 */
extern struct rram_ctx_t spv_fs_summary_region;

/**
 * @brief Location of data sectors.
 */
extern struct rram_ctx_t spv_fs_data_region;

/**
 * @brief Initializes the filesystem from a clean state, resetting all
 * superblocks. Utilize this function if the filesystem is in an irrepairable
 * state -if `fs_superblock_get()` returns non-zero.
 */
void fs_int_init_wipe();

/**
 * @brief Returns a pointer to *one* of the valid superblocks. If any superblock
 * disagrees with the rest, it is automatically overwritten with the majority
 * value. If no majority can be achieved or all superblocks are corrupted,
 * an error is returned -this error will happen on first boot.
 *
 * @param[out] superblock Pointer to one of the superblocks. All superblocks
 * will contain the same values.
 *
 * @return int 0 if successful.
 */
[[nodiscard, gnu::nonnull(1)]]
int fs_int_superblock_get(const struct fs_superblock_t **superblock);

/**
 * @brief Saves the given superblock information to every superblock.
 *
 * @param[in] sblock_data Data to save.
 */
[[gnu::nonnull(1)]]
void fs_int_superblock_save_all(const struct fs_superblock_t *sblock_data);

/**
 * @brief Overwrites an entire data sector and its associated summary block
 * with 0xFF.
 *
 * @param index Which sector to overwrite.
 */
void fs_int_sector_wipe(size_t index);


/**
 * @brief Checks if the given superblock contains valid data.
 *
 * @param[in] sblock Pointer to the superblock.
 */
[[nodiscard, gnu::nonnull(1)]]
bool fs_int_superblock_validate(const struct fs_superblock_t *sblock);

/**
 * @brief Returns a pointer to the specified superblock.
 *
 * @param[in] index Superblock index.
 *
 * @return struct fs_superblock_t* Pointer to superblock.
 */
[[nodiscard]]
struct fs_superblock_t* fs_int_superblock_get_idx(size_t index);

/**
 * @brief Returns a pointer to the requested summary sector.
 *
 * @param[in] index Index of the sector.
 * @return struct fs_summary_t* Pointer to the summary sector.
 */
[[nodiscard]]
struct fs_summary_t* fs_int_summary_get_idx(size_t index);

/**
 * @brief Updates the given superblock.
 *
 * @param[in] index Superblock to update.
 * @param[in] data Data to write.
 */
[[gnu::nonnull(2)]]
void fs_int_superblock_set_idx(size_t index, const struct fs_superblock_t *data);

/**
 * @brief Updates the given summary.
 *
 * @param[out] rram_ptr Summary to update.
 * @param[in] data Data to write.
 */
[[gnu::nonnull(1, 2)]]
void fs_int_summary_set(struct fs_summary_t *rram_ptr, const struct fs_summary_t *data);

/**
 * @brief Updates the given data entry.
 *
 * @param[out] rram_ptr Entry to update.
 * @param[in] data Data to write.
 */
[[gnu::nonnull(1, 2)]]
void fs_int_data_set(struct fs_data_entry_t *rram_ptr, const struct fs_data_entry_t *data);

/**
 * @brief Returns a pointer to a particular data entry.
 *
 * @param[in] sector_index Which sector to read into.
 * @param[in] data_offset How far into the sector to read.
 *
 * @return struct fs_data_entry_t* Pointer to the data block.
 */
[[nodiscard]]
struct fs_data_entry_t* fs_int_data_get_idx(size_t sector_index, size_t data_offset);

#endif
