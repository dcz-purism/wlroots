#include "osk.h"

#include "input-method-unstable-v1-protocol.h"

static void noop() {}

static const struct zwp_input_panel_surface_v1_interface
		input_panel_surface_impl = {
	.set_toplevel = noop,
	.set_overlay_panel = noop,
};

static void get_input_panel_surface(struct wl_client *client,
		struct wl_resource *resource, uint32_t id,
		struct wl_resource *surface) {
	int version = wl_resource_get_version(resource);
	struct wl_resource *wl_resource = wl_resource_create(client,
		&zwp_input_panel_surface_v1_interface, version, id);
	if (wl_resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(wl_resource, &input_panel_surface_impl,
		NULL, NULL);
}

static const struct zwp_input_panel_v1_interface input_panel_impl = {
	.get_input_panel_surface = get_input_panel_surface,
};

static void input_panel_bind(struct wl_client *wl_client, void *data,
		uint32_t version, uint32_t id) {
	assert(wl_client);

	struct wl_resource *wl_resource = wl_resource_create(wl_client,
		&zwp_input_panel_v1_interface, version, id);
	if (wl_resource == NULL) {
		wl_client_post_no_memory(wl_client);
		return;
	}
	wl_resource_set_implementation(wl_resource, &input_panel_impl, NULL, NULL);
}

struct roots_input_panel *roots_input_panel_create(struct wl_display *display) {
	struct roots_input_panel *panel = calloc(1,
											 sizeof(struct roots_input_panel));
	if (!panel) {
		return NULL;
	}
	panel->panel = wl_global_create(display, &zwp_input_panel_v1_interface, 1,
		panel, input_panel_bind);
	return panel;
}
