/*
 * AT-SPI - Assistive Technology Service Provider Interface
 * (Gnome Accessibility Project; http://developer.gnome.org/projects/gap)
 *
 * Copyright 2007 IBM Corp.
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

/* TODO: Improve documentation and implement some missing functions */

/**
 * atspi_collection_is_ancester_of:
 *
 * @collection: The #AtspiCollection to test against.
 * @test: The #AtspiAccessible to test.
 *
 * Returns: TRUE if @collection is an ancestor of @test; FALSE otherwise.
 *
 * Not yet implemented.
 **/
gboolean
atspi_collection_is_ancestor_of (AtspiCollection *collection,
                                 AtspiAccessible *test,
                                 GError **error)
{
  g_warning ("Atspi: TODO: Implement is_ancester_of");
  return FALSE;
}

static DBusMessage *
new_message (AtspiCollection *collection, char *method)
{
  AtspiAccessible *accessible;

  if (!collection)
    return NULL;

  accessible = ATSPI_ACCESSIBLE (collection);
  return dbus_message_new_method_call (accessible->parent.app->bus_name,
                                       accessible->parent.path,
                                       atspi_interface_collection,
                                       method);
}

static gboolean
append_match_rule (DBusMessage *message, AtspiMatchRule *rule)
{
  DBusMessageIter iter;

  dbus_message_iter_init_append (message, &iter);
  return _atspi_match_rule_marshal (rule, &iter);
}

static gboolean
append_accessible (DBusMessage *message, AtspiAccessible *accessible)
{
  DBusMessageIter iter;

  dbus_message_iter_init_append (message, &iter);
  dbus_message_iter_append_basic (&iter, DBUS_TYPE_OBJECT_PATH,
                                  &accessible->parent.path);
}

static GArray *
return_accessibles (DBusMessage *message)
{
  DBusMessageIter iter, iter_array;
  GArray *ret = g_array_new (TRUE, TRUE, sizeof (AtspiAccessible *));

  _ATSPI_DBUS_CHECK_SIG (message, "a(so)", NULL);

  dbus_message_iter_init (message, &iter);
  dbus_message_iter_recurse (&iter, &iter_array);

  while (dbus_message_iter_get_arg_type (&iter_array) != DBUS_TYPE_INVALID)
  {
    AtspiAccessible *accessible;
    GArray *new_array;
    accessible = _atspi_dbus_return_accessible_from_iter (&iter_array);
    new_array = g_array_append_val (ret, accessible);
    if (new_array)
      ret = new_array;
    /* Iter was moved already, so no need to call dbus_message_iter_next */
  }
  dbus_message_unref (message);
  return ret;
}

/**
 * atspi_collection_get_matches:
 *
 * @collection: The #AtspiCollection.
 * @rule: A #AtspiMatchRule describing the match criteria.
 * @sortby: An #AtspiCollectionSortOrder specifying the way the results are to
 *          be sorted.
 * @count: The maximum number of results to return, or 0 for no limit.
 * @traverse: TODO
 *
 * Returns: (element-type AtspiAccessible*) (transfer full): A #GArray of
 *          #AtspiAccessibles matching the given match rule.
 **/
GArray *
atspi_collection_get_matches (AtspiCollection *collection,
                              AtspiMatchRule *rule,
                              AtspiCollectionSortOrder sortby,
                              gint count,
                              gboolean traverse,
                              GError **error)
{
  DBusMessage *message = new_message (collection, "GetMatches");
  DBusMessage *reply;
  dbus_int32_t d_sortby = sortby;
  dbus_int32_t d_count = count;
  dbus_bool_t d_traverse = traverse;

  if (!message)
    return NULL;

  if (!append_match_rule (message, rule))
    return NULL;
  dbus_message_append_args (message, DBUS_TYPE_UINT32, &d_sortby,
                            DBUS_TYPE_INT32, &d_count,
                            DBUS_TYPE_BOOLEAN, &d_traverse,
                            DBUS_TYPE_INVALID);
  reply = _atspi_dbus_send_with_reply_and_block (message);
  if (!reply)
    return NULL;
  return return_accessibles (reply);
}

/**
 * atspi_collection_get_matches_to:
 *
 * @collection: The #AtspiCollection.
 * @current_object: The object at which to start searching.
 * @rule: A #AtspiMatchRule describing the match criteria.
 * @sortby: An #AtspiCollectionSortOrder specifying the way the results are to
 *          be sorted.
 * @tree: An #AtspiCollectionTreeTraversalType specifying restrictions on
 *        the objects to be traversed.
 * @recurse: TODO
 * @count: The maximum number of results to return, or 0 for no limit.
 * @traverse: TODO
 *
 * Returns: (element-type AtspiAccessible*) (transfer full): A #GArray of
 *          #AtspiAccessibles matching the given match rule after
 *          @current_object.
 **/
GArray *
atspi_collection_get_matches_to (AtspiCollection *collection,
                              AtspiAccessible *current_object,
                              AtspiMatchRule *rule,
                              AtspiCollectionSortOrder sortby,
                              AtspiCollectionTreeTraversalType tree,
                              gboolean recurse,
                              gint count,
                              gboolean traverse,
                              GError **error)
{
  DBusMessage *message = new_message (collection, "GetMatchesTo");
  DBusMessage *reply;
  dbus_int32_t d_sortby = sortby;
  dbus_int32_t d_tree = tree;
  dbus_bool_t d_recurse = recurse;
  dbus_int32_t d_count = count;
  dbus_bool_t d_traverse = traverse;

  if (!message)
    return NULL;

  if (!append_accessible (message, current_object))
    return NULL;
  if (!append_match_rule (message, rule))
    return NULL;
  dbus_message_append_args (message, DBUS_TYPE_UINT32, &d_sortby,
                                     DBUS_TYPE_UINT32, &d_tree,
                            DBUS_TYPE_BOOLEAN, &d_recurse,
                            DBUS_TYPE_INT32, &d_count,
                            DBUS_TYPE_BOOLEAN, &d_traverse,
                            DBUS_TYPE_INVALID);
  reply = _atspi_dbus_send_with_reply_and_block (message);
  if (!reply)
    return NULL;
  return return_accessibles (reply);
}

/**
 * atspi_collection_get_matches_from:
 *
 * @collection: The #AtspiCollection.
 * @current_object: Upon reaching this object, searching should stop.
 * @rule: A #AtspiMatchRule describing the match criteria.
 * @sortby: An #AtspiCollectionSortOrder specifying the way the results are to
 *          be sorted.
 * @tree: An #AtspiCollectionTreeTraversalType specifying restrictions on
 *        the objects to be traversed.
 * @count: The maximum number of results to return, or 0 for no limit.
 * @traverse: TODO
 *
 * Returns: (element-type AtspiAccessible*) (transfer full): A #GArray of
 *          #AtspiAccessibles matching the given match rule that preceed
 *          @current_object.
 **/
GArray *
atspi_collection_get_matches_from (AtspiCollection *collection,
                              AtspiAccessible *current_object,
                              AtspiMatchRule *rule,
                              AtspiCollectionSortOrder sortby,
                              AtspiCollectionTreeTraversalType tree,
                              gint count,
                              gboolean traverse,
                              GError **error)
{
  DBusMessage *message = new_message (collection, "GetMatchesFrom");
  DBusMessage *reply;
  dbus_int32_t d_sortby = sortby;
  dbus_int32_t d_tree = tree;
  dbus_int32_t d_count = count;
  dbus_bool_t d_traverse = traverse;

  if (!message)
    return NULL;

  if (!append_accessible (message, current_object))
    return NULL;
  if (!append_match_rule (message, rule))
    return NULL;
  dbus_message_append_args (message, DBUS_TYPE_UINT32, &d_sortby,
                            DBUS_TYPE_UINT32, &d_tree,
                            DBUS_TYPE_INT32, &d_count,
                            DBUS_TYPE_BOOLEAN, &d_traverse,
                            DBUS_TYPE_INVALID);
  reply = _atspi_dbus_send_with_reply_and_block (message);
  if (!reply)
    return NULL;
  return return_accessibles (reply);
}

/**
 * atspi_collection_get_active_descendant:
 * 
 * @collection: The #AtspiCollection to query.
 *
 * Returns: (transfer full): The active descendant of #collection.
 *
 * Not yet implemented.
 **/
AtspiAccessible *
atspi_collection_get_active_descendant (AtspiCollection *collection, GError **error)
{
  g_warning ("atspi: TODO: Implement get_active_descendants");
  return NULL;
}

static void
atspi_collection_base_init (AtspiCollection *klass)
{
}

GType
atspi_collection_get_type (void)
{
  static GType type = 0;

  if (!type) {
    static const GTypeInfo tinfo =
    {
      sizeof (AtspiCollection),
      (GBaseInitFunc) atspi_collection_base_init,
      (GBaseFinalizeFunc) NULL,
    };

    type = g_type_register_static (G_TYPE_INTERFACE, "AtspiCollection", &tinfo, 0);

  }
  return type;
}
