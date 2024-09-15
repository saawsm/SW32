/*
 * swx
 * Copyright (C) 2024 saawsm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _MESSAGE_H
#define _MESSAGE_H

// Each message is formatted the following way:
//
// [MSG_FRAME_START:8] [id:8] [args:n]
//
// The message is encoded using COBS before being sent. The receiver will buffer data until it receives a 0x00 byte before COBS decoding it.
// Debugging information (LOG_ macros) and COBS encoded messages share the same communication channel. This is achieved by having 0x00 byte appended
// to any debugging messages, and assuming these messages will not contain the non-printable ASCII control character (STX 0x02 - MSG_FRAME_START).

#define MSG_SIZE (1024)
#define MSG_FRAME_SIZE (MSG_SIZE + 1 + ((MSG_SIZE + (254 - 1)) / 254))
#define MSG_FRAME_START (2) // STX

// ----------------------------------------------------------------------------------------

// Requests firmware version. Replies to sender with a MSG_ID_VERSION message.
//
// Format: <none>
#define MSG_ID_REQUEST_VERSION (2)

// Firmware version
//
// Format: [version_pcb_rev:8 version_major:8 version_minor:8]
#define MSG_ID_VERSION (3)

// ----------------------------------------------------------------------------------------

// Requests swx error state. Replies to sender with a MSG_ID_ERR message.
//
// Format: <none>
#define MSG_ID_REQUEST_ERR (4)

// SWX error state. See SWX_ERR* for errors.
//
// Format: [err_hi:8 err_lo:8]
#define MSG_ID_ERR (5)

// ----------------------------------------------------------------------------------------

// Shutdown device. Device will remain on until USB power is removed.
//
// Format: <none>
#define MSG_ID_SHUTDOWN (9)

// Restart to USB bootloader.
//
// Format: <none>
#define MSG_ID_RESET_TO_USB_BOOT (10)

// ----------------------------------------------------------------------------------------

// Requests the microphone plug-in-power enable state. Replies to sender with a MSG_ID_UPDATE_MIC_PIP_EN message.
//
// Format: <none>
#define MSG_ID_REQUEST_MIC_PIP_EN (11)

// Sets if microphone plug-in-power is enabled.
//
// Format: [enable:8]
#define MSG_ID_UPDATE_MIC_PIP_EN (12)

// ----------------------------------------------------------------------------------------

// Requests the microphone preamp gain. Replies to sender with a MSG_ID_UPDATE_MIC_GAIN message.
//
// Format: <none>
#define MSG_ID_REQUEST_MIC_GAIN (13)

// Sets the microphone preamp gain.
//
// Format: [gain:8]
#define MSG_ID_UPDATE_MIC_GAIN (14)

// ----------------------------------------------------------------------------------------

// Requests the maximum power level for one or more output channels. Replies to sender with one or more MSG_ID_UPDATE_MAX_POWER messages.
//
// Format: [ch_mask:8]
#define MSG_ID_REQUEST_MAX_POWER (20)

// Sets the maximum power level for one or more output channels. Value represents a percentage out of UINT16_MAX.
//
// Format: [ch_mask:8] [value_hi:8 value_lo:8]
#define MSG_ID_UPDATE_MAX_POWER (21)

// ----------------------------------------------------------------------------------------

// Requests the "require zero" channel bitflags. Replies to sender with a MSG_ID_UPDATE_REQUIRE_ZERO message.
//
// Format: <none>
#define MSG_ID_REQUEST_REQUIRE_ZERO (22)

// Bitflags indicating what output channel is in "require zero" mode (LSB=channel 0).
//
// Format: [flags:8]
#define MSG_ID_UPDATE_REQUIRE_ZERO (23)

// ----------------------------------------------------------------------------------------

// Requests the audio source/mode for one or more output channels. Replies to sender with one or more MSG_ID_UPDATE_CH_AUDIO messages.
//
// Format: [ch_mask:8]
#define MSG_ID_REQUEST_CH_AUDIO (24)

// Sets the pulse generator audio source/mode for one or more output channels. Audio source represents an analog_channel_t. See AUDIO_MODE_FLAG* for modes.
// Flags "require zero" if audio source changed.
//
// Format: [ch_mask:8] [gen_pulses:1 gen_power:1 audio_src:6]
#define MSG_ID_UPDATE_CH_AUDIO (25)

// ----------------------------------------------------------------------------------------

// Requests the gain for a specific analog channel. Replies to sender with a MSG_ID_UPDATE_GAIN message.
//
// Format: [analog_channel:8]
#define MSG_ID_REQUEST_GAIN (26)

// Sets the gain for a specific analog channel.
//
// Format: [analog_channel:8] [gain:8]
#define MSG_ID_UPDATE_GAIN (27)

// ----------------------------------------------------------------------------------------

// Requests the pulse generator channel enable mask. Replies to sender with a MSG_ID_UPDATE_CH_EN_MASK message.
//
// Format: <none>
#define MSG_ID_REQUEST_CH_EN_MASK (28)

// Sets the pulse generator channel enable mask. Value is a bit mask representing output channels that are enabled (LSB=channel 0).
// Flags "require zero" if enable changed.
//
// Format: [en_mask:8]
#define MSG_ID_UPDATE_CH_EN_MASK (29)

// ----------------------------------------------------------------------------------------

// Requests a parameter target for one or more channels from the pulse generator. Replies to sender with one or more MSG_ID_UPDATE_CH_PARAM messages.
//
// Format: [ch_mask:8] [param:4 target:4]
#define MSG_ID_REQUEST_CH_PARAM (30)

// Sets a pulse generator parameter target for one or more channels.
//
// Format: [ch_mask:8] [param:4 target:4] [value_hi:8 value_lo:8]
#define MSG_ID_UPDATE_CH_PARAM (31)

// Update internal parameter state for one or more channels. Updates are needed when a parameter is
// auto cycling and changes to TARGET_MODE, RATE, MIN, or MAX occur. Use param 0xff to update all parameters for the given channel mask.
//
// Format: [ch_mask:8] [param:8]
#define MSG_ID_CH_PARAM_UPDATE (32)

// ----------------------------------------------------------------------------------------

// Requests output channel status for one or more channels. Replies to sender with one or more MSG_ID_CH_STATUS messages.
//
// Format: [ch_mask:8]
#define MSG_ID_REQUEST_CH_STATUS (33)

// Output channel status for one channel (LSB=channel 0). Status represents a channel_status_t.
//
// Format: [ch_mask:8] [status:8]
#define MSG_ID_CH_STATUS (34)

// ----------------------------------------------------------------------------------------

// Requests sequencer sequence. Replies to sender with a MSG_ID_UPDATE_SEQ message.
//
// Format: <none>
#define MSG_ID_REQUEST_SEQ (35)

// Sets the sequencer sequence. (LSB=channel 0). If wrap is true, sequencer wrap count (MSG_ID_UPDATE_SEQ_COUNT) is set to specified count.
//
// Format: [wrap:8] [count:8] [mask:8 ...count]
#define MSG_ID_UPDATE_SEQ (36)

// Requests the current sequencer count. Replies to sender with a MSG_ID_UPDATE_SEQ_COUNT message.
//
// Format: <none>
#define MSG_ID_REQUEST_SEQ_COUNT (37)

// Sets the current sequencer count. The total number of sequence items before the index is wrapped.
//
// Format: [count:8]
#define MSG_ID_UPDATE_SEQ_COUNT (38)

// Resets the current sequencer index back to zero.
//
// Format: <none>
#define MSG_ID_RESET_SEQ_INDEX (39)

// Requests the sequence period in milliseconds. Replies to sender with a MSG_ID_UPDATE_SEQ_PERIOD message.
//
// Format: <none>
#define MSG_ID_REQUEST_SEQ_PERIOD (40)

// Sets the period between each sequence item in milliseconds. Set zero to disable sequencer.
//
// Format: [period_ms_hi:8 period_ms_lo:8]
#define MSG_ID_UPDATE_SEQ_PERIOD (41)

// ----------------------------------------------------------------------------------------

// Requests an action at the specified action slot index. Responds with a MSG_ID_UPDATE_ACTION message.
//
// Format: [a_index:8]
#define MSG_ID_REQUEST_ACTION (42)

// Sets an action at the specified action slot index. See param_t, target_t and action_type_t.
// Set type to ACTION_NONE or enabled to zero to disable.
//
// Format: [a_index:8] [enabled:8] [type:8] [ch_mask:8] [param:8] [target:8] [value_hi:8 value_lo:8]
#define MSG_ID_UPDATE_ACTION (43)

// Runs all actions between start and end indices. End index is exclusive.
//
// Format: [a_start_index:8] [a_end_index:8]
#define MSG_ID_RUN_ACTION_LIST (44)

// ----------------------------------------------------------------------------------------

// Requests a trigger at the specified trigger slot index. Responds with a MSG_ID_UPDATE_TRIGGER message.
//
// Format: [trig_index:8]
#define MSG_ID_REQUEST_TRIGGER (50)

// Sets a trigger at the specified trigger slot index. See trigger_t and trigger_op_t.
// Set input_mask zero, op to TRIGGER_OP_DDD, or enabled to zero to disable. End index is exclusive.
//
// Format: [trig_index:8] [input_invert_mask:4 input_mask:4] [repeating:1 op_inv:1 op:6] [enabled:1 threshold_invert:1 require_both:1 input_audio:5] [threshold_hi:8
// threshold_lo:8] [min_period_ms_hi:8 min_period_ms_lo:8] [a_start_index:8] [a_end_index:8]
#define MSG_ID_UPDATE_TRIGGER (51)

// ----------------------------------------------------------------------------------------

// Requests the state of the triggers. Responds with a MSG_ID_TRIGGER_STATE message.
//
// Format: <none>
#define MSG_ID_REQUEST_TRIGGER_STATE (52)

// Trigger state as bit mask (LSB->MSB: TRIG_A1, TRIG_A2, TRIG_B1, TRIG_B2)
//
// Format: [state_mask:8]
#define MSG_ID_TRIGGER_STATE (53)

#endif // _MESSAGE_H