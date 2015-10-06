#ifndef _NAVIORCINPUT_H
#define _NAVIORCINPUT_H

/**
 * Maximum number of R/C input channels in the system. S.Bus has up to 18 channels.
 */
#define RC_INPUT_MAX_CHANNELS	18

/**
 * Maximum RSSI value
 */
#define RC_INPUT_RSSI_MAX	100

/**
 * @addtogroup topics
 * @{
 */

/**
 * Input signal type, value is a control position from zero to 100
 * percent.
 */
typedef uint16_t		rc_input_t;

enum RC_INPUT_SOURCE {
	RC_INPUT_SOURCE_UNKNOWN = 0,
	RC_INPUT_SOURCE_PX4FMU_PPM,
	RC_INPUT_SOURCE_PX4IO_PPM,
	RC_INPUT_SOURCE_PX4IO_SPEKTRUM,
	RC_INPUT_SOURCE_PX4IO_SBUS,
	RC_INPUT_SOURCE_PX4IO_ST24
};

/**
 * R/C input status structure.
 *
 * Published to input_rc, may also be published to other names depending
 * on the board involved.
 */
struct rc_input_values {
	/** publication time */
	uint64_t		timestamp_publication;

	/** last valid reception time */
	uint64_t		timestamp_last_signal;

	/** number of channels actually being seen */
	uint32_t		channel_count;

	/** receive signal strength indicator (RSSI): < 0: Undefined, 0: no signal, 255: full reception */
	int32_t			rssi;

	/**
	 * explicit failsafe flag: true on TX failure or TX out of range , false otherwise.
	 * Only the true state is reliable, as there are some (PPM) receivers on the market going
	 * into failsafe without telling us explicitly.
	 * */
	bool			rc_failsafe;

	/**
	 * RC receiver connection status: True,if no frame has arrived in the expected time, false otherwise.
	 * True usally means that the receiver has been disconnected, but can also indicate a radio link loss on "stupid" systems.
	 * Will remain false, if a RX with failsafe option continues to transmit frames after a link loss.
	 * */
	bool			rc_lost;

	/**
	 * Number of lost RC frames.
	 * Note: intended purpose: observe the radio link quality if RSSI is not available
	 * This value must not be used to trigger any failsafe-alike funtionality.
	 * */
	uint16_t		rc_lost_frame_count;

	/**
	 * Number of total RC frames.
	 * Note: intended purpose: observe the radio link quality if RSSI is not available
	 * This value must not be used to trigger any failsafe-alike funtionality.
	 * */
	uint16_t		rc_total_frame_count;

	/**
	 * Length of a single PPM frame.
	 * Zero for non-PPM systems
	 */
	uint16_t		rc_ppm_frame_length;

	/** Input source */
	enum RC_INPUT_SOURCE 	input_source;

	/** measured pulse widths for each of the supported channels */
	rc_input_t		values[RC_INPUT_MAX_CHANNELS];
};


#define RC_INPUT_GET 4

#endif /* _NAVIORCINPUT_H */
