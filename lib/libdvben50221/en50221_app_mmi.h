/*
    en50221 encoder An implementation for libdvb
    an implementation for the en50221 transport layer

    Copyright (C) 2004, 2005 Manu Abraham <abraham.manu@gmail.com>
    Copyright (C) 2005 Julian Scheel (julian at jusst dot de)
    Copyright (C) 2006 Andrew de Quincey (adq_dvb@lidskialf.net)

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as
    published by the Free Software Foundation; either version 2.1 of
    the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
*/

#ifndef __EN50221_APPLICATION_mmi_H__
#define __EN50221_APPLICATION_mmi_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <libdvben50221/en50221_app_utils.h>

#define EN50221_APP_MMI_RESOURCEID MKRID(64,1,1)

#define MMI_CLOSE_MMI_CMD_ID_IMMEDIATE                                  0x00
#define MMI_CLOSE_MMI_CMD_ID_DELAY                                      0x01

#define MMI_DISPLAY_CONTROL_CMD_ID_SET_MMI_MODE                         0x01
#define MMI_DISPLAY_CONTROL_CMD_ID_GET_DISPLAY_CHAR_TABLES              0x02
#define MMI_DISPLAY_CONTROL_CMD_ID_GET_INPUT_CHAR_TABLES                0x03
#define MMI_DISPLAY_CONTROL_CMD_ID_GET_OVERLAY_GFX_CHARACTERISTICS      0x04
#define MMI_DISPLAY_CONTROL_CMD_ID_GET_FULLSCREEN_GFX_CHARACTERISTICS   0x05

#define MMI_DISPLAY_REPLY_ID_MMI_MODE_ACK                               0x01
#define MMI_DISPLAY_REPLY_ID_LIST_DISPLAY_CHAR_TABLES                   0x02
#define MMI_DISPLAY_REPLY_ID_LIST_INPUT_CHAR_TABLES                     0x03
#define MMI_DISPLAY_REPLY_ID_LIST_OVERLAY_GFX_CHARACTERISTICS           0x04
#define MMI_DISPLAY_REPLY_ID_LIST_FULLSCREEN_GFX_CHARACTERISTICS        0x05
#define MMI_DISPLAY_REPLY_ID_UNKNOWN_CMD_ID                             0xF0
#define MMI_DISPLAY_REPLY_ID_UNKNOWN_MMI_MODE                           0xF1
#define MMI_DISPLAY_REPLY_ID_UNKNOWN_CHAR_TABLE                         0xF2

#define MMI_MODE_HIGH_LEVEL                                             0x01
#define MMI_MODE_LOW_LEVEL_OVERLAY_GFX                                  0x02
#define MMI_MODE_LOW_LEVEL_FULLSCREEN_GFX                               0x03

#define MMI_KEYPAD_CONTROL_CMD_ID_INTERCEPT_ALL                         0x01
#define MMI_KEYPAD_CONTROL_CMD_ID_IGNORE_ALL                            0x02
#define MMI_KEYPAD_CONTROL_CMD_ID_INTERCEPT_SELECTED                    0x03
#define MMI_KEYPAD_CONTROL_CMD_ID_IGNORE_SELECTED                       0x04
#define MMI_KEYPAD_CONTROL_CMD_ID_REJECT_KEYPRESS                       0x05

#define MMI_GFX_VIDEO_RELATION_NONE                                     0x00
#define MMI_GFX_VIDEO_RELATION_MATCHES_EXACTLY                          0x07

#define MMI_DISPLAY_MESSAGE_ID_OK                                       0x00
#define MMI_DISPLAY_MESSAGE_ID_ERROR                                    0x01
#define MMI_DISPLAY_MESSAGE_ID_OUT_OF_MEMORY                            0x02
#define MMI_DISPLAY_MESSAGE_ID_SUBTITLE_SYNTAX_ERROR                    0x03
#define MMI_DISPLAY_MESSAGE_ID_UNDEFINED_REGION                         0x04
#define MMI_DISPLAY_MESSAGE_ID_UNDEFINED_CLUT                           0x05
#define MMI_DISPLAY_MESSAGE_ID_UNDEFINED_OBJECT                         0x06
#define MMI_DISPLAY_MESSAGE_ID_INCOMPATABLE_OBJECT                      0x07
#define MMI_DISPLAY_MESSAGE_ID_UNKNOWN_CHARACTER                        0x08
#define MMI_DISPLAY_MESSAGE_ID_DISPLAY_CHANGED                          0x09

#define MMI_DOWNLOAD_REPLY_ID_OK                                        0x00
#define MMI_DOWNLOAD_REPLY_ID_NOT_OBJECT_SEGMENT                        0x01
#define MMI_DOWNLOAD_REPLY_ID_OUT_OF_MEMORY                             0x02

#define MMI_ANSW_ID_CANCEL                                              0x00
#define MMI_ANSW_ID_ANSWER                                              0x01

/**
 * A pixel depth as supplied with display_reply details
 */
struct en50221_app_mmi_pixel_depth {
	uint8_t display_depth;
	uint8_t pixels_per_byte;
	uint8_t region_overhead;
};

/**
 * Details returned with a display_reply
 */
struct en50221_app_mmi_display_reply_details {
	union {
		struct {
			uint16_t width;
			uint16_t height;
			uint8_t aspect_ratio;
			uint8_t gfx_relation_to_video;	/* one of MMI_GFX_VIDEO_RELATION_* */
			uint8_t multiple_depths;
			uint16_t display_bytes;
			uint8_t composition_buffer_bytes;
			uint8_t object_cache_bytes;
			uint8_t num_pixel_depths;
			struct en50221_app_mmi_pixel_depth *pixel_depths;
		} gfx;	/* MMI_DISPLAY_REPLY_ID_LIST_OVERLAY_GFX_CHARACTERISTICS or
				MMI_DISPLAY_REPLY_ID_LIST_FULLSCREEN_GFX_CHARACTERISTICS */

		struct {
			uint32_t table_length;
			uint8_t *table;
		} char_table;	/* MMI_DISPLAY_REPLY_ID_LIST_DISPLAY_CHAR_TABLES or
					MMI_DISPLAY_REPLY_ID_LIST_INPUT_CHAR_TABLES */

		struct {
			uint8_t mmi_mode;	/* one of the MMI_MODE_* values */
		} mode_ack;	/* for MMI_DISPLAY_REPLY_ID_MMI_MODE_ACK */
	} u;
};

/**
 * Pointer to a text string.
 */
struct en50221_app_mmi_text {
	uint8_t *text;
	uint32_t text_length;
};

/**
 * Type definition for close - called when we receive an mmi_close from a CAM.
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param session_number Session number concerned.
 * @param cmd_id One of the MMI_CLOSE_MMI_CMD_ID_* values.
 * @param delay Delay supplied with MMI_CLOSE_MMI_CMD_ID_DELAY.
 * @return 0 on success, -1 on failure.
 */
typedef int (*en50221_app_mmi_close_callback) (void *arg,
					       uint8_t slot_id,
					       uint16_t session_number,
					       uint8_t cmd_id,
					       uint8_t delay);

/**
 * Type definition for display_control callback.
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param session_number Session number concerned.
 * @param cmd_id One of the MMI_DISPLAY_CONTROL_CMD_ID_* values.
 * @param delay One of the MMI_MODE_* values.
 * @return 0 on success, -1 on failure.
 */
typedef int (*en50221_app_mmi_display_control_callback) (void *arg,
							 uint8_t slot_id,
							 uint16_t session_number,
							 uint8_t cmd_id,
							 uint8_t mmi_mode);

/**
 * Type definition for keypad_control callback.
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param session_number Session number concerned.
 * @param cmd_id One of the MMI_KEYPAD_CONTROL_CMD_ID_* values.
 * @param key_codes Pointer to the key codes.
 * @param key_codes_count Number of key codes.
 * @return 0 on success, -1 on failure.
 */
typedef int (*en50221_app_mmi_keypad_control_callback) (void *arg,
							uint8_t slot_id,
							uint16_t session_number,
							uint8_t cmd_id,
							uint8_t *key_codes,
							uint32_t key_codes_count);

/**
 * Type definition for subtitle_segment callback.
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param session_number Session number concerned.
 * @param segment Pointer to the segment data.
 * @param segment_size Size of segment data.
 * @return 0 on success, -1 on failure.
 */
typedef int (*en50221_app_mmi_subtitle_segment_callback) (void *arg,
							  uint8_t slot_id,
							  uint16_t session_number,
							  uint8_t *segment,
							  uint32_t segment_size);

/**
 * Type definition for scene_end_mark callback.
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param session_number Session number concerned.
 * @param decoder_continue_flag
 * @param scene_reveal_flag
 * @param send_scene_done
 * @param scene_tag
 * @return 0 on success, -1 on failure.
 */
typedef int (*en50221_app_mmi_scene_end_mark_callback) (void *arg,
							uint8_t slot_id,
							uint16_t session_number,
							uint8_t decoder_continue_flag,
							uint8_t scene_reveal_flag,
							uint8_t send_scene_done,
							uint8_t scene_tag);

/**
 * Type definition for scene_control callback.
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param session_number Session number concerned.
 * @param decoder_continue_flag
 * @param scene_reveal_flag
 * @param scene_tag
 * @return 0 on success, -1 on failure.
 */
typedef int (*en50221_app_mmi_scene_control_callback) (void *arg,
						       uint8_t slot_id,
						       uint16_t session_number,
						       uint8_t decoder_continue_flag,
						       uint8_t scene_reveal_flag,
						       uint8_t scene_tag);

/**
 * Type definition for subtitle_download callback.
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param session_number Session number concerned.
 * @param segment Pointer to the segment data.
 * @param segment_size Size of segment data.
 * @return 0 on success, -1 on failure.
 */
typedef int (*en50221_app_mmi_subtitle_download_callback) (void *arg,
							   uint8_t slot_id,
							   uint16_t session_number,
							   uint8_t *segment,
							   uint32_t segment_size);

/**
 * Type definition for flush_download callback.
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param session_number Session number concerned.
 * @return 0 on success, -1 on failure.
 */
typedef int (*en50221_app_mmi_flush_download_callback) (void *arg,
							uint8_t slot_id,
							uint16_t session_number);

/**
 * Type definition for enq callback.
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param session_number Session number concerned.
 * @param blind_answer 1=>Obscure text input in some manner,
 * @param expected_answer_length Expected max number of characters to be returned.
 * @param text Pointer to the text data.
 * @param text_size Size of text data.
 * @return 0 on success, -1 on failure.
 */
typedef int (*en50221_app_mmi_enq_callback) (void *arg,
					     uint8_t slot_id,
					     uint16_t session_number,
					     uint8_t blind_answer,
					     uint8_t expected_answer_length,
					     uint8_t * text,
					     uint32_t text_size);

/**
 * Type definition for menu callback.
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param session_number Session number concerned.
 * @param title Title text.
 * @param sub_title Sub-Title text.
 * @param bottom Bottom text.
 * @param item_count Number of text elements in items.
 * @param items Pointer to array of en50221_app_mmi_text structures which are standard menu choices,
 * @param item_raw_length Length of item raw data.
 * @param items_raw If nonstandard items were supplied, pointer to their data.
 * @return 0 on success, -1 on failure.
 */
typedef int (*en50221_app_mmi_menu_callback) (void *arg,
					      uint8_t slot_id,
					      uint16_t session_number,
					      struct en50221_app_mmi_text *title,
					      struct en50221_app_mmi_text *sub_title,
					      struct en50221_app_mmi_text *bottom,
					      uint32_t item_count,
					      struct en50221_app_mmi_text *items,
					      uint32_t item_raw_length,
					      uint8_t *items_raw);

/**
 * Type definition for list callback.
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param session_number Session number concerned.
 * @param title Title text.
 * @param sub_title Sub-Title text.
 * @param bottom Bottom text.
 * @param item_count Number of text elements in items.
 * @param items Pointer to array of en50221_app_mmi_text structures which are standard menu choices,
 * @param item_raw_length Length of item raw data.
 * @param items_raw If nonstandard items were supplied, pointer to their data.
 * @return 0 on success, -1 on failure.
 */
typedef int (*en50221_app_mmi_list_callback) (void *arg,
					      uint8_t slot_id,
					      uint16_t session_number,
					      struct en50221_app_mmi_text *title,
					      struct en50221_app_mmi_text *sub_title,
					      struct en50221_app_mmi_text *bottom,
					      uint32_t item_count,
					      struct en50221_app_mmi_text *items,
					      uint32_t item_raw_length,
					      uint8_t *items_raw);

/**
 * Opaque type representing a mmi resource.
 */
struct en50221_app_mmi;

/**
 * Create an instance of the mmi resource.
 *
 * @param funcs Send functions to use.
 * @return Instance, or NULL on failure.
 */
extern struct en50221_app_mmi *en50221_app_mmi_create(struct en50221_app_send_functions *funcs);

/**
 * Destroy an instance of the mmi resource.
 *
 * @param mmi Instance to destroy.
 */
extern void en50221_app_mmi_destroy(struct en50221_app_mmi *mmi);

/**
 * Informs the mmi object that a session to it has been closed - cleans up internal state.
 *
 * @param mmi mmi resource instance.
 * @param session_number The session concerned.
 */
extern void en50221_app_mmi_clear_session(struct en50221_app_mmi *mmi,
					  uint16_t session_number);

/**
 * Register the callback for when we receive an mmi_close request.
 *
 * @param mmi mmi resource instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_app_mmi_register_close_callback(struct en50221_app_mmi *mmi,
						    en50221_app_mmi_close_callback callback,
						    void *arg);

/**
 * Register the callback for when we receive a display control request.
 *
 * @param mmi mmi resource instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_app_mmi_register_display_control_callback(struct en50221_app_mmi *mmi,
							      en50221_app_mmi_display_control_callback callback,
							      void *arg);

/**
 * Register the callback for when we receive a keypad control request.
 *
 * @param mmi mmi resource instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_app_mmi_register_keypad_control_callback(struct en50221_app_mmi *mmi,
							     en50221_app_mmi_keypad_control_callback callback,
							     void *arg);

/**
 * Register the callback for when we receive a subtitle segment request.
 *
 * @param mmi mmi resource instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_app_mmi_register_subtitle_segment_callback(struct en50221_app_mmi *mmi,
							       en50221_app_mmi_subtitle_segment_callback callback,
							       void *arg);

/**
 * Register the callback for when we receive a scene end mark request.
 *
 * @param mmi mmi resource instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_app_mmi_register_scene_end_mark_callback(struct en50221_app_mmi *mmi,
							     en50221_app_mmi_scene_end_mark_callback callback,
							     void *arg);

/**
 * Register the callback for when we receive a scene control request.
 *
 * @param mmi mmi resource instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_app_mmi_register_scene_control_callback(struct en50221_app_mmi *mmi,
							    en50221_app_mmi_scene_control_callback callback,
							    void *arg);

/**
 * Register the callback for when we receive a subtitle download request.
 *
 * @param mmi mmi resource instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_app_mmi_register_subtitle_download_callback(struct en50221_app_mmi *mmi,
							        en50221_app_mmi_subtitle_download_callback callback,
							        void *arg);

/**
 * Register the callback for when we receive a flush download request.
 *
 * @param mmi mmi resource instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_app_mmi_register_flush_download_callback(struct en50221_app_mmi *mmi,
							     en50221_app_mmi_flush_download_callback callback,
							     void *arg);

/**
 * Register the callback for when we receive an enq request.
 *
 * @param mmi mmi resource instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_app_mmi_register_enq_callback(struct en50221_app_mmi *mmi,
						  en50221_app_mmi_enq_callback callback,
						  void *arg);

/**
 * Register the callback for when we receive a menu request.
 *
 * @param mmi mmi resource instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_app_mmi_register_menu_callback(struct en50221_app_mmi *mmi,
						   en50221_app_mmi_menu_callback callback,
						   void *arg);

/**
 * Register the callback for when we receive a list request.
 *
 * @param mmi mmi resource instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_app_mmi_register_list_callback(struct en50221_app_mmi *mmi,
						   en50221_app_mmi_list_callback callback,
						   void *arg);

/**
 * Send an mmi_close to the cam.
 *
 * @param mmi mmi resource instance.
 * @param session_number Session number to send it on.
 * @param cmd_id One of the MMI_CLOSE_MMI_CMD_ID_* values.
 * @param delay Delay to use if MMI_CLOSE_MMI_CMD_ID_DELAY specified.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_mmi_close(struct en50221_app_mmi *mmi,
				 uint16_t session_number,
				 uint8_t cmd_id, uint8_t delay);

/**
 * Send a display_reply to the cam.
 *
 * @param mmi mmi resource instance.
 * @param session_number Session number to send it on.
 * @param reply_id One of the MMI_DISPLAY_REPLY_ID_* values.
 * @param details The details of the reply - can be NULL if the chosen reply_id does not need it.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_mmi_display_reply(struct en50221_app_mmi *mmi,
					 uint16_t session_number,
					 uint8_t reply_id,
					 struct en50221_app_mmi_display_reply_details *details);

/**
 * Send a keypress to the cam.
 *
 * @param mmi mmi resource instance.
 * @param session_number Session number to send it on.
 * @param keycode The keycode.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_mmi_keypress(struct en50221_app_mmi *mmi,
				    uint16_t session_number,
				    uint8_t keycode);

/**
 * Send a display message to the cam.
 *
 * @param mmi mmi resource instance.
 * @param session_number Session number to send it on.
 * @param display_message_id One of the MMI_DISPLAY_MESSAGE_ID_* values.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_mmi_display_message(struct en50221_app_mmi *mmi,
					   uint16_t session_number,
					   uint8_t display_message_id);

/**
 * Send a scene done message to the cam.
 *
 * @param mmi mmi resource instance.
 * @param session_number Session number to send it on.
 * @param decoder_continue Copy of flag in scene_end_mark.
 * @param scene_reveal Copy of flag in scene_end_mark.
 * @param scene_tag Scene tag this responds to.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_mmi_scene_done(struct en50221_app_mmi *mmi,
				      uint16_t session_number,
				      uint8_t decoder_continue,
				      uint8_t scene_reveal,
				      uint8_t scene_tag);

/**
 * Send a download reply to the cam.
 *
 * @param mmi mmi resource instance.
 * @param session_number Session number to send it on.
 * @param object_id Object id.
 * @param download_reply_id One of the MMI_DOWNLOAD_REPLY_ID_* values.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_mmi_download_reply(struct en50221_app_mmi *mmi,
					  uint16_t session_number,
					  uint16_t object_id,
					  uint8_t download_reply_id);

/**
 * Send an answ to the cam.
 *
 * @param mmi mmi resource instance.
 * @param session_number Session number to send it on.
 * @param answ_id One of the MMI_ANSW_ID_* values.
 * @param text The text if MMI_ANSW_ID_ANSWER.
 * @param text_count Length of text.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_mmi_answ(struct en50221_app_mmi *mmi,
				uint16_t session_number,
				uint8_t answ_id,
				uint8_t * text,
				uint32_t text_count);

/**
 * Send a menu answ to the cam.
 *
 * @param mmi mmi resource instance.
 * @param session_number Session number to send it on.
 * @param choice_ref Option chosen by user (0=>canceled).
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_mmi_menu_answ(struct en50221_app_mmi *mmi,
				     uint16_t session_number,
				     uint8_t choice_ref);

/**
 * Pass data received for this resource into it for parsing.
 *
 * @param mmi mmi instance.
 * @param slot_id Slot ID concerned.
 * @param session_number Session number concerned.
 * @param resource_id Resource ID concerned.
 * @param data The data.
 * @param data_length Length of data in bytes.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_mmi_message(struct en50221_app_mmi *mmi,
				   uint8_t slot_id,
				   uint16_t session_number,
				   uint32_t resource_id,
				   uint8_t *data,
				   uint32_t data_length);

#ifdef __cplusplus
}
#endif
#endif
