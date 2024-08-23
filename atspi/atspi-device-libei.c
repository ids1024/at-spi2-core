/*
 * AT-SPI - Assistive Technology Service Provider Interface
 * (Gnome Accessibility Project; http://developer.gnome.org/projects/gap)
 *
 * Copyright 2024 System76 Inc.
 *
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "atspi-device-libei.h"
#include "glib-unix.h"

#include <libei.h>
#include <sys/mman.h>
#include <xkbcommon/xkbcommon.h>

typedef struct _AtspiDeviceLibeiPrivate AtspiDeviceLibeiPrivate;
struct _AtspiDeviceLibeiPrivate
{
  struct ei *ei;
  int source_id;
  struct xkb_context *xkb_context;
  struct xkb_keymap *xkb_keymap;
  struct xkb_state *xkb_state;
};

G_DEFINE_TYPE_WITH_CODE (AtspiDeviceLibei, atspi_device_libei, ATSPI_TYPE_DEVICE, G_ADD_PRIVATE (AtspiDeviceLibei))

static gboolean dispatch(gint fd, GIOCondition condition, gpointer user_data) {
  AtspiDeviceLibei *device = user_data;
  AtspiDeviceLibeiPrivate *priv = atspi_device_libei_get_instance_private (device);

  if (!(condition & G_IO_IN))
    return TRUE;


  struct ei_event *event;

  ei_dispatch(priv->ei);

  while ((event = ei_get_event(priv->ei)) != NULL) {
    switch (ei_event_get_type(event)) {
      case EI_EVENT_SEAT_ADDED:
	ei_seat_bind_capabilities(ei_event_get_seat(event), EI_DEVICE_CAP_KEYBOARD, NULL);
	break;
      // TODO multiple devices
      case EI_EVENT_DEVICE_ADDED:
        struct ei_keymap *keymap = ei_device_keyboard_get_keymap(ei_event_get_device(event));
        int format = ei_keymap_get_type(keymap);
        int fd = ei_keymap_get_fd(keymap);
        size_t size = ei_keymap_get_size(keymap);
        char *buffer = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
        priv->xkb_keymap = xkb_keymap_new_from_buffer(priv->xkb_context, buffer, size, format, XKB_KEYMAP_COMPILE_NO_FLAGS);
        munmap(buffer, size);
        priv->xkb_state = xkb_state_new(priv->xkb_keymap);
	break;
      case EI_EVENT_KEYBOARD_MODIFIERS:
        uint32_t group = ei_event_keyboard_get_xkb_group(event);
        xkb_state_update_mask(priv->xkb_state,
            ei_event_keyboard_get_xkb_mods_depressed(event),
            ei_event_keyboard_get_xkb_mods_latched(event),
            ei_event_keyboard_get_xkb_mods_locked(event),
            group, group, group);
        break;
      case EI_EVENT_KEYBOARD_KEY:
        uint32_t keycode = ei_event_keyboard_get_key(event) + 8;
        bool pressed = ei_event_keyboard_get_key_is_press(event);
        int keysym = xkb_state_key_get_one_sym(priv->xkb_state, keycode);
        xkb_mod_mask_t modifiers = xkb_state_serialize_mods(priv->xkb_state, XKB_STATE_MODS_EFFECTIVE);
        gchar text[16];
        xkb_state_key_get_utf8(priv->xkb_state, keycode, text, 16);
        atspi_device_notify_key (ATSPI_DEVICE (device), pressed, keycode, keysym, modifiers, text);
        break;
      default:
        break;
    }
    ei_event_unref(event);
  }

  return TRUE;
}

static gboolean
atspi_device_libei_add_key_grab (AtspiDevice *device, AtspiKeyDefinition *kd)
{
  AtspiDeviceLibei *libei_device = ATSPI_DEVICE_LIBEI (device);
  AtspiDeviceLibeiPrivate *priv = atspi_device_libei_get_instance_private (libei_device);

  const char *keycode_name = xkb_keymap_key_get_name(priv->xkb_keymap, kd->keycode);
  printf("GRAB(%d [%s], %d)\n", kd->keycode, keycode_name, kd->modifiers);

  return TRUE;
}

static void
atspi_device_libei_remove_key_grab (AtspiDevice *device, guint id)
{
  AtspiKeyDefinition *kd;
  kd = atspi_device_get_grab_by_id (device, id);
  printf("UNGRAB(%d, %d)\n", kd->keycode, kd->modifiers);
}

static void
atspi_device_libei_finalize (GObject *object)
{
  AtspiDeviceLibei *device = ATSPI_DEVICE_LIBEI (object);
  AtspiDeviceLibeiPrivate *priv = atspi_device_libei_get_instance_private (device);

  g_source_remove(priv->source_id);
  ei_unref(priv->ei);
}

static void
atspi_device_libei_init (AtspiDeviceLibei *device)
{
  AtspiDeviceLibeiPrivate *priv = atspi_device_libei_get_instance_private (device);

  priv->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  // XXX
  struct xkb_rule_names names = {
    .rules = "",
    .model = "pc105",
    .layout = "us",
    .variant = "",
    .options = "",
  };
  priv->xkb_keymap = xkb_keymap_new_from_names(priv->xkb_context, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);

  priv->ei = ei_new_receiver(NULL);
  // TODO secure way to pass socket
  ei_setup_backend_socket(priv->ei, "/tmp/atspi-ei-kb.socket");
  priv->source_id = g_unix_fd_add(ei_get_fd(priv->ei), G_IO_IN, dispatch, device);
}

static void
atspi_device_libei_class_init (AtspiDeviceLibeiClass *klass)
{
  AtspiDeviceClass *device_class = ATSPI_DEVICE_CLASS (klass);
  GObjectClass *object_class = (GObjectClass *) klass;

  device_class->add_key_grab = atspi_device_libei_add_key_grab;
  device_class->remove_key_grab = atspi_device_libei_remove_key_grab;
  object_class->finalize = atspi_device_libei_finalize;
}

AtspiDeviceLibei *
atspi_device_libei_new ()
{
  AtspiDeviceLibei *device = g_object_new (atspi_device_libei_get_type (), NULL);

  return device;
}
