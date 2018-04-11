#ifndef ROOTSTON_OSK_H
#define ROOTSTON_OSK_H

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include <wayland-server-core.h>

#include "rootston/desktop.h"

struct roots_input_panel {
	struct roots_desktop *desktop; // FIXME - remove when views come
	struct wl_global *panel;
	struct wlr_surface *surface;
	struct {
		struct wl_signal new_surface;
	} events;
};

struct roots_input_panel *roots_input_panel_create(
		struct roots_desktop *desktop, struct wl_display *display);

void input_method_bind(struct wl_client *wl_client, void *data,
		uint32_t version, uint32_t id);
#endif
