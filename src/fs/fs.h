/*!
 * @file fs.h
 * @author Gabriel Gutiérrez Fuentes (me@gab.gf)
 * @brief Embedded Sensor Data Filesystem abstraction layer.
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef SPV_FS_FS_H_
#define SPV_FS_FS_H_

#define SPV_FS_ALIGN			16
#define SPV_FS_DATA_SECTOR_SIZE	(CONFIG_SPV_FS_DATA_SECTOR_COUNT * sizeof(struct fs_data_entry_t))

#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>

#include "timesync/timesync.h"
#include "sense/sense.h"

/**
 * @brief Contents of a summary block. Specifies parameters of a data sector.
 */
struct fs_summary_t {
	/**
	 * @brief Time at which measurements started.
	 */
	uint32_t unix_time_start;

	/**
	 * @brief How many samples per seconds were taken.
	 */
	uint32_t sampling_rate_seconds;

	/**
	 * @brief Amount of sensor readings in the sector.
	 */
	uint32_t final_count;

	/**
	 * @brief Bitfield specifying which sensors were enabled.
	 */
	uint8_t sensor_flags;

	/**
	 * @brief Specifies which time source was used during the reading period.
	 */
	uint8_t timesync_status;

	char _reserved[2];
};

/**
 * @brief A single data reading. Values 0xFFFFFFFE and 0xFFFE represent
 * invalid readings.
 */
struct fs_data_entry_t {
	int32_t centibels;
	uint32_t voc_raw;
	int32_t millicelsius;
	uint16_t rh_permille;
	uint16_t lux;
};


/**
 * @brief Initializes the Embedded Sensor Data Filesystem.
 *
 * @return int 0 if successful.
 */
[[nodiscard]]
int fs_init();

/**
 * @brief Returns a pointer to all populated summary blocks.
 *
 * @param[out] summaries Pointer to retrieved summaries.
 * @param[out] count Amount of retrieved summaries.
 */
[[gnu::nonnull(1, 2)]]
void fs_summary_get_all(const struct fs_summary_t **summaries, size_t *count);

/**
 * @brief Returns the amount of active sectors.
 */
[[nodiscard]]
size_t fs_get_valid_sector_count();

/**
 * @brief Retrieves the summary at the specified index, if populated.
 *
 * @param[out] summary Pointer to the specified summary.
 * @param[in] index Index of the summary in the summary block table.
 *
 * @return 0 if successful.
 */
[[nodiscard, gnu::nonnull(1)]]
int fs_summary_get(const struct fs_summary_t **summary, size_t index);

/**
 * @brief Retrieves the data period from the specified index.
 *
 * @param[out] entries Pointer to retrieved entries. Length can be obtained
 * from the summary block.
 * @param[in] period_index Period to retrieve, same as the summary's index.
 */
[[nodiscard, gnu::nonnull(1)]]
int fs_sector_get_all(const struct fs_data_entry_t **entries, size_t period_index);

/**
 * @brief Creates a new summary sector and marks the previous one as complete.
 * If there is no space remaining, deletes the oldest sector.
 *
 * @param[in] sector_summary New sector's summary.
 */
[[gnu::nonnull(1)]]
void fs_sector_new(const struct fs_summary_t *sector_summary);

/**
 * @brief Appends one entry to the current active data sector.
 *
 * @param[in] entry Data to store.
 *
 * @return int 0 if successful. Fails if sector is full.
 */
[[nodiscard, gnu::nonnull(1)]]
int fs_reading_append(const struct fs_data_entry_t *entry);

/**
 * @brief Returns how many data entries of free space the current active sector
 * has. User code should use this function to check if `fs_reading_append` will
 * fail. Call `fs_sector_new` if size is `0`.
 */
[[nodiscard]]
size_t fs_current_sector_remaining_space();

/**
 * @brief Gets the last data reading and its timestamp.
 *
 * @param[out] time Time output.
 *
 * @return int 0 if successful.
 */
[[nodiscard, gnu::nonnull(1)]]
int fs_try_recover_time(time_t *time);

#endif
