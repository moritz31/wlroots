#include <assert.h>
#include <stdlib.h>
#include <wayland-server.h>
#include <wlr/types/wlr_gamma_control.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/log.h>
#include "gamma-control-protocol.h"

static void resource_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void gamma_control_destroy(struct wlr_gamma_control *gamma_control) {
	wl_signal_emit(&gamma_control->events.destroy, gamma_control);
	wl_list_remove(&gamma_control->output_destroy_listener.link);
	wl_resource_set_user_data(gamma_control->resource, NULL);
	wl_list_remove(&gamma_control->link);
	free(gamma_control);
}

static void gamma_control_destroy_resource(struct wl_resource *resource) {
	struct wlr_gamma_control *gamma_control =
		wl_resource_get_user_data(resource);
	gamma_control_destroy(gamma_control);
}

static void gamma_control_handle_output_destroy(struct wl_listener *listener,
		void *data) {
	struct wlr_gamma_control *gamma_control =
		wl_container_of(listener, gamma_control, output_destroy_listener);
	gamma_control_destroy(gamma_control);
}

static void gamma_control_set_gamma(struct wl_client *client,
		struct wl_resource *gamma_control_resource, struct wl_array *red,
		struct wl_array *green, struct wl_array *blue) {
	struct wlr_gamma_control *gamma_control =
		wl_resource_get_user_data(gamma_control_resource);

	if (red->size != green->size || red->size != blue->size) {
		wl_resource_post_error(gamma_control_resource,
			GAMMA_CONTROL_ERROR_INVALID_GAMMA,
			"The gamma ramps don't have the same size");
		return;
	}

	uint32_t size = red->size / sizeof(uint16_t);
	uint16_t *r = (uint16_t *)red->data;
	uint16_t *g = (uint16_t *)green->data;
	uint16_t *b = (uint16_t *)blue->data;

	wlr_output_set_gamma(gamma_control->output, size, r, g, b);
}

static void gamma_control_reset_gamma(struct wl_client *client,
		struct wl_resource *gamma_control_resource) {
	// TODO
}

static const struct gamma_control_interface gamma_control_impl = {
	.destroy = resource_destroy,
	.set_gamma = gamma_control_set_gamma,
	.reset_gamma = gamma_control_reset_gamma,
};

static void gamma_control_manager_get_gamma_control(struct wl_client *client,
		struct wl_resource *gamma_control_manager_resource, uint32_t id,
		struct wl_resource *output_resource) {
	struct wlr_gamma_control_manager *gamma_control_manager =
		wl_resource_get_user_data(gamma_control_manager_resource);
	struct wlr_output *output = wl_resource_get_user_data(output_resource);

	struct wlr_gamma_control *gamma_control =
		calloc(1, sizeof(struct wlr_gamma_control));
	if (gamma_control == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	gamma_control->output = output;

	int version = wl_resource_get_version(gamma_control_manager_resource);
	gamma_control->resource = wl_resource_create(client,
		&gamma_control_interface, version, id);
	if (gamma_control->resource == NULL) {
		free(gamma_control);
		wl_client_post_no_memory(client);
		return;
	}
	wlr_log(L_DEBUG, "new gamma_control %p (res %p)", gamma_control,
		gamma_control->resource);
	wl_resource_set_implementation(gamma_control->resource,
		&gamma_control_impl, gamma_control, gamma_control_destroy_resource);

	wl_signal_init(&gamma_control->events.destroy);

	wl_signal_add(&output->events.destroy,
		&gamma_control->output_destroy_listener);
	gamma_control->output_destroy_listener.notify =
		gamma_control_handle_output_destroy;

	wl_list_insert(&gamma_control_manager->controls, &gamma_control->link);

	gamma_control_send_gamma_size(gamma_control->resource,
		wlr_output_get_gamma_size(output));
}

static struct gamma_control_manager_interface gamma_control_manager_impl = {
	.get_gamma_control = gamma_control_manager_get_gamma_control,
};

static void gamma_control_manager_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id) {
	struct wlr_gamma_control_manager *gamma_control_manager = data;
	assert(client && gamma_control_manager);

	struct wl_resource *resource = wl_resource_create(client,
		&gamma_control_manager_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &gamma_control_manager_impl,
		gamma_control_manager, NULL);
}

struct wlr_gamma_control_manager *wlr_gamma_control_manager_create(
		struct wl_display *display) {
	struct wlr_gamma_control_manager *gamma_control_manager =
		calloc(1, sizeof(struct wlr_gamma_control_manager));
	if (!gamma_control_manager) {
		return NULL;
	}
	struct wl_global *wl_global = wl_global_create(display,
		&gamma_control_manager_interface, 1, gamma_control_manager,
		gamma_control_manager_bind);
	if (!wl_global) {
		free(gamma_control_manager);
		return NULL;
	}
	gamma_control_manager->wl_global = wl_global;

	wl_list_init(&gamma_control_manager->controls);

	return gamma_control_manager;
}

void wlr_gamma_control_manager_destroy(
		struct wlr_gamma_control_manager *gamma_control_manager) {
	if (!gamma_control_manager) {
		return;
	}
	struct wlr_gamma_control *gamma_control, *tmp;
	wl_list_for_each_safe(gamma_control, tmp, &gamma_control_manager->controls,
			link) {
		gamma_control_destroy(gamma_control);
	}
	// TODO: this segfault (wl_display->registry_resource_list is not init)
	// wl_global_destroy(gamma_control_manager->wl_global);
	free(gamma_control_manager);
}
