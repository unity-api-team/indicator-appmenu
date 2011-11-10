#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <sqlite3.h>
#include "usage-tracker.h"

struct _UsageTrackerPrivate {
	gchar * cachefile;
	sqlite3 * db;
};

#define USAGE_TRACKER_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), USAGE_TRACKER_TYPE, UsageTrackerPrivate))

static void usage_tracker_class_init (UsageTrackerClass *klass);
static void usage_tracker_init       (UsageTracker *self);
static void usage_tracker_dispose    (GObject *object);
static void usage_tracker_finalize   (GObject *object);
static void build_db                 (UsageTracker * self);

G_DEFINE_TYPE (UsageTracker, usage_tracker, G_TYPE_OBJECT);

static void
usage_tracker_class_init (UsageTrackerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (UsageTrackerPrivate));

	object_class->dispose = usage_tracker_dispose;
	object_class->finalize = usage_tracker_finalize;

	return;
}

static void
usage_tracker_init (UsageTracker *self)
{
	self->priv = USAGE_TRACKER_GET_PRIVATE(self);

	const gchar * basecachedir = g_getenv("HUD_CACHE_DIR");
	if (basecachedir == NULL) {
		basecachedir = g_get_user_cache_dir();
	}

	self->priv->cachefile = g_build_filename(basecachedir, "hud", "usage-log.sqlite", NULL);
	if (g_file_test(self->priv->cachefile, G_FILE_TEST_EXISTS)) {
		if (sqlite3_open(self->priv->cachefile, &self->priv->db) != SQLITE_OK) {
			self->priv->db = NULL;
		}
	}
	
	if (self->priv->db == NULL) {
		build_db(self);
	}

	return;
}

static void
usage_tracker_dispose (GObject *object)
{
	UsageTracker * self = USAGE_TRACKER(object);

	if (self->priv->db != NULL) {
		sqlite3_close(self->priv->db);
		self->priv->db = NULL;
	}

	G_OBJECT_CLASS (usage_tracker_parent_class)->dispose (object);
	return;
}

static void
usage_tracker_finalize (GObject *object)
{
	UsageTracker * self = USAGE_TRACKER(object);

	if (self->priv->cachefile != NULL) {
		g_free(self->priv->cachefile);
		self->priv->cachefile = NULL;
	}

	G_OBJECT_CLASS (usage_tracker_parent_class)->finalize (object);
	return;
}

UsageTracker *
usage_tracker_new (void)
{
	return g_object_new(USAGE_TRACKER_TYPE, NULL);
}

static void
build_db (UsageTracker * self)
{

	return;
}
