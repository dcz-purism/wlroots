#include <wayland-util.h>
#include "input-method-unstable-v1-protocol.h"
#include "text-input-unstable-v1-protocol.h"
#include <wlr/util/log.h>
#include "util/signal.h"
#include "rootston/osk.h"
#include "rootston/output.h"

static void noop() {}

static const struct zwp_input_panel_surface_v1_interface
		input_panel_surface_impl = {
	.set_toplevel = noop,
	.set_overlay_panel = noop,
};

void commit_string(struct wl_client *client, struct wl_resource *resource,
				   uint32_t serial, const char *text) {
	struct roots_input_panel *panel = wl_resource_get_user_data(resource);
	struct wl_resource *text_input_res = panel->desktop->text_input_res;
	zwp_text_input_v1_send_commit_string(text_input_res,
		panel->desktop->text_input_last_serial, text);
}

void preedit_string(struct wl_client *client, struct wl_resource *resource,
					uint32_t serial, const char *text, const char *commit) {
	struct roots_input_panel *panel = wl_resource_get_user_data(resource);
	struct wl_resource *text_input_res = panel->desktop->text_input_res;
	zwp_text_input_v1_send_preedit_string(text_input_res,
		panel->desktop->text_input_last_serial, text, commit);
}

static const struct zwp_input_method_context_v1_interface input_context_impl = {
	.destroy = noop,
	.commit_string = commit_string,
	.preedit_string = preedit_string,
	.preedit_styling = noop,
	.preedit_cursor = noop,
	.delete_surrounding_text = noop,
	.cursor_position = noop,
	.modifiers_map = noop,
	.keysym = noop,
	.grab_keyboard = noop,
	.key = noop,
	.modifiers = noop,
	.language = noop,
	.text_direction = noop,
};

static void handle_wlr_surface_committed(struct wlr_surface *wlr_surface,
		void *role_data) {
	struct wlr_input_panel_surface *surface = role_data;
	if (!surface->configured &&
			wlr_surface_has_buffer(surface->surface)) {
		surface->configured = true;
//		wlr_signal_emit_safe(&surface->panel->events.new_surface, surface);
	}
}

static const struct zwp_input_panel_v1_interface input_panel_impl;

static struct roots_input_panel *panel_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &zwp_input_panel_v1_interface,
		&input_panel_impl));
	return wl_resource_get_user_data(resource);
}

static void handle_surface_commit(struct wl_listener *listener, void *data) {
	struct roots_input_panel_surface *roots_surface =
		wl_container_of(listener, roots_surface, surface_commit);
	struct roots_view *view = roots_surface->view;
	struct wlr_input_panel_surface *surface = view->input_panel_surface;

// let kb choose its preferred size for now (resize manually later)
	int width = surface->surface->current->width;
	int height = surface->surface->current->height;

	view_update_size(view, width, height);
	view_update_position(view, 100, 100);

	view_apply_damage(view);
}

// FIXME: cargo-culted from desktop.h
static void view_update_output(const struct roots_view *view,
		const struct wlr_box *before) {
	struct roots_desktop *desktop = view->desktop;
	struct roots_output *output;
	struct wlr_box box;
	view_get_box(view, &box);
	wl_list_for_each(output, &desktop->outputs, link) {
		bool intersected = before != NULL && wlr_output_layout_intersects(
			desktop->layout, output->wlr_output, before);
		bool intersects = wlr_output_layout_intersects(desktop->layout,
			output->wlr_output, &box);
		if (intersected && !intersects) {
			wlr_surface_send_leave(view->wlr_surface, output->wlr_output);
		}
		if (!intersected && intersects) {
			wlr_surface_send_enter(view->wlr_surface, output->wlr_output);
		}
	}
}


void handle_input_panel_surface(struct roots_desktop *desktop,
		struct wlr_input_panel_surface *surface) {
	struct roots_input_panel_surface *roots_surface =
		calloc(1, sizeof(struct roots_input_panel_surface));
	if (!roots_surface) {
		return;
	}
	//roots_surface->destroy.notify = handle_destroy;
//	wl_signal_add(&surface->events.destroy, &roots_surface->destroy);
// TODO: show/hide?
	roots_surface->surface_commit.notify = handle_surface_commit;
	wl_signal_add(&surface->surface->events.commit, &roots_surface->surface_commit);

	struct roots_view *view = calloc(1, sizeof(struct roots_view));
	if (!view) {
		free(roots_surface);
		return;
	}
	view->type = ROOTS_INPUT_PANEL_VIEW;
	view->width = surface->surface->current->width;
	view->height = surface->surface->current->height;
	view->x = 100;
	view->y = 100;

	view->input_panel_surface = surface;
	view->roots_input_panel_surface = roots_surface;
	view->wlr_surface = surface->surface;
//	view->resize = resize;
//	view->close = close;
	roots_surface->view = view;
	view_init(view, desktop);
	wl_list_insert(&desktop->views, &view->link);
	view_update_output(view, NULL);

	//view_apply_damage(view); // TODO: maybe do at activate
}


static void get_input_panel_surface(struct wl_client *client,
		struct wl_resource *panel_resource, uint32_t id,
		struct wl_resource *surface_resource) {
	struct wlr_surface *surface = wlr_surface_from_resource(surface_resource);
	if (wlr_surface_set_role(surface, "zwp-input-panel-role",
			panel_resource, WL_SHELL_ERROR_ROLE)) {
		return;
	}

	struct wl_resource *wl_resource = wl_resource_create(client,
		&zwp_input_panel_surface_v1_interface,
		wl_resource_get_version(panel_resource), id);
	if (wl_resource == NULL) {
		wl_resource_post_no_memory(panel_resource);
		return;
	}
	struct roots_input_panel *panel = panel_from_resource(panel_resource);
	struct wlr_input_panel_surface *panel_surface =
		calloc(1, sizeof(struct wlr_input_panel_surface));
	if (panel_surface == NULL) {
		wl_resource_post_no_memory(panel_resource);
		return;
	}
	panel_surface->surface = surface;
	panel_surface->panel = panel; // be able to tell the panel about changes
	panel->surface = surface; // FIXME: turn into panel_surface
/*	struct roots_output *output;
	wl_list_for_each(output, &panel->desktop->outputs, link) {
		damage_whole_surface(panel->surface, 0, 0, 0, output); // FIXME: use views
	}*/
	wl_resource_set_implementation(wl_resource, &input_panel_surface_impl,
		NULL, NULL);

	wlr_surface_set_role_committed(surface, &handle_wlr_surface_committed,
		panel_surface);


	struct wl_resource *context = wl_resource_create(client,
		&zwp_input_method_context_v1_interface,
		wl_resource_get_version(panel_resource), 0);
	if (wl_resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(context, &input_context_impl, panel, NULL);
	// FIXME: activate input method only when something needs it

	handle_input_panel_surface(panel->desktop, panel_surface);
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
	wl_resource_set_implementation(wl_resource, &input_panel_impl, data, NULL);
}

struct roots_input_panel *roots_input_panel_create(
		struct roots_desktop *desktop, struct wl_display *display) {
	struct roots_input_panel *panel = calloc(1,
											 sizeof(struct roots_input_panel));
	if (!panel) {
		return NULL;
	}
	panel->desktop = desktop;
	panel->panel = wl_global_create(display, &zwp_input_panel_v1_interface, 1,
		panel, input_panel_bind);
	wl_signal_init(&panel->events.new_surface);
	return panel;
}

void input_method_bind(struct wl_client *wl_client, void *data,
		uint32_t version, uint32_t id) {
	assert(wl_client);

	struct wl_resource **res = data;
	struct wl_resource *wl_resource = wl_resource_create(wl_client,
		&zwp_input_method_v1_interface, version, id);
	if (wl_resource == NULL) {
		wl_client_post_no_memory(wl_client);
		return;
	}
	*res = wl_resource;
}
