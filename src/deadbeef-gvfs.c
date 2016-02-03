#include "config.h"

#include <deadbeef/deadbeef.h>
#include <gio/gio.h>

typedef struct {
    DB_FILE stream;

    GInputStream *handle;
    const char *content_type;
} vfs_gvfs_data_t;

static DB_functions_t *deadbeef = NULL;
static DB_vfs_t plugin;

static GVfs *gvfs = NULL;
static const char *scheme_names[] = { "smb://", "sftp://", "http://", NULL };

static DB_FILE*
ddb_gvfs_open (const char *path)
{
  const char *content_type;
  g_warning ("Call: %s", __FUNCTION__);
  g_return_val_if_fail (path != NULL, NULL);

  GError *error = NULL;
  GFile *file = g_file_new_for_uri (path);

  GFileInfo *info =
    g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                       G_FILE_QUERY_INFO_NONE, NULL, &error);

  if (info) {
      content_type = g_strdup (g_file_info_get_content_type (info));
      g_object_unref (info);
  } else {
      g_warning ("Error on request file %s information: %s", path, error->message);
      g_error_free (error);
      return NULL;
  }

  GFileInputStream *handle = g_file_read (file, NULL, &error);

  g_object_unref (file);
  if (handle == NULL)
    {
      g_warning ("Could not open %s for reading: %s", path, error->message);
      g_error_free (error);
      return NULL;
    }

  vfs_gvfs_data_t *data;
  data = g_new (vfs_gvfs_data_t, 1);
  data->stream.vfs = &plugin;
  data->content_type = content_type;
  data->handle = G_INPUT_STREAM (handle);

  g_warning ("return from: %s", __FUNCTION__);
  return (DB_FILE *) data;
}

static void
ddb_gvfs_close (DB_FILE *stream)
{
  g_warning ("Call: %s", __FUNCTION__);
  vfs_gvfs_data_t *data = (vfs_gvfs_data_t *) stream;
  g_return_if_fail (data != NULL);

  if (data->handle)
    g_object_unref (data->handle);

  g_free (data);
  g_warning ("return from: %s", __FUNCTION__);
}

static size_t
ddb_gvfs_read (void *ptr, size_t size, size_t nmemb, DB_FILE *stream)
{
/*   g_warning ("Call: %s", __FUNCTION__); */
  vfs_gvfs_data_t *data = (vfs_gvfs_data_t *) stream;
  g_return_val_if_fail (data != NULL, EOF);
  g_return_val_if_fail (! g_input_stream_is_closed (data->handle), -1);

  GError *error = NULL;
  gssize bytes = g_input_stream_read (data->handle, ptr, size * nmemb, NULL, &error);
  if (bytes < 0) {
      g_warning ("ddb_gvfs_read: error: %s", error->message);
      g_error_free (error);
      return 0;
  }
/*   g_warning ("return from: %s", __FUNCTION__); */
  return bytes / size;
}

static int
ddb_gvfs_seek (DB_FILE *stream, int64_t offset, int whence)
{
/*   g_warning ("Call: %s", __FUNCTION__); */
  vfs_gvfs_data_t *data = (vfs_gvfs_data_t *) stream;

  g_return_val_if_fail (data != NULL, -1);

  if (!g_seekable_can_seek (G_SEEKABLE (data->handle)))
    return -1;

  g_return_val_if_fail (! g_input_stream_is_closed (data->handle), -1);

  GSeekType seektype;
  switch (whence) {
    case SEEK_CUR:
      seektype = G_SEEK_CUR;
      break;

    case SEEK_END:
      seektype = G_SEEK_END;
      break;

    default:
      seektype = G_SEEK_SET;
      break;
  }

  GError *err = NULL;

/*   g_warning ("return from: %s", __FUNCTION__); */
  if (g_seekable_seek (G_SEEKABLE (data->handle), offset, seektype, NULL, &err))
    return 0;

  g_warning ("Could not seek: %s", err->message);
  g_error_free (err);
  return -1;
}

static int64_t
ddb_gvfs_tell (DB_FILE *stream)
{
/*   g_warning ("Call: %s", __FUNCTION__); */
  vfs_gvfs_data_t *data = (vfs_gvfs_data_t *) stream;
  g_return_val_if_fail (data != NULL, -1);

/*   g_warning ("return from: %s", __FUNCTION__); */
  return g_seekable_tell (G_SEEKABLE (data->handle));
}

static void
ddb_gvfs_rewind (DB_FILE *stream)
{
  g_warning ("Call: %s", __FUNCTION__);
  vfs_gvfs_data_t *data = (vfs_gvfs_data_t *) stream;
  g_return_if_fail (data != NULL);

  if (g_seekable_can_seek (G_SEEKABLE (data->handle)))
    g_seekable_seek (G_SEEKABLE (data->handle), 0, G_SEEK_SET, NULL, NULL);
}

static int64_t
ddb_gvfs_getlength (DB_FILE *stream)
{
  g_warning ("Call: %s", __FUNCTION__);
  vfs_gvfs_data_t *data = (vfs_gvfs_data_t *) stream;
  g_return_val_if_fail (data != NULL, -1);

  GError *error = NULL;
  GFileInfo *info;

  info = g_file_input_stream_query_info (G_FILE_INPUT_STREAM(data->handle),
                                         G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                         NULL, &error);
  if (info == NULL)
    {
      g_warning ("getlength(): error: %s", error->message);
      g_error_free (error);
      return -1;
    }

  gint64 size = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_STANDARD_SIZE);
  g_object_unref (info);

  g_warning ("return from: %s, ret=%d", __FUNCTION__, size);
  return size;
}

int
ddb_gvfs_scandir (const char *dir, struct dirent ***namelist,
                  int (*selector) (const struct dirent *), int (*cmp) (const struct dirent **, const struct dirent **))
{
  g_warning ("ddb_gvfs scandir(): not implemented yet");
  return -1;
}

const char **
ddb_gvfs_get_schemes (void)
{
  /* TODO: use g_vfs_get_supported_uri_schemes */
  return scheme_names;
}

int
ddb_gvfs_is_streaming (void)
{
  return 0;
}

int
ddb_gvfs_is_container (const char *path)
{
  return 0;
}

static int
ddb_gvfs_start (void)
{
  gvfs = g_vfs_get_default ();

  if (!g_vfs_is_active (gvfs))
    {
      g_warning ("GVfs not active - disabling gvfs plugin");
      return 1;
    }

  return 0;
}

static int
ddb_gvfs_stop (void)
{
  return 0;
}

static void
ddb_gvfs_set_track (DB_FILE *stream, DB_playItem_t *it) {
    g_warning ("Call: %s", __FUNCTION__);
}

static void
ddb_gvfs_abort (DB_FILE *stream) {
    g_warning ("Call: %s", __FUNCTION__);
}

static const char *
ddb_gvfs_get_content_type (DB_FILE *stream) {
    vfs_gvfs_data_t *data = (vfs_gvfs_data_t *) stream;
    g_return_val_if_fail (data != NULL, NULL);
    return data->content_type;
}

static DB_vfs_t plugin = {

    .plugin.api_vmajor = 1,
    .plugin.api_vminor = 0,

    .plugin.version_major = 0,
    .plugin.version_minor = 1,

    .plugin.type = DB_PLUGIN_VFS,

    .plugin.id = "ddb_gvfs",
    .plugin.name = "GVFS",
    .plugin.descr = "GVFS support",
    .plugin.copyright = "Public Domain",
    .plugin.website = "example.com",

    .plugin.start = ddb_gvfs_start,
    .plugin.stop = ddb_gvfs_stop,

    .set_track = ddb_gvfs_set_track,

    .open = ddb_gvfs_open,
    .close = ddb_gvfs_close,
    .abort = ddb_gvfs_abort,

    .read = ddb_gvfs_read,
    .seek = ddb_gvfs_seek,
    .tell = ddb_gvfs_tell,

    .rewind = ddb_gvfs_rewind,

    .getlength = ddb_gvfs_getlength,
    .get_schemes = ddb_gvfs_get_schemes,
    .get_content_type = ddb_gvfs_get_content_type,

    .is_streaming = ddb_gvfs_is_streaming,
    .is_container = ddb_gvfs_is_container,
    .scandir = ddb_gvfs_scandir
};

DB_plugin_t *
ddb_gvfs_load (DB_functions_t *api)
{
  deadbeef = api;
  return DB_PLUGIN (&plugin);
}
