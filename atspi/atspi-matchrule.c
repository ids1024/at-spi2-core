/*
 * AT-SPI - Assistive Technology Service Provider Interface
 * (Gnome Accessibility Project; http://developer.gnome.org/projects/gap)
 *
 * Copyright 2001, 2002 Sun Microsystems Inc.,
 * Copyright 2001, 2002 Ximian, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "atspi-private.h"

G_DEFINE_TYPE (AtspiMatchRule, atspi_match_rule, G_TYPE_OBJECT)

static void
atspi_match_rule_init (AtspiMatchRule *match_rule)
{
}

static void
atspi_match_rule_finalize (GObject *obj)
{
  AtspiMatchRule *rule = ATSPI_MATCH_RULE (obj);

  if (rule->states)
    g_object_unref (rule->states);
  if (rule->attributes)
    g_hash_table_unref (rule->attributes);
  /* TODO: Check that interfaces don't leak */
  if (rule->interfaces)
    g_array_free (rule->interfaces, TRUE);
}

static void
atspi_match_rule_class_init (AtspiMatchRuleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = atspi_match_rule_finalize;
}

/**
 * atspi_match_rule_new:
 *
 * @states: An #AtspiStateSet specifying the states to match or NULL if none.
 * @statematchtype: An #AtspiCollectionMatchType specifying how to interpret
 *                  @states.
 * @attributes: (element-type gchar* gchar*): A #GHashTable specifying
 *              attributes to match.
 * @attributematchtype: An #AtspiCollectionMatchType specifying how to
 *                      interpret @attributes.
 * @interfaces: (element-type gchar*): An array of interfaces to match, or
 *              NUL if not applicable.  Interface names should be specified
 *              by their DBus names (org.a11y.Atspi.Accessible,
 *              org.a11y.Atspi.Component, etc).
 * @interfacematchtype: An #AtspiCollectionMatchType specifying how to
 *                      interpret @interfaces.
 * @roles: (element-type AtspiRole): A #GArray of roles to match, or NULL if
 *         not applicable.
 * @rolematchtype: An #AtspiCollectionMatchType specifying how to
 *                      interpret @roles.
 * @invert: Specifies whether results should be inverted.
 * TODO: Document this parameter better.
 *
 * Returns: (transfer full): A new #AtspiMatchRule.
 */
AtspiMatchRule *
atspi_match_rule_new (AtspiStateSet *states,
                      AtspiCollectionMatchType statematchtype,
                      GHashTable *attributes,
                      AtspiCollectionMatchType attributematchtype,
                      GArray *roles,
                      AtspiCollectionMatchType rolematchtype,
                      GArray *interfaces,
                      AtspiCollectionMatchType interfacematchtype,
                      gboolean invert)
{
  AtspiMatchRule *rule = g_object_new (ATSPI_TYPE_MATCH_RULE, NULL);
  int i;

  if (!rule)
    return NULL;

  if (states)
    rule->states = g_object_ref (states);
  rule->statematchtype = statematchtype;

  if (attributes)
    rule->attributes = g_hash_table_ref (attributes);
    rule->attributematchtype = attributematchtype;

  if (interfaces)
    rule->interfaces = g_array_ref (interfaces);
  rule->interfacematchtype = interfacematchtype;

  if (roles)
  {
    for (i = 0; i < roles->len; i++)
    {
      AtspiRole role = g_array_index (roles, AtspiRole, i);
      if (role < 128)
        rule->roles [role / 32] |= (1 << (role % 32));
      else
        g_warning ("Atspi: unexpected role %d\n", role);
    }
  }
  else
    rule->roles [0] = rule->roles [1] = 0;
  rule->rolematchtype = rolematchtype;

  rule->invert = invert;

  return rule;
}

static void
append_entry (gpointer *key, gpointer *val, gpointer data)
{
  DBusMessageIter *iter = data;
  DBusMessageIter iter_entry;

  if (!dbus_message_iter_open_container (iter, DBUS_TYPE_DICT_ENTRY, NULL,
                                        &iter_entry))
    return;
  dbus_message_iter_append_basic (&iter_entry, DBUS_TYPE_STRING, &key);
  dbus_message_iter_append_basic (&iter_entry, DBUS_TYPE_STRING, &val);
  dbus_message_iter_close_container (iter, &iter_entry);
}

gboolean
_atspi_match_rule_marshal (AtspiMatchRule *rule, DBusMessageIter *iter)
{
  DBusMessageIter iter_struct, iter_array, iter_dict;
  dbus_int32_t states [2];
  dbus_int32_t d_statematchtype = rule->statematchtype;
  dbus_int32_t d_attributematchtype = rule->attributematchtype;
  dbus_int32_t d_interfacematchtype = rule->interfacematchtype;
  dbus_uint32_t d_rolematchtype = rule->rolematchtype;
  dbus_bool_t d_invert = rule->invert;
  gint i;
  dbus_int32_t d_role;

  if (!dbus_message_iter_open_container (iter, DBUS_TYPE_STRUCT, NULL,
                                         &iter_struct))
    return FALSE;

  /* states */
  if (rule->states)
  {
    states [0] = rule->states->states & 0xffffffff;
    states [1] = rule->states->states >> 32;
  }
  else
  {
    states [0] = states [1] = 0;
  }
  dbus_message_iter_open_container (&iter_struct, DBUS_TYPE_ARRAY, "i", &iter_array);
  dbus_message_iter_append_basic (&iter_array, DBUS_TYPE_INT32, &states [0]);
  dbus_message_iter_append_basic (&iter_array, DBUS_TYPE_INT32, &states [1]);
  dbus_message_iter_close_container (&iter_struct, &iter_array);
  dbus_message_iter_append_basic (&iter_struct, DBUS_TYPE_INT32, &d_statematchtype);

  /* attributes */
  if (!dbus_message_iter_open_container (&iter_struct, DBUS_TYPE_ARRAY, "{ss}",
                                         &iter_dict))
    return FALSE;
  g_hash_table_foreach (rule->attributes, append_entry, &iter_dict);
  dbus_message_iter_close_container (&iter_struct, &iter_dict);
  dbus_message_iter_append_basic (&iter_struct, DBUS_TYPE_INT32, &d_attributematchtype);

  if (!dbus_message_iter_open_container (&iter_struct, DBUS_TYPE_ARRAY, "i",
      &iter_array))
    return FALSE;
  d_role = rule->roles [0];
  dbus_message_iter_append_basic (&iter_array, DBUS_TYPE_INT32, &d_role);
  d_role = rule->roles [1];
  dbus_message_iter_append_basic (&iter_array, DBUS_TYPE_INT32, &d_role);
  d_role = rule->roles [2];
  dbus_message_iter_append_basic (&iter_array, DBUS_TYPE_INT32, &d_role);
  d_role = rule->roles [3];
  dbus_message_iter_append_basic (&iter_array, DBUS_TYPE_INT32, &d_role);
  dbus_message_iter_close_container (&iter_struct, &iter_array);
  dbus_message_iter_append_basic (&iter_struct, DBUS_TYPE_INT32,
                                  &d_rolematchtype);

  /* interfaces */
  if (!dbus_message_iter_open_container (&iter_struct, DBUS_TYPE_ARRAY, "s",
      &iter_array))
    return FALSE;
  if (rule->interfaces)
  {
    for (i = 0; i < rule->interfaces->len; i++)
    {
      char *val = g_array_index (rule->interfaces, gchar *, i);
      dbus_message_iter_append_basic (&iter_array, DBUS_TYPE_STRING, &val);
    }
  }
  dbus_message_iter_close_container (&iter_struct, &iter_array);
  dbus_message_iter_append_basic (&iter_struct, DBUS_TYPE_INT32, &d_interfacematchtype);

  dbus_message_iter_append_basic (&iter_struct, DBUS_TYPE_BOOLEAN, &d_invert);

  dbus_message_iter_close_container (iter, &iter_struct);
  return TRUE;
}
