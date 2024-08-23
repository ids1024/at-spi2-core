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
#include "atspi-private.h"
#include "glib-unix.h"

#include <libei.h>
#include <poll.h>
#include <sys/mman.h>
#include <xkbcommon/xkbcommon.h>

#define ATSPI_VIRTUAL_MODIFIER_MASK 0x0000f000

typedef struct _AtspiDeviceLibeiPrivate AtspiDeviceLibeiPrivate;
struct _AtspiDeviceLibeiPrivate
{
  struct ei *ei;
  int source_id;
  struct xkb_context *xkb_context;
  struct xkb_keymap *xkb_keymap;
  struct xkb_state *xkb_state;
  GSList *modifiers;
};

G_DEFINE_TYPE_WITH_CODE (AtspiDeviceLibei, atspi_device_libei, ATSPI_TYPE_DEVICE, G_ADD_PRIVATE (AtspiDeviceLibei))

typedef struct
{
  guint keycode;
  guint modifier;
} AtspiLibeiKeyModifier;

static guint
find_virtual_mapping (AtspiDeviceLibei *libei_device, gint keycode)
{
  AtspiDeviceLibeiPrivate *priv = atspi_device_libei_get_instance_private (libei_device);
  GSList *l;

  for (l = priv->modifiers; l; l = l->next)
    {
      AtspiLibeiKeyModifier *entry = l->data;
      if (entry->keycode == keycode)
        return entry->modifier;
    }

  return 0;
}

static gboolean
check_virtual_modifier (AtspiDeviceLibei *libei_device, guint modifier)
{
  AtspiDeviceLibeiPrivate *priv = atspi_device_libei_get_instance_private (libei_device);
  GSList *l;

  if (modifier == (1 << ATSPI_MODIFIER_NUMLOCK))
    return TRUE;

  for (l = priv->modifiers; l; l = l->next)
    {
      AtspiLibeiKeyModifier *entry = l->data;
      if (entry->modifier == modifier)
        return TRUE;
    }

  return FALSE;
}

static guint
get_unused_virtual_modifier (AtspiDeviceLibei *libei_device)
{
  guint ret = 0x1000;

  while (ret < 0x10000)
    {
      if (!check_virtual_modifier (libei_device, ret))
        return ret;
      ret <<= 1;
    }

  return 0;
}

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

xkb_keysym_t keycode_to_keysym(struct xkb_keymap *xkb_keymap, gint keycode) {
  const xkb_keysym_t *syms;
  // XXX layout?
  xkb_keymap_key_get_syms_by_level(xkb_keymap, keycode, 0, 0, &syms);
  if (syms)
    return syms[0];
  else
    return XKB_KEY_NoSymbol;
}

// TODO debugging function
static void print_key_definition(AtspiDeviceLibei *libei_device, AtspiKeyDefinition *kd) {
  AtspiDeviceLibeiPrivate *priv = atspi_device_libei_get_instance_private (libei_device);
  char name[32];

  xkb_keysym_t keysym = keycode_to_keysym(priv->xkb_keymap, kd->keycode);
  xkb_keysym_get_name(keysym, name, 32);
  printf("KeyDefintion(%s, [", name);

  gboolean first = TRUE;
  for (GSList *l = priv->modifiers; l; l = l->next)
    {
      AtspiLibeiKeyModifier *entry = l->data;
      if (entry->modifier & kd->modifiers) {
	xkb_keysym_t keysym = keycode_to_keysym(priv->xkb_keymap, entry->keycode);
	  char name[32];
	  xkb_keysym_get_name(keysym, name, 32);
	if (!first)
	  printf(", ");
        printf("%s", name);
	first = FALSE;
      }
    }

  printf("])\n");


}

static gboolean
atspi_device_libei_add_key_grab (AtspiDevice *device, AtspiKeyDefinition *kd)
{
  AtspiDeviceLibei *libei_device = ATSPI_DEVICE_LIBEI (device);
  AtspiDeviceLibeiPrivate *priv = atspi_device_libei_get_instance_private (libei_device);

  printf("Grab ");
  print_key_definition(libei_device, kd);

  return TRUE;
}

static void
atspi_device_libei_remove_key_grab (AtspiDevice *device, guint id)
{
  AtspiDeviceLibei *libei_device = ATSPI_DEVICE_LIBEI (device);
  AtspiKeyDefinition *kd;
  kd = atspi_device_get_grab_by_id (device, id);
  printf("Ungrab ");
  print_key_definition(libei_device, kd);
}

static guint
atspi_device_libei_map_modifier (AtspiDevice *device, gint keycode)
{
  AtspiDeviceLibei *libei_device = ATSPI_DEVICE_LIBEI (device);
  AtspiDeviceLibeiPrivate *priv = atspi_device_libei_get_instance_private (libei_device);
  guint ret;
  AtspiLibeiKeyModifier *entry;

  const xkb_keysym_t keysym = keycode_to_keysym(priv->xkb_keymap, keycode);

  ret = find_virtual_mapping (libei_device, keycode);
  if (ret)
    return ret;

  ret = get_unused_virtual_modifier (libei_device);

  entry = g_new (AtspiLibeiKeyModifier, 1);
  entry->keycode = keycode;
  entry->modifier = ret;
  priv->modifiers = g_slist_append (priv->modifiers, entry);

  return ret;
}

static void
atspi_device_libei_unmap_modifier (AtspiDevice *device, gint keycode)
{
        printf("unmap_modifier\n");
}

static guint
atspi_device_libei_get_modifier (AtspiDevice *device, gint keycode)
{
        printf("get_modifier: %d\n", keycode);
        return 0;
}

static guint
atspi_device_libei_get_locked_modifiers (AtspiDevice *device)
{
        printf("get locked modifiers\n");
        return 0;
}

static void
atspi_device_libei_generate_mouse_event (AtspiDevice *device, AtspiAccessible *obj, gint x, gint y, const gchar *name, GError **error)
{
  AtspiPoint *p;

  p = atspi_component_get_position (ATSPI_COMPONENT (obj), ATSPI_COORD_TYPE_SCREEN, error);
  if (p->y == -1 && atspi_accessible_get_role (obj, NULL) == ATSPI_ROLE_APPLICATION)
    {
      g_clear_error (error);
      g_free (p);
      AtspiAccessible *child = atspi_accessible_get_child_at_index (obj, 0, NULL);
      if (child)
        {
          p = atspi_component_get_position (ATSPI_COMPONENT (child), ATSPI_COORD_TYPE_SCREEN, error);
          g_object_unref (child);
        }
    }

  if (p->y == -1 || p->x == -1)
    return;

  x += p->x;
  y += p->y;
  g_free (p);

  /* TODO: do this in process */
  atspi_generate_mouse_event (x, y, name, error);
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
  priv->ei = ei_new_receiver(NULL);
  // TODO secure way to pass socket
  ei_setup_backend_socket(priv->ei, "/tmp/atspi-ei-kb.socket");
  int fd = ei_get_fd(priv->ei);

  priv->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

  // Block until keymap received, so it can be used by vfuncs
  struct pollfd pollfd = {
    .fd = fd,
    .events = POLLIN,
  };
  while (!priv->xkb_keymap) {
    poll(&pollfd, 1, -1);
    dispatch(fd, G_IO_IN, device);
  }

  priv->source_id = g_unix_fd_add(fd, G_IO_IN, dispatch, device);
}

static void
atspi_device_libei_class_init (AtspiDeviceLibeiClass *klass)
{
  AtspiDeviceClass *device_class = ATSPI_DEVICE_CLASS (klass);
  GObjectClass *object_class = (GObjectClass *) klass;

  device_class->add_key_grab = atspi_device_libei_add_key_grab;
  device_class->remove_key_grab = atspi_device_libei_remove_key_grab;
  device_class->map_modifier = atspi_device_libei_map_modifier;
  device_class->unmap_modifier = atspi_device_libei_unmap_modifier;
  device_class->get_modifier = atspi_device_libei_get_modifier;
  device_class->get_locked_modifiers = atspi_device_libei_get_locked_modifiers;
  device_class->generate_mouse_event = atspi_device_libei_generate_mouse_event;
  object_class->finalize = atspi_device_libei_finalize;
}

AtspiDeviceLibei *
atspi_device_libei_new ()
{
  AtspiDeviceLibei *device = g_object_new (atspi_device_libei_get_type (), NULL);

  return device;
}
