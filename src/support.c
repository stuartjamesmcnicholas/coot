/*
 * DO NOT EDIT THIS FILE - it is generated by Glade.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#ifndef _MSC_VER
#include <unistd.h>
#else
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif
#include <string.h>

#include <gtk/gtk.h>

#include "support.h"

GList *pixmaps_directories = NULL;

/* This is an internally used function to check if a pixmap file exists. */
static gchar* check_file_exists        (const gchar     *directory,
                                        const gchar     *filename);

/* This is an internally used function to create pixmaps. */
static GtkWidget* create_dummy_pixmap  (GtkWidget       *widget);

GtkWidget*
lookup_widget                          (GtkWidget       *widget,
                                        const gchar     *widget_name)
{
  GtkWidget *parent = 0, *found_widget = 0;

#if 0
  if (widget) { 
    for (;;)
      {
	if (GTK_IS_MENU (widget))
	  parent = gtk_menu_get_attach_widget (GTK_MENU (widget));
	else
	  parent = gtk_widget_get_parent(widget);
	if (parent == NULL)
	  break;
	widget = parent;
      }

    found_widget = (GtkWidget*) g_object_get_data (G_OBJECT (widget), widget_name);

    if (!found_widget)
      g_warning ("Widget not found: %s", widget_name);
  } else { 
    g_warning ("Widget is NULL: %s", widget_name);
  }
#endif
  return found_widget;
}

/* This is a dummy pixmap we use when a pixmap can't be found. */
static char *dummy_pixmap_xpm[] = {
/* columns rows colors chars-per-pixel */
"1 1 1 1",
"  c None",
/* pixels */
" "
};

/* This is an internally used function to create pixmaps. */
static GtkWidget*
create_dummy_pixmap                    (GtkWidget       *widget)
{

#if 0
  GdkColormap *colormap;
  GdkPixmap *gdkpixmap;
  GdkBitmap *mask;
  GtkWidget *pixmap;

  colormap = gtk_widget_get_colormap (widget);
  gdkpixmap = gdk_pixmap_colormap_create_from_xpm_d (NULL, colormap, &mask,
                                                     NULL, dummy_pixmap_xpm);
  if (gdkpixmap == NULL)
    g_error ("Couldn't create replacement pixmap.");
  pixmap = gtk_pixmap_new (gdkpixmap, mask);
  gdk_pixmap_unref (gdkpixmap);
  gdk_bitmap_unref (mask);
  return pixmap;
#endif
  return 0;
}

/* Use this function to set the directory containing installed pixmaps. */
void
add_pixmap_directory                   (const gchar     *directory)
{
  pixmaps_directories = g_list_prepend(pixmaps_directories, g_strdup (directory));
}


GtkWidget *
create_pixmap_gtk3_version(GtkWidget *widget, const gchar *filename) {

  GtkWidget *image = 0;
  GList *elem;
  gchar *found_filename = NULL;

  if (check_file_exists("./", filename)) {
    image = gtk_image_new_from_file (filename);
  } else {
     elem = pixmaps_directories;
     while (elem) {
       found_filename = check_file_exists ((gchar*)elem->data, filename);
       if (found_filename)
         break;
       elem = elem->next;
     }
     if (found_filename)
       image = gtk_image_new_from_file(found_filename);
  }
  g_free (found_filename);

  /* printf("################################################## create_pixmap_gtk3_version() %s returns image %p\n", */
  /*        filename, image); */

  return image;
}

/* This is an internally used function to create pixmaps. */
GtkWidget*
create_pixmap                          (GtkWidget       *widget,
                                        const gchar     *filename)
{
  return create_pixmap_gtk3_version(widget, filename);
}

/* This is an internally used function to check if a pixmap file exists. */
static gchar*
check_file_exists                      (const gchar     *directory,
                                        const gchar     *filename)
{
  gchar *full_filename;
  struct stat s;
  gint status;

  full_filename = (gchar*) g_malloc (strlen (directory) + 1
                                     + strlen (filename) + 1);
  strcpy (full_filename, directory);
  strcat (full_filename, G_DIR_SEPARATOR_S);
  strcat (full_filename, filename);

  status = stat (full_filename, &s);
  if (status == 0 && S_ISREG (s.st_mode))
    return full_filename;
  g_free (full_filename);
  return NULL;
}

