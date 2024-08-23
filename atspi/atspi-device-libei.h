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

#ifndef _ATSPI_DEVICE_LIBEI_H_
#define _ATSPI_DEVICE_LIBEI_H_

#include "glib-object.h"

#include "atspi-device.h"
#include "atspi-types.h"

G_BEGIN_DECLS

#define ATSPI_TYPE_DEVICE_LIBEI (atspi_device_libei_get_type ())
#define ATSPI_DEVICE_LIBEI(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), ATSPI_TYPE_DEVICE_LIBEI, AtspiDeviceLibei))
#define ATSPI_DEVICE_LIBEI_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), ATSPI_TYPE_DEVICE_LIBEI, AtspiDeviceLibeiClass))
#define ATSPI_IS_DEVICE_LIBEI(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ATSPI_TYPE_DEVICE_LIBEI))
#define ATSPI_IS_DEVICE_LIBEI_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), ATSPI_TYPE_DEVICE_LIBEI))
#define ATSPI_DEVICE_LIBEI_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), ATSPI_TYPE_DEVICE_LIBEI, AtspiDeviceLibeiClass))

typedef struct _AtspiDeviceLibei AtspiDeviceLibei;
struct _AtspiDeviceLibei
{
  AtspiDevice parent;
};

typedef struct _AtspiDeviceLibeiClass AtspiDeviceLibeiClass;
struct _AtspiDeviceLibeiClass
{
  AtspiDeviceClass parent_class;
};

GType atspi_device_libei_get_type (void);

AtspiDeviceLibei *atspi_device_libei_new ();

G_END_DECLS

#endif /* _ATSPI_DEVICE_LIBEI_H_ */
