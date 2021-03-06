#include <stdlib.h>
#include <assert.h>
#include <libinput.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/util/log.h>
#include "backend/libinput.h"

struct wlr_libinput_keyboard {
	struct wlr_keyboard wlr_keyboard;
	struct libinput_device *libinput_dev;
};

static void wlr_libinput_keyboard_set_leds(struct wlr_keyboard *wlr_kb, uint32_t leds) {
	struct wlr_libinput_keyboard *wlr_libinput_kb = (struct wlr_libinput_keyboard *)wlr_kb;
	libinput_device_led_update(wlr_libinput_kb->libinput_dev, leds);
}

static void wlr_libinput_keyboard_destroy(struct wlr_keyboard *wlr_kb) {
	struct wlr_libinput_keyboard *wlr_libinput_kb =
		(struct wlr_libinput_keyboard *)wlr_kb;
	libinput_device_unref(wlr_libinput_kb->libinput_dev);
}

struct wlr_keyboard_impl impl = {
	.destroy = wlr_libinput_keyboard_destroy,
	.led_update = wlr_libinput_keyboard_set_leds
};

struct wlr_keyboard *wlr_libinput_keyboard_create(
		struct libinput_device *libinput_dev) {
	assert(libinput_dev);
	struct wlr_libinput_keyboard *wlr_libinput_kb;
	if (!(wlr_libinput_kb= calloc(1, sizeof(struct wlr_libinput_keyboard)))) {
		return NULL;
	}
	wlr_libinput_kb->libinput_dev = libinput_dev;
	libinput_device_ref(libinput_dev);
	libinput_device_led_update(libinput_dev, 0);
	struct wlr_keyboard *wlr_kb = &wlr_libinput_kb->wlr_keyboard;
	wlr_keyboard_init(wlr_kb, &impl);
	return wlr_kb;
}

void handle_keyboard_key(struct libinput_event *event,
		struct libinput_device *libinput_dev) {
	struct wlr_input_device *wlr_dev =
		get_appropriate_device(WLR_INPUT_DEVICE_KEYBOARD, libinput_dev);
	if (!wlr_dev) {
		wlr_log(L_DEBUG, "Got a keyboard event for a device with no keyboards?");
		return;
	}
	struct libinput_event_keyboard *kbevent =
		libinput_event_get_keyboard_event(event);
	struct wlr_event_keyboard_key wlr_event = { 0 };
	wlr_event.time_msec =
		usec_to_msec(libinput_event_keyboard_get_time_usec(kbevent));
	wlr_event.keycode = libinput_event_keyboard_get_key(kbevent);
	enum libinput_key_state state = 
		libinput_event_keyboard_get_key_state(kbevent);
	switch (state) {
	case LIBINPUT_KEY_STATE_RELEASED:
		wlr_event.state = WLR_KEY_RELEASED;
		break;
	case LIBINPUT_KEY_STATE_PRESSED:
		wlr_event.state = WLR_KEY_PRESSED;
		break;
	}
	wlr_event.update_state = true;
	wlr_keyboard_notify_key(wlr_dev->keyboard, &wlr_event);
}
