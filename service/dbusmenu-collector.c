#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gio/gio.h>
#include <glib.h>

#include <libdbusmenu-glib/client.h>

#include "dbusmenu-collector.h"

#define GENERIC_ICON   "dbusmenu-lens-panel"

typedef struct _DbusmenuCollectorPrivate DbusmenuCollectorPrivate;

struct _DbusmenuCollectorPrivate {
	GDBusConnection * bus;
	guint signal;
	GHashTable * hash;
};

typedef struct _menu_key_t menu_key_t;
struct _menu_key_t {
	gchar * sender;
	gchar * path;
};

static void dbusmenu_collector_class_init (DbusmenuCollectorClass *klass);
static void dbusmenu_collector_init       (DbusmenuCollector *self);
static void dbusmenu_collector_dispose    (GObject *object);
static void dbusmenu_collector_finalize   (GObject *object);
static void update_layout_cb (GDBusConnection * connection, const gchar * sender, const gchar * path, const gchar * interface, const gchar * signal, GVariant * params, gpointer user_data);
static guint menu_hash_func (gconstpointer key);
static gboolean menu_equal_func (gconstpointer a, gconstpointer b);
static void menu_key_destroy (gpointer key);
static void place_in_results (DbusmenuMenuitem * menuitem, GArray * results);

G_DEFINE_TYPE (DbusmenuCollector, dbusmenu_collector, G_TYPE_OBJECT);

static void
dbusmenu_collector_class_init (DbusmenuCollectorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (DbusmenuCollectorPrivate));

	object_class->dispose = dbusmenu_collector_dispose;
	object_class->finalize = dbusmenu_collector_finalize;

	return;
}

static void
dbusmenu_collector_init (DbusmenuCollector *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE ((self), DBUSMENU_COLLECTOR_TYPE, DbusmenuCollectorPrivate);

	self->priv->bus = NULL;
	self->priv->signal = 0;
	self->priv->hash = NULL;

	self->priv->hash = g_hash_table_new_full(menu_hash_func, menu_equal_func,
	                                         menu_key_destroy, g_object_unref /* DbusmenuClient */);

	self->priv->bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	self->priv->signal = g_dbus_connection_signal_subscribe(self->priv->bus,
	                                                        NULL, /* sender */
	                                                        "com.canonical.dbusmenu", /* interface */
	                                                        "LayoutUpdated", /* member */
	                                                        NULL, /* object path */
	                                                        NULL, /* arg0 */
	                                                        G_DBUS_SIGNAL_FLAGS_NONE, /* flags */
	                                                        update_layout_cb, /* cb */
	                                                        self, /* data */
	                                                        NULL); /* free func */


	return;
}

static void
dbusmenu_collector_dispose (GObject *object)
{
	DbusmenuCollector * collector = DBUSMENU_COLLECTOR(object);

	if (collector->priv->signal != 0) {
		g_dbus_connection_signal_unsubscribe(collector->priv->bus, collector->priv->signal);
		collector->priv->signal = 0;
	}

	if (collector->priv->hash != NULL) {
		g_hash_table_destroy(collector->priv->hash);
		collector->priv->hash = NULL;
	}

	G_OBJECT_CLASS (dbusmenu_collector_parent_class)->dispose (object);
	return;
}

static void
dbusmenu_collector_finalize (GObject *object)
{

	G_OBJECT_CLASS (dbusmenu_collector_parent_class)->finalize (object);
	return;
}

DbusmenuCollector *
dbusmenu_collector_new (void)
{
	return DBUSMENU_COLLECTOR(g_object_new(DBUSMENU_COLLECTOR_TYPE, NULL));
}

static void
update_layout_cb (GDBusConnection * connection, const gchar * sender, const gchar * path, const gchar * interface, const gchar * signal, GVariant * params, gpointer user_data)
{
	g_debug("Client updating %s:%s", sender, path);
	DbusmenuCollector * collector = DBUSMENU_COLLECTOR(user_data);
	g_return_if_fail(collector != NULL);

	menu_key_t search_key = {
		sender: (gchar *)sender,
		path:   (gchar *)path
	};

	gpointer found = g_hash_table_lookup(collector->priv->hash, &search_key);
	if (found == NULL) {
		/* Build one becasue we don't have it */
		menu_key_t * built_key = g_new0(menu_key_t, 1);
		built_key->sender = g_strdup(sender);
		built_key->path = g_strdup(path);

		DbusmenuClient * client = dbusmenu_client_new(sender, path);

		g_hash_table_insert(collector->priv->hash, built_key, client);
	}

	/* Assume that dbusmenu client is doing this for us */
	return;
}

static guint
menu_hash_func (gconstpointer key)
{
	const menu_key_t * mk = (const menu_key_t*)key;

	return g_str_hash(mk->sender) + g_str_hash(mk->path) - 5381;
}

static gboolean
menu_equal_func (gconstpointer a, gconstpointer b)
{
	const menu_key_t * ak = (const menu_key_t *)a;
	const menu_key_t * bk = (const menu_key_t *)b;

	if (g_strcmp0(ak->sender, bk->sender) == 0) {
		return TRUE;
	}

	if (g_strcmp0(ak->path, bk->path) == 0) {
		return TRUE;
	}

	return FALSE;
}

static void
menu_key_destroy (gpointer key)
{
	menu_key_t * ikey = (menu_key_t *)key;

	g_free(ikey->sender);
	g_free(ikey->path);

	g_free(ikey);
	return;
}

typedef struct _hash_table_t hash_table_t;
struct _hash_table_t {
	gchar ** tokens;
	GArray * results;
};

static gboolean
label_contains_token (const gchar * label, const gchar * token)
{
	if (token == NULL) return -1;
	if (label == NULL) return -1;

	gchar * upper = g_utf8_strup(label, -1);

	gboolean contains = g_strrstr(upper, token) != NULL;
	g_free(upper);

	return contains;
}

gchar *
remove_underline (const gchar * input)
{
	const gchar * in = input;
	gchar * output = g_new0(gchar, g_utf8_strlen(input, -1) + 1);
	gchar * out = output;

	while (in[0] != '\0') {
		if (in[0] == '_') {
			in++;
		} else {
			out[0] = in[0];
			in++;
			out++;
		}
	}

	return output;
}

static void
tokens_to_children (DbusmenuMenuitem * rootitem, hash_table_t * hashdata, DbusmenuClient * client)
{
	if (hashdata->tokens[0] == NULL) {
		return;
	}

	GList * children = dbusmenu_menuitem_get_children(rootitem);
	GList * child;

	for (child = children; child != NULL; child = g_list_next(child)) {
		DbusmenuMenuitem * item = DBUSMENU_MENUITEM(child->data);

		if (!dbusmenu_menuitem_property_exist(item, DBUSMENU_MENUITEM_PROP_LABEL)) {
			continue;
		}

		const gchar * label = dbusmenu_menuitem_property_get(item, DBUSMENU_MENUITEM_PROP_LABEL);
		gchar * underline_label = remove_underline(label);

		if (hashdata->tokens[1] == NULL) {
			/* This is the last token, if it matches, let's show that one */
			if (label_contains_token(label, hashdata->tokens[0])) {
				place_in_results(item, hashdata->results);
			} else {
				tokens_to_children(item, hashdata, client);
			}
		} else {
			gchar * token = NULL;

			for (token = hashdata->tokens[0]; token != NULL; token++) {
				if (label_contains_token(label, token)) {
					break;
				}
			}

			if (token != NULL) {
				GArray * newitems = g_array_new(TRUE, TRUE, sizeof(gchar *));
				gchar * itoken = NULL;

				for (itoken = hashdata->tokens[0]; itoken != NULL; itoken++) {
					if (!label_contains_token(label, itoken)) {
						g_array_append_val(newitems, itoken);
					}
				}

				if (newitems->len == 0) {
					place_in_results(item, hashdata->results);
				} else {
					hash_table_t newdata = {
						tokens: (gchar **)newitems->data,
						results: hashdata->results
					};

					tokens_to_children(item, &newdata, client);
				}

				g_array_free(newitems, TRUE);
			}
		}

		g_free(underline_label);
	}

	return;
}

static void
hash_table_foreach (gpointer inkey, gpointer inval, gpointer user_data)
{
	DbusmenuClient * client = DBUSMENU_CLIENT(inval);
	hash_table_t * hashdata = (hash_table_t *)user_data;

	/* Handle the case where there are no search terms */
	if (hashdata->tokens[0] == NULL) {
		GList * children = dbusmenu_menuitem_get_children(dbusmenu_client_get_root(client));
		GList * child;

		for (child = children; child != NULL; child = g_list_next(child)) {
			DbusmenuMenuitem * item = DBUSMENU_MENUITEM(child->data);

			if (!dbusmenu_menuitem_property_exist(item, DBUSMENU_MENUITEM_PROP_LABEL)) {
				continue;
			}

			place_in_results(item, hashdata->results);
		}

		return;
	}

	tokens_to_children(dbusmenu_client_get_root(client), hashdata, client);
	return;
}

gchar **
normalize_tokens (gchar ** tokens)
{
	if (tokens[0] == NULL) {
		gchar ** out = (gchar **)g_new0(gchar*, 1);
		return out;
	}

	GArray * array = g_array_new(TRUE, TRUE, sizeof(gchar *));
	gint i;

	for (i = 0; tokens[i] != NULL; i++) {
		gchar * up = g_utf8_strup(tokens[i], -1);
		g_array_append_val(array, up);
	}

	gchar ** out = (gchar **)array->data;
	g_array_free(array, FALSE);

	return out;
}

GStrv
dbusmenu_collector_search (DbusmenuCollector * collector, const gchar * dbus_addr, const gchar * dbus_path, GStrv tokens)
{
	g_return_val_if_fail(IS_DBUSMENU_COLLECTOR(collector), NULL);
	g_return_val_if_fail(tokens != NULL, NULL);

	GArray * results = g_array_new(sizeof(gchar *), TRUE, TRUE);
	gchar ** newtokens = normalize_tokens(tokens);

	hash_table_t hashdata = {
		tokens: newtokens,
		results: results
	};

	g_hash_table_foreach(collector->priv->hash, hash_table_foreach, &hashdata);
	g_strfreev(newtokens);

	GStrv retval = (GStrv)results->data;
	g_array_free(results, FALSE);

	return retval;
}

static void
place_in_results (DbusmenuMenuitem * item, GArray * results)
{
	const gchar * label = dbusmenu_menuitem_property_get(item, DBUSMENU_MENUITEM_PROP_LABEL);
	gchar * underline_label = remove_underline(label);

	g_array_append_val(results, underline_label);

	return;
}

