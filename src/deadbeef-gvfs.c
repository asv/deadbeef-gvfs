#include "config.h"

#include <string.h>
#include <stdlib.h>

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
static const char *scheme_names[] = { "smb://", "sftp://", NULL };

static DB_FILE*
ddb_gvfs_open (const char *path)
{
  g_return_val_if_fail (path != NULL, NULL);

  GError *error = NULL;
  GFile *file = g_file_new_for_uri (path);

  GFileInfo *info =
    g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                       G_FILE_QUERY_INFO_NONE, NULL, &error);

  if (info == NULL)
    {
      g_warning ("Error on request file %s information: %s", path, error->message);
      g_error_free (error);
      return NULL;
    }

  GFileInputStream *handle = g_file_read (file, NULL, &error);

  g_object_unref (file);
  if (handle == NULL)
    {
      g_warning ("Could not open %s for reading: %s", path, error->message);

      g_object_unref (info);
      g_error_free (error);

      return NULL;
    }

  vfs_gvfs_data_t *data;

  data = g_new (vfs_gvfs_data_t, 1);
  data->stream.vfs = &plugin;
  data->handle = G_INPUT_STREAM (handle);
  data->content_type = g_strdup (g_file_info_get_content_type (info));

  g_object_unref (info);
  return (DB_FILE *) data;
}

static void
ddb_gvfs_close (DB_FILE *stream)
{
  vfs_gvfs_data_t *data = (vfs_gvfs_data_t *) stream;
  g_return_if_fail (data != NULL);

  if (data->handle)
    g_object_unref (data->handle);

  g_free (data);
}

static size_t
ddb_gvfs_read (void *ptr, size_t size, size_t nmemb, DB_FILE *stream)
{
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

  return bytes / size;
}

static int
ddb_gvfs_seek (DB_FILE *stream, int64_t offset, int whence)
{
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

  GError *error = NULL;

  if (g_seekable_seek (G_SEEKABLE (data->handle), offset, seektype, NULL, &error))
    return 0;

  g_warning ("Could not seek: %s", error->message);
  g_error_free (error);

  return -1;
}

static int64_t
ddb_gvfs_tell (DB_FILE *stream)
{
  vfs_gvfs_data_t *data = (vfs_gvfs_data_t *) stream;

  g_return_val_if_fail (data != NULL, -1);
  g_return_val_if_fail (! g_input_stream_is_closed (data->handle), -1);

  return g_seekable_tell (G_SEEKABLE (data->handle));
}

static void
ddb_gvfs_rewind (DB_FILE *stream)
{
  vfs_gvfs_data_t *data = (vfs_gvfs_data_t *) stream;
  g_return_if_fail (data != NULL);

  if (g_seekable_can_seek (G_SEEKABLE (data->handle)))
    g_seekable_seek (G_SEEKABLE (data->handle), 0, G_SEEK_SET, NULL, NULL);
}

static int64_t
ddb_gvfs_getlength (DB_FILE *stream)
{
  vfs_gvfs_data_t *data = (vfs_gvfs_data_t *) stream;

  g_return_val_if_fail (data != NULL, -1);
  g_return_val_if_fail (! g_input_stream_is_closed (data->handle), -1);

  GError *error = NULL;
  GFileInfo *info;

  info = g_file_input_stream_query_info (G_FILE_INPUT_STREAM(data->handle),
                                         G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                         NULL, &error);
  if (info == NULL)
    {
      g_warning ("Could not read stream info: %s", error->message);
      g_error_free (error);
      return -1;
    }

  gint64 size = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_STANDARD_SIZE);
  g_object_unref (info);

  return size;
}

int
ddb_gvfs_scandir (const char *dir, struct dirent ***namelist, int (*selector) (const struct dirent *), int (*cmp) (const struct dirent **, const struct dirent **))
{
  GQueue *file_list = g_queue_new ();
  GQueue *dir_list = g_queue_new ();
  g_queue_push_head (dir_list, g_file_new_for_uri (dir));

  GFile *gdir;
  while ((gdir = g_queue_pop_head (dir_list)) != NULL)
    {
      GFileEnumerator *file_enumerator = g_file_enumerate_children (gdir, G_FILE_ATTRIBUTE_STANDARD_NAME, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL);
      if (file_enumerator == NULL)
        {
          g_object_unref (gdir);
          continue;
        }

      GFileInfo *file_info;
      GFile *child_file;

     while ((file_info = g_file_enumerator_next_file (file_enumerator, NULL, NULL)) != NULL)
        {
          child_file = g_file_get_child (gdir, g_file_info_get_name (file_info));
          g_object_unref (file_info);

          if (g_file_query_file_type (child_file, G_FILE_QUERY_INFO_NONE, NULL) == G_FILE_TYPE_DIRECTORY)
            g_queue_push_head (dir_list, child_file);
          else
            {
              g_queue_push_tail (file_list, g_file_get_uri (child_file));
              g_object_unref (child_file);
            }
        }

      g_file_enumerator_close (file_enumerator, NULL, NULL);
      g_object_unref (file_enumerator);

      g_object_unref (gdir);
    }

  g_queue_free (dir_list);

  int num_files = 0;
  *namelist = malloc (sizeof(void *) * g_queue_get_length (file_list));

  char *fname;
  while ((fname = g_queue_pop_head (file_list)) != NULL)
    {
      struct dirent entry;
      strncpy (entry.d_name, fname, sizeof(entry.d_name) - 1);
      entry.d_name[sizeof(entry.d_name) - 1] = '\0';

      if (selector == NULL || (selector && selector(&entry)))
        {
          (*namelist)[num_files] = calloc (1, sizeof (struct dirent));
          strcpy ((*namelist)[num_files]->d_name, entry.d_name);
          num_files++;
        }

      g_free (fname);
    }

  g_queue_free (file_list);
  return num_files;
}

const char **
ddb_gvfs_get_schemes (void)
{
  return scheme_names;
}

int
ddb_gvfs_is_streaming (void)
{
  return 0;
}

int
ddb_gvfs_is_container (const char *fname)
{
  const char **sn = scheme_names;
  while (*sn)
    {
      if (g_str_has_prefix (fname, *sn))
        break;
      sn++;
    }

  if (*sn == NULL)
    return 0;

  GFile *file = g_file_new_for_uri (fname);
  GFileType type = g_file_query_file_type (file, G_FILE_QUERY_INFO_NONE, NULL);
  g_object_unref (file);

  return type == G_FILE_TYPE_DIRECTORY;
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
    .plugin.website = "https://github.com/asv/deadbeef-gvfs",

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
