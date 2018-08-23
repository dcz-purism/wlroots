#ifndef ROOTSTON_TEXT_INPUT_H
#define ROOTSTON_TEXT_INPUT_H

#include <wlr/types/wlr_text_input_v3.h>
#include <wlr/types/wlr_input_method_v2.h>
#include "rootston/seat.h"

struct roots_input_method_relay {
	struct roots_seat *seat;

	struct wl_list text_inputs; // roots_text_input::link
	struct wlr_input_method_v2 *input_method; // doesn't have to be present

	struct wl_listener text_input_new;
	struct wl_listener text_input_enable;
	struct wl_listener text_input_commit;
	struct wl_listener text_input_disable;
	struct wl_listener text_input_destroy;

	struct wl_listener input_method_new;
	struct wl_listener input_method_commit;
	struct wl_listener input_method_destroy;
};

struct roots_text_input {
	struct roots_input_method_relay *relay;

	struct wlr_text_input_v3 *input;

	bool enabled; // client requested state
	bool sent_enter; // server requested state
	bool active; // active focus

	struct wl_list link;
};

void roots_input_method_relay_init(struct roots_seat *seat,
	struct roots_input_method_relay *relay);

struct roots_text_input *roots_text_input_create(
	struct roots_input_method_relay *relay,
	struct wlr_text_input_v3 *text_input);

#endif
