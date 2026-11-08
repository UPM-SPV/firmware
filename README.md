<div align="center">

# $SPV\ firmware$
[![Continuous Integration](https://github.com/UPM-SPV/firmware/actions/workflows/build.yml/badge.svg)](https://github.com/UPM-SPV/firmware/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/v/release/UPM-SPV/firmware)](https://github.com/UPM-SPV/firmware/releases)
[![License](https://img.shields.io/github/license/UPM-SPV/firmware)](https://github.com/UPM-SPV/firmware/blob/master/COPYING)

</div>

Firmware for the [**SPV board**](https://github.com/UPM-SPV/pcb), which implements an offline weather station capable of measuring temperature, humidity, luminosity, noise level, and air quality. Data is [stored locally](#sensor-data-filesystem) in non-volatile memory, and egressed via [BLE](#sensor-data-service).

## Data logging
Data is logged at a fixed interval which can be configured over BLE, it can be set as low as 1 minute per sample, or as high as 60 minutes per sample. To conserve power, any sensor that is deemed unnecessary can also be disabled. Note that due to filesystem geometry, disabling a sensor does not increase retention capacity.

### Sensor Data Filesystem
To reduce power consumption and storage overhead, a custom filesystem has been implemented. It is a circular append-only buffer, older entries are automatically deleted when new ones require space. Metadata -timestamp, active sensors, time source quality- is coarsely stored in *summary* data blocks, which reduces overhead for each reading but still allows reconfiguration without a full filesystem wipe. By default, each data sector contains 256 data blocks and has one summary block each.

The implementation is built directly over the nRF54L's RRAM, which allows memory-mapped access and implements hardware-level error correction. All data structures are aligned and sized so as to take advantage of this.

The default configuration allocates 684 kB to the entire filesystem (44% of total NV memory), with enough capacity for 45,312 individual readings. Assuming a static configuration, the system can store the following amount of data before a download is required:

<table align="center">
	<thead>
		<tr>
			<th align="center">Sampling period</th>
			<th align="center">Sampling rate</th>
			<th align="center" colspan="2">Retention period</th>
		</tr>
	</thead>
	<tbody>
		<tr>
			<td align="center">1 min/sample</td>
			<td align="center">60 samples/hour</td>
			<td align="center">755.2 hours</td>
			<td align="center">31.46 days</td>
		</tr>
		<tr>
			<td align="center">2 min/sample</td>
			<td align="center">30 samples/hour</td>
			<td align="center">1,510.4 hours</td>
			<td align="center">62.93 days</td>
		</tr>
		<tr>
			<td align="center">5 min/sample</td>
			<td align="center">12 samples/hour</td>
			<td align="center">3,776 hours</td>
			<td align="center">157.33 days</td>
		</tr>
		<tr>
			<td align="center">10 min/sample</td>
			<td align="center">6 samples/hour</td>
			<td align="center">7,552 hours</td>
			<td align="center">314.66 days</td>
		</tr>
		<tr>
			<td align="center" colspan="5">...</td>
		</tr>
		<tr>
			<td align="center">60 min/sample</td>
			<td align="center">1 sample/hour</td>
			<td align="center">45,312 hours <a href="#sensor-data-filesystem-note">*</a></td>
			<td align="center">1,888 days <a href="#sensor-data-filesystem-note">*</a></td>
		</tr>
	</tbody>
</table>

<a id="sensor-data-filesystem-note"></a>
> [!NOTE]
> Some sample rate configurations may cause data retention capacity to exceed battery life.

### Sensor Data Service
Data synchronization uses both BLE GATT and L2CAP. The GATT control characteristic (UUID [`8011`](#ble-services)) can be read to obtain the L2CAP PSM, and be written to to trigger full data dumps over the L2CAP channel. See [`sensor_data_request.proto`](res/protobuf/sensor_data_request.proto) for the request format.

The device can either send:

- **Data summaries** by sending an `SdsSummaryRequest`. This causes every summary block to be sent over the L2CAP channel.

- An entire **data sector** by sending an `SdsDataRequest` together with the requested index and the offset into the sector itself.

Every L2CAP response includes a protobuf header (`SdsSummaryResponseHeader` or `SdsDataResponseHeader`) which specifies the amount of blocks that will be sent, followed by a stream of data. The data payload are formatted as raw C structs, and their layout can be found in [`fs.h`](src/fs/fs.h)(`fs_summary_t` and `fs_data_entry_t`).

## Attestation
Every device is equipped with a unique private key generated during flashing, as well as an associated X.509 certificate signed by a root Certificate Authority. This allows verifying the authenticity of every device by using a challenge-response protocol.

The X.509 certificate can be read over BLE, and contains both the device's MAC address as well as its serial number. After verifying the certificate against the root CA cert, the attestator can write a **32-byte** nonce to the [challenge characteristic](#ble-svcs-attest-challenge), and obtain the signed value by reading the [response characteristic](#ble-svcs-attest-response).

## Image protection and OTA
Firmware images are encrypted using AES-128 and a common X25519 private value stored in every device. They are also signed with an ed25519 key.

MCUboot is used to manage A/B partitioning for firmware images using the `swap` mode.

## BLE services
The following is a full table of BLE services. Non-standard UUIDs (marked with **\***) use the 128-bit 3C15**XXXX**-9311-4A07-8520-A817462A9F16 UUID as a base, with the two **XXXX** bytes acting as placeholder. Custom services use `80X0` as their ID.  The device will also broadcast the 128-bit UUID with a value of `0001` during normal operation, and `0002` during pairing.

<table align="center">
	<thead>
		<tr>
			<th align="center">Service</th>
			<th align="center">Characteristic</th>
			<th align="center">UUID</th>
			<th align="center">Ops.</th>
			<th align="center">Description</th>
		</tr>
	</thead>
	<tbody>
		<tr>
			<td align="center"><b>Sensor Data *</b></td>
			<td align="center">Control *</td>
			<td align="center"><code>8011</code></td>
			<td align="center">R/W</td>
			<td align="center">Sensor Data Service control endpoint, see <a href="#sensor-data-service">SDS</a></td>
		</tr>
		<tr id="ble-svcs-attest-challenge">
			<td align="center" rowspan="3"><b>Attestation *</b></td>
			<td align="center">Challenge *</td>
			<td align="center"><code>8021</code></td>
			<td align="center">W</td>
			<td align="center">32-byte challenge nonce</td>
		</tr>
		<tr id="ble-svcs-attest-response">
			<td align="center">Response *</td>
			<td align="center"><code>8022</code></td>
			<td align="center">R/I</td>
			<td align="center">Raw secp256r1+SHA-256 64-byte signature</td>
		</tr>
		<tr>
			<td align="center">Certificate *</td>
			<td align="center"><code>8023</code></td>
			<td align="center">R</td>
			<td align="center">Device-unique DER-encoded X.509 certificate</td>
		</tr>
		<tr>
			<td align="center"><b>Battery</b></td>
			<td align="center">Battery level</td>
			<td align="center"><code>2A19</code></td>
			<td align="center">R</td>
			<td align="center">Battery level from 0% to 100%, represented as a <code>uint8_t</code></td>
		</tr>
		<tr>
			<td align="center"><b>Date &amp; Time</b></td>
			<td align="center">UNIX Time *</td>
			<td align="center"><code>8031</code></td>
			<td align="center">R/W</td>
			<td align="center">System UNIX time in seconds as a <code>uint32_t</code></td>
		</tr>
		<tr>
			<td align="center" rowspan="3"><b>Device<br>information<br>service</b></td>
			<td align="center">Serial number</td>
			<td align="center"><code>2A25</code></td>
			<td align="center">R</td>
			<td align="center">Unique serial number, text string</td>
		</tr>
		<tr>
			<td align="center">Firmware version</td>
			<td align="center"><code>2A26</code></td>
			<td align="center">R</td>
			<td align="center">Current firmware version, SemVer string: <code>X.Y.Z</code></td>
		</tr>
		<tr>
			<td align="center">Hardware revision</td>
			<td align="center"><code>2A27</code></td>
			<td align="center">R</td>
			<td align="center">PCB version, SemVer string: <code>X.Y.Z</code></td>
		</tr>
		<tr>
			<td align="center"><b>Settings</b></td>
			<td align="center">Settings *</td>
			<td align="center"><code>8041</code></td>
			<td align="center">R/W</td>
			<td align="center">Serialized device settings, see <a href="./res/protobuf/device_settings.proto"><code>device_settings.proto</code></a></td>
		</tr>
		<tr>
			<td align="center" rowspan="5"><b>Environmental<br>Sensing **</b></td>
			<td align="center">Temperature</td>
			<td align="center"><code>2A6E</code></td>
			<td align="center">R</td>
			<td align="center">Current temperature in degrees centicelsius as an <code>int16_t</code></td>
		</tr>
		<tr>
			<td align="center">Humidity</td>
			<td align="center"><code>2A6F</code></td>
			<td align="center">R</td>
			<td align="center">Relative humidity as a <code>uint16_t</code> in increments of 0.01%</td>
		</tr>
		<tr>
			<td align="center">Luminosity</td>
			<td align="center"><code>2AFB</code></td>
			<td align="center">R</td>
			<td align="center">Ambient light level in lux as a <code>uint16_t</code></td>
		</tr>
		<tr>
			<td align="center">Noise level *</td>
			<td align="center"><code>8051</code></td>
			<td align="center">R</td>
			<td align="center">Noise level in centibels as a <code>uint16_t</code></td>
		</tr>
		<tr>
			<td align="center">Air quality *</td>
			<td align="center"><code>8052</code></td>
			<td align="center">R</td>
			<td align="center">Raw Sensirion VOC value as a <code>uint32_t</code></td>
		</tr>
	</tbody>
</table>

> [!NOTE]
> The environmental sensing service provides current values and should only be used for real-time monitoring.
>
> To obtain past readings, use the [Sensor Data Service](#sensor-data-service).

## Power consumption
I used a [Nordic PPKII](https://www.nordicsemi.com/Products/Development-hardware/Power-Profiler-Kit-2) to measure power consumption, three ten-minute-long tests were performed on different modes. Every test was configured with every sensor enabled and maximum sampling rate of 1 min/sample. The battery life estimates are just that, they don't consider battery degradation nor current spikes, they assume a flat 600 mAh CR2450.

<table align="center">
	<thead>
		<tr>
			<th align="center">TX power</th>
			<th align="center">Average power draw @3.3V</th>
			<th align="center">Battery life</th>
		</tr>
	</thead>
	<tbody>
		<tr>
			<td align="center" colspan="3"><b>Disconnected (2s advertisement interval)</b></td>
		</tr>
		<tr>
			<td align="center">0 dBm</td>
			<td align="center">27.08 μA</td>
			<td align="center">2 years and 6 months</td>
		</tr>
		<tr>
			<td align="center">2 dBm</td>
			<td align="center">30.17 μA</td>
			<td align="center">2 years and 3 months</td>
		</tr>
		<tr>
			<td align="center">4 dBm</td>
			<td align="center">30.88 μA</td>
			<td align="center">2 years and 2 months</td>
		</tr>
		<tr>
			<td align="center">8 dBm</td>
			<td align="center">32.58 μA</td>
			<td align="center">2 years and 1 month</td>
		</tr>
		<tr>
			<td align="center" colspan="3"><b>Connected (20 ms advertisement interval)</b></td>
		</tr>
		<tr>
			<td align="center">0 dBm</td>
			<td align="center">41.68 μA</td>
			<td align="center">1 year and 7 months</td>
		</tr>
		<tr>
			<td align="center">2 dBm</td>
			<td align="center">43.85 μA</td>
			<td align="center">1 year and 6 months</td>
		</tr>
		<tr>
			<td align="center">4 dBm</td>
			<td align="center">44.59 μA</td>
			<td align="center">1 year and 6 months</td>
		</tr>
		<tr>
			<td align="center">8 dBm</td>
			<td align="center">52.16 μA</td>
			<td align="center">1 year and 3 months</td>
		</tr>
		<tr>
			<td align="center" colspan="3"><b>Disconnected (200ms advertisement interval + LED blink)</b></td>
		</tr>
		<tr>
			<td align="center">0 dBm</td>
			<td align="center">375.59 μA</td>
			<td align="center">2 months</td>
		</tr>
		<tr>
			<td align="center">2 dBm</td>
			<td align="center">474.89 μA</td>
			<td align="center">1 month and 3 weeks</td>
		</tr>
		<tr>
			<td align="center">4 dBm</td>
			<td align="center">489.40 μA</td>
			<td align="center">1 month and 2 weeks</td>
		</tr>
		<tr>
			<td align="center">8 dBm</td>
			<td align="center">547.17 μA</td>
			<td align="center">1 month</td>
		</tr>
	</tbody>
</table>

## Building and flashing

### Set up NCS
Install the Nordic Semiconductor SDK, either by using the [`nrfutil`](https://www.nordicsemi.com/Products/Development-software/nRF-Connect-SDK) CLI tool, or using the [VSCode](https://www.nordicsemi.com/Products/Development-software/nRF-Connect-SDK) extension. Make sure you install version **3.4.1**.

After installing, make sure to run every subsequent command in a terminal with the nRF environment enabled.

### Generating keys
Next, generate the Root CA certificate. Currently only a single-level CA is implemented, but the firmware is agnostic to this detail and may be changed without modifying any firmware code. Run the following command:

```shell
$ sh config/cert/cagen.sh
```

Then, generate the firmware encryption and signing keys by running:

```shell
$ sh config/keys/keygen.sh
```

Both the CA and firmware protection keys will be stored in `config/local/`. Make sure you do not **lose** nor **commit** them.

### Flashing firmware
Finally, connect a Segger probe into your PC, and plug the SWD port into your board. Then run the following command:

```shell
$ python config/provision/flash_device.py \
	--release \
	--nfc-tag
```

The `--release` parameter enables optimizations and removes debugging information from the final image.

The `--nfc-tag` flag allows writing the device's MAC and serial number into an NTAG-compatible NFC tag for easy pairing when using the [Android app](https://github.com/UPM-SPV/android-app).

## License
This repository and all of its files are released under the [*GNU General Public License v3.0 **or later***](COPYING) at your own choice.

---
Copyright (c) Gabriel Gutiérrez Fuentes 2026
