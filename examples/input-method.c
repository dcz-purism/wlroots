#define _POSIX_C_SOURCE 200809L
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <wlr/render/egl.h>
#include "input-method-unstable-v2-client-protocol.h"
#include "xdg-shell-client-protocol.h"

#include <linux/input-event-codes.h>

/**
 * Usage: input-method [seconds]
 * Creates an input method using the input-method protocol.
 * Sends a couple of commands to the compositor whenever a new text input
 * appears. Displays received messages.
 * Time is optional and defines the delay between commits.
 */

static int sleeptime = 0;

static struct wl_display *display = NULL;
static struct wl_compositor *compositor = NULL;
static struct wl_seat *seat = NULL;
static struct zwp_input_method_manager_v2 *input_method_manager = NULL;
static struct zwp_input_method_v2 *input_method = NULL;

static uint32_t serial = 0;
bool active = false;
bool pending_active = false;
bool unavailable = false;

static void handle_content_type(void *data,
		struct zwp_input_method_v2 *zwp_input_method_v2,
		uint32_t hint, uint32_t purpose) {
	// TODO: convert into names
	printf("hint: %x, purpose: %x\n", hint, purpose);
}

static void handle_surrounding_text(void *data,
		struct zwp_input_method_v2 *zwp_input_method_v2,
		const char *text, uint32_t cursor, uint32_t anchor) {
	printf("Surrounding text: %s\n", text);
	if (cursor < anchor) {
		printf("Selection: %s\n", strndup(&text[cursor], anchor - cursor)); // TODO: leak
	} else {
		printf("Cursor after %d: %s\n", cursor, strndup(text, cursor)); // TODO: leak
	}
}

static void handle_text_change_cause(void *data,
		struct zwp_input_method_v2 *zwp_input_method_v2,
		uint32_t cause) {
	if (cause) {
		printf("Text modified externally\n");
	}
}


static void handle_activate(void *data,
		struct zwp_input_method_v2 *zwp_input_method_v2) {
	pending_active = true;
}

static void handle_deactivate(void *data,
		struct zwp_input_method_v2 *zwp_input_method_v2) {
	pending_active = false;
}

static void handle_unavailable(void *data,
		struct zwp_input_method_v2 *zwp_input_method_v2) {
	printf("IM disappeared");
	// FIXME: destroy
	zwp_input_method_v2_destroy(zwp_input_method_v2);
	input_method = NULL;
}

static void im_activate(void *data,
		struct zwp_input_method_v2 *id) {
	printf("Activated IM\n");
	wl_display_roundtrip(display);
	sleep(sleeptime);
	wl_display_roundtrip(display);
	zwp_input_method_v2_preedit_string(input_method, "Preedit", 2, 4);
	zwp_input_method_v2_commit(input_method, serial);
	wl_display_roundtrip(display);
	sleep(sleeptime);
	wl_display_roundtrip(display);
	zwp_input_method_v2_preedit_string(input_method, "Præedit2", strlen("Pr"), strlen("Præed"));
	zwp_input_method_v2_commit_string(input_method, "_Commit_");
	zwp_input_method_v2_commit(input_method, serial);
	wl_display_roundtrip(display);
	return;
	sleep(sleeptime);
	wl_display_roundtrip(display);
	zwp_input_method_v2_commit_string(input_method, "_CommitNoPreed_");
	zwp_input_method_v2_commit(input_method, serial);
	wl_display_roundtrip(display);
	sleep(sleeptime);
	wl_display_roundtrip(display);

	zwp_input_method_v2_commit_string(input_method, "_WaitNo_");
	zwp_input_method_v2_delete_surrounding_text(input_method, strlen("_CommitNoPreed_"), 0);
	zwp_input_method_v2_commit(input_method, serial);
	wl_display_roundtrip(display);
	sleep(sleeptime);
	wl_display_roundtrip(display);
	zwp_input_method_v2_preedit_string(input_method, "PreedWithDel", strlen("Preed"), strlen("Preed"));
	zwp_input_method_v2_delete_surrounding_text(input_method, strlen("_WaitNo_"), 0);
	zwp_input_method_v2_commit(input_method, serial);
	wl_display_roundtrip(display);
	sleep(sleeptime);
	wl_display_roundtrip(display);
	zwp_input_method_v2_delete_surrounding_text(input_method, strlen("mit_"), 0);
	zwp_input_method_v2_commit(input_method, serial);
}

static void im_deactivate(void *data,
		struct zwp_input_method_v2 *context) {
	printf("Deactivated IM\n");
}

static void handle_done(void *data,
		struct zwp_input_method_v2 *zwp_input_method_v2) {
	bool prev_active = active;
	serial++;
	printf("FIXME: apply changes\n");
	active = pending_active;
	if (active && !prev_active) {
		im_activate(data, zwp_input_method_v2);
	} else if (!active && prev_active) {
		im_deactivate(data, zwp_input_method_v2);
	}
}

static const struct zwp_input_method_v2_listener im_listener = {
	.activate = handle_activate,
	.deactivate = handle_deactivate,
	.surrounding_text = handle_surrounding_text,
	.text_change_cause = handle_text_change_cause,
	.content_type = handle_content_type,
	.done = handle_done,
	.unavailable = handle_unavailable,
};

static void handle_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version) {
	if (strcmp(interface, "wl_compositor") == 0) {
		compositor = wl_registry_bind(registry, name,
			&wl_compositor_interface, 1);
	} else if (strcmp(interface, zwp_input_method_manager_v2_interface.name) == 0) {
		input_method_manager = wl_registry_bind(registry, name,
			&zwp_input_method_manager_v2_interface, 1);
	} else if (strcmp(interface, wl_seat_interface.name) == 0) {
		seat = wl_registry_bind(registry, name, &wl_seat_interface, version);
	}
}

static void handle_global_remove(void *data, struct wl_registry *registry,
		uint32_t name) {
	// who cares
}

static const struct wl_registry_listener registry_listener = {
	.global = handle_global,
	.global_remove = handle_global_remove,
};

int main(int argc, char **argv) {
	if (argc > 1) {
		sleeptime = atoi(argv[1]);
	}
	display = wl_display_connect(NULL);
	if (display == NULL) {
		fprintf(stderr, "Failed to create display\n");
		return EXIT_FAILURE;
	}

	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_dispatch(display);
	wl_display_roundtrip(display);

	if (compositor == NULL) {
		fprintf(stderr, "wl-compositor not available\n");
		return EXIT_FAILURE;
	}
	if (input_method_manager == NULL) {
		fprintf(stderr, "input-method not available\n");
		return EXIT_FAILURE;
	}
	if (seat == NULL) {
		fprintf(stderr, "seat not available\n");
		return EXIT_FAILURE;
	}

	input_method = zwp_input_method_manager_v2_get_input_method(
		input_method_manager, seat);

	zwp_input_method_v2_add_listener(input_method, &im_listener, NULL);

	wl_display_roundtrip(display);

	while (wl_display_dispatch(display) != -1) {
		// This space intentionally left blank
	}

	return EXIT_SUCCESS;
}
