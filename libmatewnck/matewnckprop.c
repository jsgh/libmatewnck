/* vim: set sw=2 et: */
/*
 * Copyright (C) 2007 Vincent Untz 
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *	Vincent Untz <vuntz@gnome.org>
 *
 * Some of this code is based on code from mate-panel/panel-force-quit.c
 * This part of the code was written by Mark McLoughlin <mark@skynet.ie> and
 * copyrighted by Sun. Sun (through Glynn Foster <glynn.foster@sun.com>)
 * agreed to relicense it from the GPL to the LGPL.
 */

/* TODO:
 *  uncomment code that prints the workspace layout when API is public.
 *  uncomment code for matewnck_class_group_get_icon_is_fallback() when API is done
 *
 *  add --list-screen
 */
#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <glib/gi18n.h>

#include <libmatewnck/libmatewnck.h>
#include <libmatewnck/class-group.h>

enum {
  INVALID_MODE,
  SCREEN_READ_MODE,
  SCREEN_LIST_MODE,
  SCREEN_WRITE_MODE,
  WORKSPACE_READ_MODE,
  WORKSPACE_LIST_MODE,
  WORKSPACE_WRITE_MODE,
  APPLICATION_READ_MODE,
  APPLICATION_LIST_MODE,
  CLASS_GROUP_READ_MODE,
  CLASS_GROUP_LIST_MODE,
  WINDOW_READ_MODE,
  WINDOW_WRITE_MODE
} mode = INVALID_MODE;

gboolean option_xid = FALSE;
gboolean option_application = FALSE;
gboolean option_class_group = FALSE;
gboolean option_workspace = FALSE;
gboolean option_screen = FALSE;

gboolean get_from_user = TRUE;
MateWnckWindow *got_from_user = NULL;

gulong   xid = 0;
gulong   interact_app_xid = 0;
char     *interact_class_group = NULL;
int      interact_space = -1;
int      interact_screen = -1;

gboolean list = FALSE;
gboolean list_workspaces = FALSE;

int      set_n_workspaces = -1;
int      set_workspace_rows = 0;
int      set_workspace_cols = 0;
gboolean set_show_desktop = FALSE;
gboolean set_unshow_desktop = FALSE;
int      set_viewport_x = -1;
int      set_viewport_y = -1;

gboolean set_activate = FALSE;
char     *set_change_name = NULL;

gboolean set_minimize = FALSE;
gboolean set_unminimize = FALSE;
gboolean set_maximize = FALSE;
gboolean set_unmaximize = FALSE;
gboolean set_maximize_horizontally = FALSE;
gboolean set_unmaximize_horizontally = FALSE;
gboolean set_maximize_vertically = FALSE;
gboolean set_unmaximize_vertically = FALSE;
gboolean set_keyboard_move = FALSE;
gboolean set_keyboard_resize = FALSE;
gboolean set_close = FALSE;

gboolean set_fullscreen = FALSE;
gboolean set_unfullscreen = FALSE;
gboolean set_make_above = FALSE;
gboolean set_unmake_above = FALSE;
gboolean set_make_below = FALSE;
gboolean set_unmake_below = FALSE;
gboolean set_shade = FALSE;
gboolean set_unshade = FALSE;
gboolean set_stick = FALSE;
gboolean set_unstick = FALSE;
gboolean set_skip_pager = FALSE;
gboolean set_unskip_pager = FALSE;
gboolean set_skip_tasklist = FALSE;
gboolean set_unskip_tasklist = FALSE;
gboolean set_pin = FALSE;
gboolean set_unpin = FALSE;
int      set_workspace = -1;
int      set_x = G_MAXINT;
int      set_y = G_MAXINT;
int      set_width = -1;
int      set_height = -1;
char     *set_window_type = NULL;
MateWnckWindowType set_window_type_t = MATEWNCK_WINDOW_NORMAL;

static gboolean
option_parse (const char  *option_name,
              const char  *value,
              gpointer     data,
              GError     **error);

static GOptionEntry main_entries[] = {
	{ "window", 0, G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, option_parse,
          N_("X window ID of the window to examine or modify"), N_("XID") },
	{ "application", 0, G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, option_parse,
          /* Translators: A group leader is the window that is the "owner" of a
           * group of windows, ie: if you have multiple windows in one
           * application, one window has some information about the application
           * (like the application name). */
          N_("X window ID of the group leader of an application to examine"),
          N_("XID") },
	{ "class", 0, G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, option_parse,
          /* Translators: A class is like a "family". E.g., all gvim windows
           * are of the same class. */
          N_("Class resource of the class group to examine"), N_("CLASS") },
	{ "workspace", 0, G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, option_parse,
          N_("NUMBER of the workspace to examine or modify"), N_("NUMBER") },
	{ "screen", 0, G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, option_parse,
          N_("NUMBER of the screen to examine or modify"), N_("NUMBER") },
	{ "xid", 0, G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, option_parse,
          N_("Alias of --window"), N_("XID") },
	{ NULL }
};

static GOptionEntry list_entries[] = {
	{ "list", 0, 0, G_OPTION_ARG_NONE, &list,
          /* Translators: A class is like a "family". E.g., all gvim windows
           * are of the same class. */
          N_("List windows of the application/class group/workspace/screen (output format: \"XID: Window Name\")"), NULL },
	{ "list-workspaces", 0, 0, G_OPTION_ARG_NONE, &list_workspaces,
          N_("List workspaces of the screen (output format: \"Number: Workspace Name\")"), NULL },
	{ NULL }
};

static GOptionEntry screen_entries[] = {
	{ "set-n-workspaces", 0, 0, G_OPTION_ARG_INT, &set_n_workspaces,
          N_("Change the number of workspaces of the screen to NUMBER"), N_("NUMBER") },
	{ "set-workspace-rows", 0, 0, G_OPTION_ARG_INT, &set_workspace_rows,
          N_("Change the workspace layout of the screen to use NUMBER rows"), N_("NUMBER") },
	{ "set-workspace-columns", 0, 0, G_OPTION_ARG_INT, &set_workspace_cols,
          N_("Change the workspace layout of the screen to use NUMBER columns"), N_("NUMBER") },
	{ "show-desktop", 0, 0, G_OPTION_ARG_NONE, &set_show_desktop,
          N_("Show the desktop"), NULL },
	{ "unshow-desktop", 0, 0, G_OPTION_ARG_NONE, &set_unshow_desktop,
          N_("Stop showing the desktop"), NULL },
	{ "move-viewport-x", 0, 0, G_OPTION_ARG_INT, &set_viewport_x,
          /* Translators: 'viewport' is kind of the viewable area. A viewport
           * can be used to implement workspaces (e.g. compiz is an example);
           * however it is not just the current workspace. */
          N_("Move the viewport of the current workspace to X coordinate X"), N_("X") },
	{ "move-viewport-y", 0, 0, G_OPTION_ARG_INT, &set_viewport_y,
          /* Translators: 'viewport' is kind of the viewable area. A viewport
           * can be used to implement workspaces (e.g. compiz is an example);
           * however it is not just the current workspace. */
          N_("Move the viewport of the current workspace to Y coordinate Y"), N_("Y") },
	{ NULL }
};

static GOptionEntry window_entries[] = {
	{ "minimize", 0, 0, G_OPTION_ARG_NONE, &set_minimize,
          N_("Minimize the window"), NULL },
	{ "unminimize", 0, 0, G_OPTION_ARG_NONE, &set_unminimize,
          N_("Unminimize the window"), NULL },
	{ "maximize", 0, 0, G_OPTION_ARG_NONE, &set_maximize,
          N_("Maximize the window"), NULL },
	{ "unmaximize", 0, 0, G_OPTION_ARG_NONE, &set_unmaximize,
          N_("Unmaximize the window"), NULL },
	{ "maximize-horizontally", 0, 0, G_OPTION_ARG_NONE, &set_maximize_horizontally,
          N_("Maximize horizontally the window"), NULL },
	{ "unmaximize-horizontally", 0, 0, G_OPTION_ARG_NONE, &set_unmaximize_horizontally,
          N_("Unmaximize horizontally the window"), NULL },
	{ "maximize-vertically", 0, 0, G_OPTION_ARG_NONE, &set_maximize_vertically,
          N_("Maximize vertically the window"), NULL },
	{ "unmaximize-vertically", 0, 0, G_OPTION_ARG_NONE, &set_unmaximize_vertically,
          N_("Unmaximize vertically the window"), NULL },
	{ "keyboard-move", 0, 0, G_OPTION_ARG_NONE, &set_keyboard_move,
          N_("Start moving the window via the keyboard"), NULL },
	{ "keyboard-resize", 0, 0, G_OPTION_ARG_NONE, &set_keyboard_resize,
          N_("Start resizing the window via the keyboard"), NULL },
	{ "activate", 0, G_OPTION_FLAG_NOALIAS, G_OPTION_ARG_NONE, &set_activate,
          N_("Activate the window"), NULL },
	{ "close", 0, 0, G_OPTION_ARG_NONE, &set_close,
          N_("Close the window"), NULL },

	{ "make-fullscreen", 0, 0, G_OPTION_ARG_NONE, &set_fullscreen,
          N_("Make the window fullscreen"), NULL },
	{ "unmake-fullscreen", 0, 0, G_OPTION_ARG_NONE, &set_unfullscreen,
          N_("Make the window quit fullscreen mode"), NULL },
	{ "make-above", 0, 0, G_OPTION_ARG_NONE, &set_make_above,
          N_("Make the window always on top"), NULL },
	{ "unmake-above", 0, 0, G_OPTION_ARG_NONE, &set_unmake_above,
          N_("Make the window not always on top"), NULL },
	{ "make-below", 0, 0, G_OPTION_ARG_NONE, &set_make_below,
          N_("Make the window below other windows"), NULL },
	{ "unmake-below", 0, 0, G_OPTION_ARG_NONE, &set_unmake_below,
          N_("Make the window not below other windows"), NULL },
	{ "shade", 0, 0, G_OPTION_ARG_NONE, &set_shade,
          N_("Shade the window"), NULL },
	{ "unshade", 0, 0, G_OPTION_ARG_NONE, &set_unshade,
          N_("Unshade the window"), NULL },
	{ "stick", 0, 0, G_OPTION_ARG_NONE, &set_stick,
          /* Translators: 'viewport' is kind of the viewable area. A viewport
           * can be used to implement workspaces (e.g. compiz is an example);
           * however it is not just the current workspace. */
          N_("Make the window have a fixed position in the viewport"), NULL },
	{ "unstick", 0, 0, G_OPTION_ARG_NONE, &set_unstick,
          /* Translators: 'viewport' is kind of the viewable area. A viewport
           * can be used to implement workspaces (e.g. compiz is an example);
           * however it is not just the current workspace. */
          N_("Make the window not have a fixed position in the viewport"), NULL },
	{ "skip-pager", 0, 0, G_OPTION_ARG_NONE, &set_skip_pager,
          /* Translators: A pager is the technical term for the workspace
           * switcher. It's a representation of all workspaces with windows
           * inside it. Please make sure that the translation is in sync with
           * mate-panel, where this term is also used in translatable strings
           */
          N_("Make the window not appear in pagers"), NULL },
	{ "unskip-pager", 0, 0, G_OPTION_ARG_NONE, &set_unskip_pager,
          /* Translators: A pager is the technical term for the workspace
           * switcher. It's a representation of all workspaces with windows
           * inside it. Please make sure that the translation is in sync with
           * mate-panel, where this term is also used in translatable strings
           */
          N_("Make the window appear in pagers"), NULL },
	{ "skip-tasklist", 0, 0, G_OPTION_ARG_NONE, &set_skip_tasklist,
          /* Translators: "tasklist" is the list of running applications (the
           * window list) */
          N_("Make the window not appear in tasklists"), NULL },
	{ "unskip-tasklist", 0, 0, G_OPTION_ARG_NONE, &set_unskip_tasklist,
          /* Translators: "tasklist" is the list of running applications (the
           * window list) */
          N_("Make the window appear in tasklists"), NULL },
	{ "pin", 0, 0, G_OPTION_ARG_NONE, &set_pin,
          N_("Make the window visible on all workspaces"), NULL },
	{ "unpin", 0, 0, G_OPTION_ARG_NONE, &set_unpin,
          N_("Make the window visible on the current workspace only"), NULL },
	{ "set-workspace", 0, 0, G_OPTION_ARG_INT, &set_workspace,
          N_("Move the window to workspace NUMBER (first workspace is 0)"), N_("NUMBER") },
	{ "set-x", 0, 0, G_OPTION_ARG_INT, &set_x,
          N_("Change the X coordinate of the window to X"), N_("X") },
	{ "set-y", 0, 0, G_OPTION_ARG_INT, &set_y,
          N_("Change the Y coordinate of the window to Y"), N_("Y") },
	{ "set-width", 0, 0, G_OPTION_ARG_INT, &set_width,
          N_("Change the width of the window to WIDTH"), N_("WIDTH") },
	{ "set-height", 0, 0, G_OPTION_ARG_INT, &set_height,
          N_("Change the height of the window to HEIGHT"), N_("HEIGHT") },
	{ "set-window-type", 0, 0, G_OPTION_ARG_STRING, &set_window_type,
          /* Translators: do not translate "normal, desktop, dock..." */
          N_("Change the type of the window to TYPE (valid values: normal, desktop, dock, dialog, toolbar, menu, utility, splash)"), N_("TYPE") },
	{ NULL }
};

static GOptionEntry space_entries[] = {
	{ "change-name", 0, 0, G_OPTION_ARG_STRING, &set_change_name,
          N_("Change the name of the workspace to NAME"), N_("NAME") },
	{ "activate", 0, G_OPTION_FLAG_NOALIAS, G_OPTION_ARG_NONE, &set_activate,
          N_("Activate the workspace"), NULL },
	{ NULL }
};

static void clean_up (void);

/* this part is mostly stolen from xutils.c */
typedef struct 
{
  Window window;
  Atom timestamp_prop_atom;
} TimeStampInfo;

static Bool
timestamp_predicate (Display *display,
		     XEvent  *xevent,
		     XPointer arg)
{
  TimeStampInfo *info = (TimeStampInfo *)arg;

  if (xevent->type == PropertyNotify &&
      xevent->xproperty.window == info->window &&
      xevent->xproperty.atom == info->timestamp_prop_atom)
    return True;

  return False;
}

static guint32
get_xserver_timestamp (MateWnckScreen *screen)
{
  Display *display;
  int number;
  Screen *xscreen;
  TimeStampInfo info;
  unsigned char c = 'a';
  XEvent xevent;

  display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
  number = matewnck_screen_get_number (screen);
  xscreen = ScreenOfDisplay (display, number);

  info.window = XCreateSimpleWindow (display,
                                     RootWindowOfScreen (xscreen),
                                     0, 0, 10, 10, 0,
                                     WhitePixel (display, number),
                                     WhitePixel (display, number));
  info.timestamp_prop_atom = XInternAtom (display, "_TIMESTAMP_PROP",
                                          FALSE);

  XSelectInput (display, info.window, PropertyChangeMask);

  XChangeProperty (display, info.window,
		   info.timestamp_prop_atom, info.timestamp_prop_atom,
		   8, PropModeReplace, &c, 1);

  XIfEvent (display, &xevent,
	    timestamp_predicate, (XPointer)&info);

  XDestroyWindow (display, info.window);

  XSync (display, False);

  return xevent.xproperty.time;
}
/* end of stolen code */

static gboolean
option_parse (const char  *option_name,
              const char  *value,
              gpointer     data,
              GError     **error)
{
  char *end;

  /* skip "--" */
  option_name += 2;

  if (strcmp (option_name, "window") == 0 || strcmp (option_name, "xid") == 0)
    {
      gulong xid_buf;

      option_xid = TRUE;

      if (value)
        {
          get_from_user = FALSE;

          xid_buf = strtoul (value, &end, 10);
          if (end && end[0] == '\0')
            xid = xid_buf;
          else
            {
              g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                           _("Invalid value \"%s\" for --%s"),
                           value, option_name);
              return FALSE;
            }
        }

      return TRUE;
    }
  else if (strcmp (option_name, "application") == 0)
    {
      gulong xid_buf;

      option_application = TRUE;

      if (value)
        {
          get_from_user = FALSE;

          xid_buf = strtoul (value, &end, 10);
          if (end && end[0] == '\0')
            interact_app_xid = xid_buf;
          else
            {
              g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                           _("Invalid value \"%s\" for --%s"),
                           value, option_name);
              return FALSE;
            }
        }

      return TRUE;
    }
  else if (strcmp (option_name, "class") == 0)
    {
      option_class_group = TRUE;

      if (value)
        {
          get_from_user = FALSE;

          interact_class_group = g_strdup (value);
        }

      return TRUE;
    }
  else if (strcmp (option_name, "workspace") == 0)
    {
      int space_buf;

      option_workspace = TRUE;
      get_from_user = FALSE;

      if (value)
        {
          space_buf = strtol (value, &end, 10);
          if (end && end[0] == '\0')
            interact_space = space_buf;
          else
            {
              g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                           _("Invalid value \"%s\" for --%s"),
                           value, option_name);
              return FALSE;
            }
        }

      return TRUE;
    }
  else if (strcmp (option_name, "screen") == 0)
    {
      int screen_buf;

      option_screen = TRUE;
      get_from_user = FALSE;

      if (value)
        {
          screen_buf = strtol (value, &end, 10);
          if (end && end[0] == '\0')
            interact_screen = screen_buf;
          else
            {
              g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                           _("Invalid value \"%s\" for --%s"),
                           value, option_name);
              return FALSE;
            }
        }

      return TRUE;
    }
  else
    g_assert_not_reached ();

  return FALSE;
}

static gboolean
set_mode (int         new_mode,
          const char *option,
          gboolean    list)
{
  switch (mode)
    {
      case INVALID_MODE:
        if (list)
          g_assert_not_reached ();

        mode = new_mode;
        break;
      case SCREEN_READ_MODE:
        if (new_mode == SCREEN_READ_MODE || new_mode == SCREEN_WRITE_MODE)
          mode = new_mode;
        else if (list)
          mode = SCREEN_LIST_MODE;
        else
          {
            g_printerr (_("Conflicting options are present: screen %d should "
                          "be interacted with, but --%s has been used\n"),
                        interact_screen, option);
            return FALSE;
          }
        break;
      case SCREEN_LIST_MODE:
        if (new_mode != SCREEN_LIST_MODE)
          {
            g_printerr (_("Conflicting options are present: windows or "
                          "workspaces of screen %d should be listed, "
                          "but --%s has been used\n"),
                        interact_screen, option);
            return FALSE;
          }
        break;
      case SCREEN_WRITE_MODE:
        if (new_mode != SCREEN_WRITE_MODE)
          {
            g_printerr (_("Conflicting options are present: screen %d should "
                          "be interacted with, but --%s has been used\n"),
                        interact_screen, option);
            return FALSE;
          }
        break;
      case WORKSPACE_READ_MODE:
        if (new_mode == WORKSPACE_READ_MODE || new_mode == WORKSPACE_WRITE_MODE)
          mode = new_mode;
        else if (list)
          mode = WORKSPACE_LIST_MODE;
        else
          {
            g_printerr (_("Conflicting options are present: workspace %d "
                          "should be interacted with, but --%s has been "
                          "used\n"),
                        interact_space, option);
            return FALSE;
          }
        break;
      case WORKSPACE_LIST_MODE:
        if (new_mode != WORKSPACE_LIST_MODE)
          {
            g_printerr (_("Conflicting options are present: windows of "
                          "workspace %d should be listed, but --%s has been "
                          "used\n"),
                        interact_space, option);
            return FALSE;
          }
        break;
      case WORKSPACE_WRITE_MODE:
        if (new_mode != WORKSPACE_WRITE_MODE)
          {
            g_printerr (_("Conflicting options are present: workspace %d "
                          "should be interacted with, but --%s has been "
                          "used\n"),
                        interact_space, option);
            return FALSE;
          }
        break;
      case APPLICATION_READ_MODE:
        if (list)
          mode = APPLICATION_LIST_MODE;
        else if (new_mode != APPLICATION_READ_MODE)
          {
            g_printerr (_("Conflicting options are present: an application "
                          "should be interacted with, but --%s has been "
                          "used\n"),
                        option);
            return FALSE;
          }
        break;
      case APPLICATION_LIST_MODE:
        if (new_mode != APPLICATION_LIST_MODE)
          {
            g_printerr (_("Conflicting options are present: windows of an "
                          "application should be listed, but --%s has been "
                          "used\n"),
                        option);
            return FALSE;
          }
        break;
      case CLASS_GROUP_READ_MODE:
        if (list)
          mode = CLASS_GROUP_LIST_MODE;
        else if (new_mode != CLASS_GROUP_READ_MODE)
          {
            /* Translators: A class is like a "family". E.g., all gvim windows
             * are of the same class. */
            g_printerr (_("Conflicting options are present: class group "
                          "\"%s\" should be interacted with, but --%s has "
                          "been used\n"),
                        interact_class_group, option);
            return FALSE;
          }
        break;
      case CLASS_GROUP_LIST_MODE:
        if (new_mode != CLASS_GROUP_LIST_MODE)
          {
            /* Translators: A class is like a "family". E.g., all gvim windows
             * are of the same class. */
            g_printerr (_("Conflicting options are present: windows of class "
                          "group \"%s\" should be listed, but --%s has "
                          "been used\n"),
                        interact_class_group, option);
            return FALSE;
          }
        break;
      case WINDOW_READ_MODE:
        if (new_mode == WINDOW_READ_MODE || new_mode == WINDOW_WRITE_MODE)
          mode = new_mode;
        else
          {
            g_printerr (_("Conflicting options are present: a window should "
                          "be interacted with, but --%s has been used\n"),
                        option);
            return FALSE;
          }
        break;
      case WINDOW_WRITE_MODE:
        if (new_mode != WINDOW_WRITE_MODE)
          {
            g_printerr (_("Conflicting options are present: a window should "
                          "be interacted with, but --%s has been used\n"),
                        option);
            return FALSE;
          }
        break;
      default:
        g_assert_not_reached ();
    }

  return TRUE;
}

static gboolean
validate_options (void)
{
#define CHECK_DUAL_OPTIONS(shortvar, mode)                              \
  if (set_##shortvar && set_un##shortvar)                               \
    {                                                                   \
      g_printerr (_("Conflicting options are present: --%s and --%s\n"),\
                  #shortvar, "un"#shortvar);                            \
      return FALSE;                                                     \
    }                                                                   \
  if (set_##shortvar)                                                   \
    {                                                                   \
      if (!set_mode (mode, #shortvar, FALSE))                           \
        return FALSE;                                                   \
    }                                                                   \
  if (set_un##shortvar)                                                 \
    {                                                                   \
      if (!set_mode (mode, "un"#shortvar, FALSE))                       \
        return FALSE;                                                   \
    }

#define CHECK_BOOL(shortvar, name, mode)                                \
  if (set_##shortvar)                                                   \
    {                                                                   \
      if (!set_mode (mode, name, FALSE))                                \
        return FALSE;                                                   \
    }

#define CHECK_BOOL_REAL(var, name, mode)                                \
  if (var)                                                              \
    {                                                                   \
      if (!set_mode (mode, name, FALSE))                                \
        return FALSE;                                                   \
    }

#define CHECK_INT(var, name, mode)                                      \
  if (var != G_MAXINT)                                                  \
    {                                                                   \
      if (!set_mode (mode, name, FALSE))                                \
        return FALSE;                                                   \
    }

#define CHECK_POSITIVE_STRICT_INT(var, name, mode)                      \
  if (var != -1 && var <= 0)                                            \
    {                                                                   \
      g_printerr (_("Invalid argument \"%d\" for --%s: the argument must be strictly positive\n"), \
                  var, name);                                           \
      return FALSE;                                                     \
    }                                                                   \
  if (var != -1)                                                        \
    {                                                                   \
      if (!set_mode (mode, name, FALSE))                                \
        return FALSE;                                                   \
    }

#define CHECK_POSITIVE_INT(var, name, mode)                             \
  if (var != -1 && var < 0)                                             \
    {                                                                   \
      g_printerr (_("Invalid argument \"%d\" for --%s: the argument must be positive\n"), \
                  var, name);                                           \
      return FALSE;                                                     \
    }                                                                   \
  if (var != -1)                                                        \
    {                                                                   \
      if (!set_mode (mode, name, FALSE))                                \
        return FALSE;                                                   \
    }

  if (option_xid)
    mode = WINDOW_READ_MODE;
  if (option_application)
    if (!set_mode (APPLICATION_READ_MODE, "application", FALSE))
      return FALSE;
  if (option_class_group)
    if (!set_mode (CLASS_GROUP_READ_MODE, "class", FALSE))
      return FALSE;
  if (option_workspace)
    if (!set_mode (WORKSPACE_READ_MODE, "workspace", FALSE))
      return FALSE;
  if (option_screen)
    if (!set_mode (SCREEN_READ_MODE, "screen", FALSE))
      return FALSE;

  CHECK_BOOL_REAL (list_workspaces, "list-workspaces", SCREEN_LIST_MODE)

  if (list && list_workspaces)
    {
      g_printerr (_("Conflicting options are present: --%s and --%s\n"),
                  "list", "list-workspaces");
      return FALSE;
    }

  /* if there's just --list, then we list windows of the default screen */
  if (list && mode == INVALID_MODE)
    mode = SCREEN_LIST_MODE;
  else if (list)
    if (!set_mode (INVALID_MODE, "list", TRUE))
      return FALSE;

  CHECK_POSITIVE_STRICT_INT (set_n_workspaces, "set-n-workspaces",
                             SCREEN_WRITE_MODE)
  if (set_workspace_rows > 0)
    if (!set_mode (SCREEN_WRITE_MODE, "set-workspace-rows", FALSE))
      return FALSE;
  if (set_workspace_cols > 0)
    if (!set_mode (SCREEN_WRITE_MODE, "set-workspace-columns", FALSE))
      return FALSE;
  CHECK_DUAL_OPTIONS (show_desktop, SCREEN_WRITE_MODE)
  CHECK_POSITIVE_INT (set_viewport_x, "move-viewport-x", SCREEN_WRITE_MODE)
  CHECK_POSITIVE_INT (set_viewport_y, "move-viewport-y", SCREEN_WRITE_MODE)

  /* screen options can work by assuming it's on the default screen */
  if (mode == SCREEN_READ_MODE || mode == SCREEN_LIST_MODE ||
      mode == SCREEN_WRITE_MODE)
    {
      get_from_user = FALSE;
      option_screen = TRUE;
    }

  /* no command line option specifying the mode => the user will choose a
   * window */
  if (mode == INVALID_MODE)
    mode = WINDOW_READ_MODE;

  if (set_change_name != NULL)
    if (!set_mode (WORKSPACE_WRITE_MODE, "change-name", FALSE))
      return FALSE;

  CHECK_DUAL_OPTIONS (minimize, WINDOW_WRITE_MODE)
  CHECK_DUAL_OPTIONS (maximize, WINDOW_WRITE_MODE)
  CHECK_DUAL_OPTIONS (maximize_horizontally, WINDOW_WRITE_MODE)
  CHECK_DUAL_OPTIONS (maximize_vertically, WINDOW_WRITE_MODE)
  if (set_keyboard_move && set_keyboard_resize)
    {
      g_printerr (_("Conflicting options are present: --%s and --%s\n"),
                  "keyboard-move", "keyboard-resize");
      return FALSE;
    }
  CHECK_BOOL (keyboard_move, "keyboard-move", WINDOW_WRITE_MODE)
  CHECK_BOOL (keyboard_resize, "keyboard-resize", WINDOW_WRITE_MODE)
  CHECK_BOOL (close, "close", WINDOW_WRITE_MODE)

  CHECK_DUAL_OPTIONS (fullscreen, WINDOW_WRITE_MODE)
  CHECK_DUAL_OPTIONS (make_above, WINDOW_WRITE_MODE)
  CHECK_DUAL_OPTIONS (make_below, WINDOW_WRITE_MODE)
  CHECK_DUAL_OPTIONS (shade, WINDOW_WRITE_MODE)
  CHECK_DUAL_OPTIONS (stick, WINDOW_WRITE_MODE)
  CHECK_DUAL_OPTIONS (skip_pager, WINDOW_WRITE_MODE)
  CHECK_DUAL_OPTIONS (skip_tasklist, WINDOW_WRITE_MODE)
  CHECK_DUAL_OPTIONS (pin, WINDOW_WRITE_MODE)

  if ((set_pin || set_unpin) && set_workspace != -1)
    {
      g_printerr (_("Conflicting options are present: --%s or --%s, "
                    "and --%s\n"),
                  "pin", "unpin", "set-workspace");
      return FALSE;
    }

  CHECK_POSITIVE_INT (set_workspace, "set-workspace", WINDOW_WRITE_MODE)
  CHECK_INT (set_x, "set-x", WINDOW_WRITE_MODE)
  CHECK_INT (set_y, "set-y", WINDOW_WRITE_MODE)
  CHECK_POSITIVE_INT (set_width, "set-width", WINDOW_WRITE_MODE)
  CHECK_POSITIVE_INT (set_height, "set-height", WINDOW_WRITE_MODE)

  if (set_window_type != NULL)
    {
      if (strcmp (set_window_type, "normal") == 0)
        set_window_type_t = MATEWNCK_WINDOW_NORMAL;
      else if (strcmp (set_window_type, "desktop") == 0)
        set_window_type_t = MATEWNCK_WINDOW_DESKTOP;
      else if (strcmp (set_window_type, "dock") == 0)
        set_window_type_t = MATEWNCK_WINDOW_DOCK;
      else if (strcmp (set_window_type, "dialog") == 0)
        set_window_type_t = MATEWNCK_WINDOW_DIALOG;
      else if (strcmp (set_window_type, "toolbar") == 0)
        set_window_type_t = MATEWNCK_WINDOW_TOOLBAR;
      else if (strcmp (set_window_type, "menu") == 0)
        set_window_type_t = MATEWNCK_WINDOW_MENU;
      else if (strcmp (set_window_type, "utility") == 0)
        set_window_type_t = MATEWNCK_WINDOW_UTILITY;
      else if (strcmp (set_window_type, "splash") == 0)
        set_window_type_t = MATEWNCK_WINDOW_SPLASHSCREEN;
      else
        {
          g_printerr (_("Invalid argument \"%s\" for --%s, valid values are: "
                        "%s\n"),
                      set_window_type, "set-window-type",
                      "normal, desktop, dock, dialog, toolbar, menu, utility, "
                      "splash");
        }

      if (!set_mode (WINDOW_WRITE_MODE, "set-window-type", FALSE))
        return FALSE;
    }

  if (set_activate)
    {
      if (mode == WORKSPACE_READ_MODE || mode == WORKSPACE_WRITE_MODE)
        set_mode (WORKSPACE_WRITE_MODE, "activate", FALSE);
      else if (mode == WINDOW_READ_MODE || mode == WINDOW_WRITE_MODE)
        set_mode (WINDOW_WRITE_MODE, "activate", FALSE);
      else
        return set_mode (INVALID_MODE, "activate", FALSE);
    }

  return TRUE;
}

static void
update_screen (MateWnckScreen *screen)
{
  int viewport_x;
  int viewport_y;

  if (set_n_workspaces > 0)
    matewnck_screen_change_workspace_count (screen, set_n_workspaces);

  if (set_workspace_rows > 0 || set_workspace_cols > 0)
    {
      int token;

      token = matewnck_screen_try_set_workspace_layout (screen, 0,
                                                    set_workspace_rows,
                                                    set_workspace_cols);
      if (token)
        matewnck_screen_release_workspace_layout (screen, token);
      else
        g_printerr (_("Cannot change the workspace layout on the screen: "
                      "the layout is already owned\n"));
    }

  if (set_show_desktop)
    matewnck_screen_toggle_showing_desktop (screen, TRUE);
  else if (set_unshow_desktop)
    matewnck_screen_toggle_showing_desktop (screen, FALSE);

  if (set_viewport_x != -1 || set_viewport_y != -1)
    {
       MateWnckWorkspace *active_space;

       active_space = matewnck_screen_get_active_workspace (screen);

       if (active_space != NULL)
         {
           if (matewnck_workspace_is_virtual (active_space))
             {
               if (set_viewport_x != -1)
                 viewport_x = set_viewport_x;
               else
                 viewport_x = matewnck_workspace_get_viewport_x (active_space);

               if (set_viewport_y != -1)
                 viewport_y = set_viewport_y;
               else
                 viewport_y = matewnck_workspace_get_viewport_y (active_space);

               matewnck_screen_move_viewport (screen, viewport_x, viewport_y);
             }
           else
         /* Translators: 'viewport' is kind of the viewable area. A viewport
          * can be used to implement workspaces (e.g. compiz is an example);
          * however it is not just the current workspace. */
         g_printerr (_("Viewport cannot be moved: "
                       "the current workspace does not contain a viewport\n"));
         }
       else
         /* Translators: 'viewport' is kind of the viewable area. A viewport
          * can be used to implement workspaces (e.g. compiz is an example);
          * however it is not just the current workspace. */
         g_printerr (_("Viewport cannot be moved: "
                       "there is no current workspace\n"));
    }
}

static void
update_workspace (MateWnckWorkspace *space)
{
  unsigned int timestamp;

  timestamp = get_xserver_timestamp (matewnck_workspace_get_screen (space));

  if (set_activate)
    matewnck_workspace_activate (space, timestamp);

  if (set_change_name)
    matewnck_workspace_change_name (space, set_change_name);
}


static void
update_window (MateWnckWindow *window)
{
  MateWnckWindowActions        actions;
  MateWnckWindowMoveResizeMask geometry_mask;
  unsigned int             timestamp;

  actions = matewnck_window_get_actions (window);
  timestamp = get_xserver_timestamp (matewnck_window_get_screen (window));

#define SET_PROPERTY(name, action)                                      \
  if (set_##name)                                                       \
    {                                                                   \
      if (actions & action)                                             \
        matewnck_window_##name (window);                                    \
      else                                                              \
        g_printerr (_("Action not allowed\n"));                         \
    }

#define SET_PROPERTY_TIMESTAMP(name, action)                            \
  if (set_##name)                                                       \
    {                                                                   \
      if (actions & action)                                             \
        matewnck_window_##name (window, timestamp);                         \
      else                                                              \
        g_printerr (_("Action not allowed\n"));                         \
    }

#define SET_PROPERTY_DUAL(name, action1, action2)                       \
  if (set_##name)                                                       \
    {                                                                   \
      if (actions & action1)                                            \
        matewnck_window_##name (window);                                    \
      else                                                              \
        g_printerr (_("Action not allowed\n"));                         \
    }                                                                   \
  else if (set_un##name)                                                \
    {                                                                   \
      if (actions & action2)                                            \
        matewnck_window_un##name (window);                                  \
      else                                                              \
        g_printerr (_("Action not allowed\n"));                         \
    }

/* FIXME: why do we have dual & boolean API. This is not consistent! */
#define SET_PROPERTY_BOOLEAN(name, action1, action2)                    \
  if (set_##name)                                                       \
    {                                                                   \
      if (actions & action1)                                            \
        matewnck_window_set_##name (window, TRUE);                          \
      else                                                              \
        g_printerr (_("Action not allowed\n"));                         \
    }                                                                   \
  else if (set_un##name)                                                \
    {                                                                   \
      if (actions & action2)                                            \
        matewnck_window_set_##name (window, FALSE);                         \
      else                                                              \
        g_printerr (_("Action not allowed\n"));                         \
    }

#define SET_GEOMETRY(param, unset, action, change)                      \
  if (set_##param != unset)                                             \
    {                                                                   \
      if (actions & action)                                             \
        geometry_mask |= change;                                        \
      else                                                              \
        g_printerr (_("Action not allowed\n"));                         \
    }

  SET_PROPERTY (minimize, MATEWNCK_WINDOW_ACTION_MINIMIZE)
  SET_PROPERTY_TIMESTAMP (unminimize, MATEWNCK_WINDOW_ACTION_UNMINIMIZE)

  SET_PROPERTY_DUAL (maximize,
                     MATEWNCK_WINDOW_ACTION_MAXIMIZE,
                     MATEWNCK_WINDOW_ACTION_UNMAXIMIZE)
  SET_PROPERTY_DUAL (maximize_horizontally,
                     MATEWNCK_WINDOW_ACTION_MAXIMIZE_HORIZONTALLY,
                     MATEWNCK_WINDOW_ACTION_UNMAXIMIZE_HORIZONTALLY)
  SET_PROPERTY_DUAL (maximize_vertically,
                     MATEWNCK_WINDOW_ACTION_MAXIMIZE_VERTICALLY,
                     MATEWNCK_WINDOW_ACTION_UNMAXIMIZE_VERTICALLY)

  SET_PROPERTY_BOOLEAN (fullscreen,
                        MATEWNCK_WINDOW_ACTION_FULLSCREEN,
                        MATEWNCK_WINDOW_ACTION_FULLSCREEN)
  SET_PROPERTY_DUAL (make_above,
                     MATEWNCK_WINDOW_ACTION_ABOVE, MATEWNCK_WINDOW_ACTION_ABOVE)
  SET_PROPERTY_DUAL (make_below,
                     MATEWNCK_WINDOW_ACTION_BELOW, MATEWNCK_WINDOW_ACTION_BELOW)
  SET_PROPERTY_DUAL (shade,
                     MATEWNCK_WINDOW_ACTION_SHADE, MATEWNCK_WINDOW_ACTION_UNSHADE)
  SET_PROPERTY_DUAL (stick,
                     MATEWNCK_WINDOW_ACTION_STICK, MATEWNCK_WINDOW_ACTION_UNSTICK)
  SET_PROPERTY_BOOLEAN (skip_pager, actions, actions)
  SET_PROPERTY_BOOLEAN (skip_tasklist, actions, actions)
  SET_PROPERTY_DUAL (pin,
                     MATEWNCK_WINDOW_ACTION_CHANGE_WORKSPACE,
                     MATEWNCK_WINDOW_ACTION_CHANGE_WORKSPACE)

  if (set_workspace != -1)
    {
      if (actions & MATEWNCK_WINDOW_ACTION_CHANGE_WORKSPACE)
        {
           MateWnckScreen    *screen;
           MateWnckWorkspace *space;

           screen = matewnck_window_get_screen (window);
           space = matewnck_screen_get_workspace (screen, set_workspace);
           if (space)
             matewnck_window_move_to_workspace (window, space);
           else
             g_printerr (_("Window cannot be moved to workspace %d: "
                           "the workspace does not exist\n"), set_workspace);
        }
      else
        g_printerr (_("Action not allowed\n"));
    }

  /* do activation after the workspace change */
  if (set_activate)
    {
       MateWnckScreen    *screen;
       MateWnckWorkspace *space;

       screen = matewnck_window_get_screen (window);
       space = matewnck_window_get_workspace (window);
       if (space != NULL)
         {
           MateWnckWorkspace *active_space;

           active_space = matewnck_screen_get_active_workspace (screen);
           if (space != active_space)
             matewnck_workspace_activate (space, timestamp);
         }

      matewnck_window_activate_transient (window, timestamp);
    }

  geometry_mask = 0;
  SET_GEOMETRY (x, G_MAXINT, MATEWNCK_WINDOW_ACTION_MOVE, MATEWNCK_WINDOW_CHANGE_X)
  SET_GEOMETRY (y, G_MAXINT, MATEWNCK_WINDOW_ACTION_MOVE, MATEWNCK_WINDOW_CHANGE_Y)
  SET_GEOMETRY (width, -1, MATEWNCK_WINDOW_ACTION_RESIZE, MATEWNCK_WINDOW_CHANGE_WIDTH)
  SET_GEOMETRY (height, -1, MATEWNCK_WINDOW_ACTION_RESIZE, MATEWNCK_WINDOW_CHANGE_HEIGHT)

  if (geometry_mask != 0)
    matewnck_window_set_geometry (window,
                              MATEWNCK_WINDOW_GRAVITY_CURRENT, geometry_mask,
                              set_x, set_y, set_width, set_height);

  if (set_window_type != NULL)
    matewnck_window_set_window_type (window, set_window_type_t);

  /* interactive actions at the end */
  SET_PROPERTY (keyboard_move, MATEWNCK_WINDOW_ACTION_MOVE)
  /* FIXME: hack: we should rename the API */
  #define matewnck_window_keyboard_resize matewnck_window_keyboard_size
  SET_PROPERTY (keyboard_resize, MATEWNCK_WINDOW_ACTION_RESIZE)
  SET_PROPERTY_TIMESTAMP (close, MATEWNCK_WINDOW_ACTION_CLOSE)
}

static void
list_windows (GList *windows)
{
  MateWnckWindow *window;
  GList      *l;
  const char *buf;

  for (l = windows; l; l = l->next)
    {
      window = MATEWNCK_WINDOW (l->data);

      if (matewnck_window_has_name (window))
        buf = matewnck_window_get_name (window);
      else
        /* Translators: 'unset' in the sense of "something has not been set". */
        buf = _("<name unset>");

      /* Translators: %lu is a window number and %s a window name */
      g_print (_("%lu: %s\n"), matewnck_window_get_xid (window), buf);
    }
}

static void
list_screen (MateWnckScreen *screen)
{
  if (list_workspaces)
    {
      MateWnckWorkspace *space;
      GList         *spaces;
      GList         *l;

      spaces = matewnck_screen_get_workspaces (screen);

      for (l = spaces; l; l = l->next)
        {
          space = MATEWNCK_WORKSPACE (l->data);

          /* Translators: %d is a workspace number and %s a workspace name */
          g_print (_("%d: %s\n"),
                   matewnck_workspace_get_number (space),
                   matewnck_workspace_get_name (space));
        }
    }
  else
    list_windows (matewnck_screen_get_windows (screen));
}

static void
list_workspace (MateWnckWorkspace *space)
{
  MateWnckWindow *window;
  GList      *all_windows;
  GList      *l;
  GList      *space_windows;

  all_windows = matewnck_screen_get_windows (matewnck_workspace_get_screen (space));
  space_windows = NULL;

  for (l = all_windows; l; l = l->next)
    {
      window = MATEWNCK_WINDOW (l->data);

      if (matewnck_window_get_workspace (window) != NULL &&
          matewnck_window_get_workspace (window) != space)
        continue;

      space_windows = g_list_prepend (space_windows, window);
    }

  space_windows = g_list_reverse (space_windows);

  list_windows (space_windows);

  g_list_free (space_windows);
}

static void
list_class_group (MateWnckClassGroup *class_group)
{
  list_windows (matewnck_class_group_get_windows (class_group));
}

static void
list_application (MateWnckApplication *app)
{
  list_windows (matewnck_application_get_windows (app));
}

static void
print_screen (MateWnckScreen *screen)
{
  MateWnckWorkspace *space;
  MateWnckWindow    *window;
  const char    *buf;
  char          *free_buf;
#if 0
  MateWnckLayoutOrientation orientation;
  int            rows;
  int            columns;
#endif

  g_print (_("Screen Number: %d\n"), matewnck_screen_get_number (screen));

  g_print (_("Geometry (width, height): %d, %d\n"),
           matewnck_screen_get_width (screen),
           matewnck_screen_get_height (screen));

  g_print (_("Number of Workspaces: %d\n"),
           matewnck_screen_get_workspace_count (screen));

#if 0
  matewnck_screen_get_workspace_layout (screen, &orientation, &rows, &columns,
                                    NULL);
  g_print (_("Workspace Layout (rows, columns, orientation): "
             "%d, %d, %s\n"),
           rows, columns,
           orientation == MATEWNCK_LAYOUT_ORIENTATION_VERTICAL ? "vertical" :
                                                             "horizontal");
#endif

  if (matewnck_screen_get_window_manager_name (screen) != NULL)
    buf = matewnck_screen_get_window_manager_name (screen);
  else
    buf = _("<no EWMH-compliant window manager>");
  g_print (_("Window Manager: %s\n"), buf);

  space = matewnck_screen_get_active_workspace (screen);
  if (space)
    /* Translators: %d is a workspace number and %s a workspace name */
    free_buf = g_strdup_printf (_("%d (\"%s\")"),
                                matewnck_workspace_get_number (space),
                                matewnck_workspace_get_name (space));
  else
    /* Translators: "none" here means "no workspace" */
    free_buf = g_strdup (C_("workspace", "none"));
  g_print (_("Active Workspace: %s\n"), free_buf);
  g_free (free_buf);

  window = matewnck_screen_get_active_window (screen);
  if (window)
    {
      char *name;

      if (matewnck_window_has_name (window))
        name = g_strdup_printf (_("\"%s\""), matewnck_window_get_name (window));
      else
        /* Translators: 'unset' in the sense of "something has not been set". */
        name = g_strdup (_("<name unset>"));

      /* Translators: %lu is a window identifier (number) and %s a window name */
      free_buf = g_strdup_printf (_("%lu (%s)"),
                                  matewnck_window_get_xid (window), name);
      g_free (name);
    }
  else
    /* Translators: "none" here means "no window" */
    free_buf = g_strdup (C_("window", "none"));
  g_print (_("Active Window: %s\n"), free_buf);
  g_free (free_buf);

  g_print (_("Showing the desktop: %s\n"),
           matewnck_screen_get_showing_desktop (screen) ?
             _("true") : _("false"));
}

static void
print_workspace (MateWnckWorkspace *space)
{
  MateWnckScreen    *screen;
  MateWnckWorkspace *neighbor;
  const char    *buf;
  char          *free_buf;

  g_print (_("Workspace Name: %s\n"), matewnck_workspace_get_name (space));
  g_print (_("Workspace Number: %d\n"), matewnck_workspace_get_number (space));

  screen = matewnck_workspace_get_screen (space);
  if (matewnck_screen_get_window_manager_name (screen) != NULL)
    buf = matewnck_screen_get_window_manager_name (screen);
  else
    buf = _("<no EWMH-compliant window manager>");
  g_print (_("On Screen: %d (Window Manager: %s)\n"),
           matewnck_screen_get_number (screen), buf);

  g_print (_("Geometry (width, height): %d, %d\n"),
           matewnck_workspace_get_width (space),
           matewnck_workspace_get_height (space));

  if (matewnck_workspace_is_virtual (space))
    free_buf = g_strdup_printf ("%d, %d",
                                matewnck_workspace_get_viewport_x (space),
                                matewnck_workspace_get_viewport_y (space));
  else
    /* Translators: 'viewport' is kind of the viewable area. A viewport can be
     * used to implement workspaces (e.g. compiz is an example); however it is
     * not just the current workspace. */
    free_buf = g_strdup (_("<no viewport>"));
  /* Translators: 'viewport' is kind of the viewable area. A viewport can be
   * used to implement workspaces (e.g. compiz is an example); however it is
   * not just the current workspace. */
  g_print (_("Viewport position (x, y): %s\n"), free_buf);
  g_free (free_buf);

  g_print (_("Position in Layout (row, column): %d, %d\n"),
           matewnck_workspace_get_layout_row (space),
           matewnck_workspace_get_layout_column (space));

  neighbor = matewnck_workspace_get_neighbor (space, MATEWNCK_MOTION_LEFT);
  if (neighbor)
    /* Translators: %d is a workspace number and %s a workspace name */
    free_buf = g_strdup_printf (_("%d (\"%s\")"),
                                matewnck_workspace_get_number (neighbor),
                                matewnck_workspace_get_name (neighbor));
  else
    /* Translators: "none" here means "no workspace" */
    free_buf = g_strdup (C_("workspace", "none"));
  g_print (_("Left Neighbor: %s\n"), free_buf);
  g_free (free_buf);

  neighbor = matewnck_workspace_get_neighbor (space, MATEWNCK_MOTION_RIGHT);
  if (neighbor)
    /* Translators: %d is a workspace number and %s a workspace name */
    free_buf = g_strdup_printf (_("%d (\"%s\")"),
                                matewnck_workspace_get_number (neighbor),
                                matewnck_workspace_get_name (neighbor));
  else
    /* Translators: "none" here means "no workspace" */
    free_buf = g_strdup (C_("workspace", "none"));
  g_print (_("Right Neighbor: %s\n"), free_buf);
  g_free (free_buf);

  neighbor = matewnck_workspace_get_neighbor (space, MATEWNCK_MOTION_UP);
  if (neighbor)
    /* Translators: %d is a workspace number and %s a workspace name */
    free_buf = g_strdup_printf (_("%d (\"%s\")"),
                                matewnck_workspace_get_number (neighbor),
                                matewnck_workspace_get_name (neighbor));
  else
    /* Translators: "none" here means "no workspace" */
    free_buf = g_strdup (C_("workspace", "none"));
  g_print (_("Top Neighbor: %s\n"), free_buf);
  g_free (free_buf);

  neighbor = matewnck_workspace_get_neighbor (space, MATEWNCK_MOTION_DOWN);
  if (neighbor)
    /* Translators: %d is a workspace number and %s a workspace name */
    free_buf = g_strdup_printf (_("%d (\"%s\")"),
                                matewnck_workspace_get_number (neighbor),
                                matewnck_workspace_get_name (neighbor));
  else
    /* Translators: "none" here means "no workspace" */
    free_buf = g_strdup (C_("workspace", "none"));
  g_print (_("Bottom Neighbor: %s\n"), free_buf);
  g_free (free_buf);
}

static void
print_class_group (MateWnckClassGroup *class_group)
{
  GList *windows;

  windows = matewnck_class_group_get_windows (class_group);

  /* Translators: Resource class is the name to identify a class. */
  g_print (_("Resource Class: %s\n"),
           matewnck_class_group_get_res_class (class_group));
  g_print (_("Group Name: %s\n"), matewnck_class_group_get_name (class_group));

  /* TODO: missing API */
#if 0
  if (!matewnck_class_group_get_icon_is_fallback (class_group))
    /* Translators: 'set' in the sense of "something has been set". */
    buf = _("set");
  else
    /* Translators: 'unset' in the sense of "something has not been set". */
    buf = _("<unset>");
  g_print (_("Icons: %s\n"), buf);
#endif

  g_print (_("Number of Windows: %d\n"), g_list_length (windows));
}

static void
print_application (MateWnckApplication *app)
{
  const char *buf;
  char       *free_buf;
  GList      *windows;

  windows = matewnck_application_get_windows (app);

  g_print (_("Name: %s\n"), matewnck_application_get_name (app));
  g_print (_("Icon Name: %s\n"), matewnck_application_get_icon_name (app));

  if (!matewnck_application_get_icon_is_fallback (app))
    /* Translators: 'set' in the sense of "something has been set". */
    buf = _("set");
  else
    /* Translators: 'unset' in the sense of "something has not been set". */
    buf = _("<unset>");
  g_print (_("Icons: %s\n"), buf);

  if (matewnck_application_get_pid (app) != 0)
    free_buf = g_strdup_printf ("%d", matewnck_application_get_pid (app));
  else
    /* Translators: 'unset' in the sense of "something has not been set". */
    free_buf = g_strdup (_("<unset>"));
  g_print (_("PID: %s\n"), free_buf);
  g_free (free_buf);

  if (matewnck_application_get_startup_id (app) != NULL)
    buf = matewnck_application_get_startup_id (app);
  else
    /* Translators: "none" here means "no startup ID" */
    buf = C_("startupID", "none");
  g_print (_("Startup ID: %s\n"), buf);

  g_print (_("Number of Windows: %d\n"), g_list_length (windows));
}

static void
print_window (MateWnckWindow *window)
{
  MateWnckWindowType     type;
  int                x, y, w, h;
  MateWnckClassGroup    *class_group;
  MateWnckWorkspace     *space;
  MateWnckScreen        *screen;
  MateWnckWindowActions  actions;
  const char        *buf;
  char              *free_buf;

  if (matewnck_window_has_name (window))
    buf = matewnck_window_get_name (window);
  else
    /* Translators: 'unset' in the sense of "something has not been set". */
    buf = _("<unset>");
  g_print (_("Name: %s\n"), buf);

  if (matewnck_window_has_icon_name (window))
    buf = matewnck_window_get_icon_name (window);
  else
    /* Translators: 'unset' in the sense of "something has not been set". */
    buf = _("<unset>");
  /* Translators: note that "Icon" here has a specific window
   * management-related meaning. It means minimized. */
  g_print (_("Icon Name: %s\n"), buf);

  if (!matewnck_window_get_icon_is_fallback (window))
    /* Translators: 'set' in the sense of "something has been set". */
    buf = _("set");
  else
    /* Translators: 'unset' in the sense of "something has not been set". */
    buf = _("<unset>");
  g_print (_("Icons: %s\n"), buf);

  space = matewnck_window_get_workspace (window);
  if (space)
    /* Translators: %d is a workspace number and %s a workspace name */
    free_buf = g_strdup_printf (_("%d (\"%s\")"),
                                matewnck_workspace_get_number (space),
                                matewnck_workspace_get_name (space));
  else if (matewnck_window_is_pinned (window))
    free_buf = g_strdup (_("all workspaces"));
  else
    /* Translators: "none" here means "no workspace" */
    free_buf = g_strdup (C_("workspace", "none"));
  g_print (_("On Workspace: %s\n"), free_buf);
  g_free (free_buf);

  screen = matewnck_window_get_screen (window);
  if (matewnck_screen_get_window_manager_name (screen) != NULL)
    buf = matewnck_screen_get_window_manager_name (screen);
  else
    buf = _("<no EWMH-compliant window manager>");
  g_print (_("On Screen: %d (Window Manager: %s)\n"),
           matewnck_screen_get_number (screen), buf);

  type = matewnck_window_get_window_type (window);
  switch (type)
    {
      case MATEWNCK_WINDOW_NORMAL:
        buf = _("normal window");
        break;
      case MATEWNCK_WINDOW_DESKTOP:
        buf = _("desktop");
        break;
      case MATEWNCK_WINDOW_DOCK:
        buf = _("dock or panel");
        break;
      case MATEWNCK_WINDOW_DIALOG:
        buf = _("dialog window");
        break;
      case MATEWNCK_WINDOW_TOOLBAR:
        buf = _("tearoff toolbar");
        break;
      case MATEWNCK_WINDOW_MENU:
        buf = _("tearoff menu");
        break;
      case MATEWNCK_WINDOW_UTILITY:
        buf = _("utility window");
        break;
      case MATEWNCK_WINDOW_SPLASHSCREEN:
        buf = _("splash screen");
        break;
      default:
        g_assert_not_reached ();
    }
  g_print (_("Window Type: %s\n"), buf);

  matewnck_window_get_geometry (window, &x, &y, &w, &h);
  g_print (_("Geometry (x, y, width, height): %d, %d, %d, %d\n"), x, y, w, h);

  class_group = matewnck_window_get_class_group (window);
  if (strcmp (matewnck_class_group_get_res_class (class_group), ""))
    buf = matewnck_class_group_get_res_class (class_group);
  else
    /* Translators: 'unset' in the sense of "something has not been set". */
    buf = _("<unset>");
  /* Translators: A class is like a "family". E.g., all gvim windows are of the
   * same class. */
  g_print (_("Class Group: %s\n"), buf);

  g_print (_("XID: %lu\n"), matewnck_window_get_xid (window));

  if (matewnck_window_get_pid (window) != 0)
    free_buf = g_strdup_printf ("%d", matewnck_window_get_pid (window));
  else
    /* Translators: 'unset' in the sense of "something has not been set". */
    free_buf = g_strdup (_("<unset>"));
  g_print (_("PID: %s\n"), free_buf);
  g_free (free_buf);

  if (matewnck_window_get_session_id (window))
    buf = matewnck_window_get_session_id (window);
  else
    /* Translators: 'unset' in the sense of "something has not been set". */
    buf = _("<unset>");
  g_print (_("Session ID: %s\n"), buf);

  if (matewnck_window_get_group_leader (window) != matewnck_window_get_xid (window))
    /* Translators: A group leader is the window that is the "owner" of a group
     * of windows, ie: if you have multiple windows in one application, one
     * window has some information about the application (like the application
     * name). */
    g_print (_("Group Leader: %lu\n"), matewnck_window_get_group_leader (window));
  //FIXME: else print something?

  if (matewnck_window_get_transient (window))
    /* Translators: A window can be transient for another window: it means it's
     * on top of it */
    g_print (_("Transient for: %lu\n"),
             matewnck_window_get_xid (matewnck_window_get_transient (window)));
  //FIXME: else print something?

#define PRINT_LIST_ITEM(func, string)                           \
  if (func (window))                                            \
    {                                                           \
      char *tmp;                                                \
                                                                \
      /* Translators: we're building a list of items here.      \
       * For example, the result is "a, b".                     \
       * In this case, the first string is "a", the second      \
       * string is ", " and the third string is "b".            \
       * We can then use this information here to also          \
       * recursively build longer lists, like "a, b, c, d" */   \
      tmp = g_strdup_printf (_("%1$s%2$s%3$s"),                 \
                             free_buf ? free_buf : "",          \
      /* Translators: see comment for "%1$s%2$s%3$s" in order   \
       * to properly translate this */                          \
                             free_buf ? _(", ") : "",           \
                             string);                           \
      g_free (free_buf);                                        \
      free_buf = tmp;                                           \
    }
  free_buf = NULL;
  PRINT_LIST_ITEM (matewnck_window_is_minimized, _("minimized"));
  PRINT_LIST_ITEM (matewnck_window_is_maximized, _("maximized"));
  if (!matewnck_window_is_maximized (window))
    {
      PRINT_LIST_ITEM (matewnck_window_is_maximized_horizontally,
                       _("maximized horizontally"));
      PRINT_LIST_ITEM (matewnck_window_is_maximized_vertically,
                       _("maximized vertically"));
    }
  PRINT_LIST_ITEM (matewnck_window_is_shaded, _("shaded"));
  PRINT_LIST_ITEM (matewnck_window_is_pinned, _("pinned"));
  PRINT_LIST_ITEM (matewnck_window_is_sticky, _("sticky"));
  PRINT_LIST_ITEM (matewnck_window_is_above, _("above"));
  PRINT_LIST_ITEM (matewnck_window_is_below, _("below"));
  PRINT_LIST_ITEM (matewnck_window_is_fullscreen, _("fullscreen"));
  PRINT_LIST_ITEM (matewnck_window_needs_attention, _("needs attention"));
  /* Translators: A pager is the technical term for the workspace switcher.
   * It's a representation of all workspaces with windows inside it.
   * Please make sure that the translation is in sync with mate-panel,
   * where this term is also used in translatable strings */
  PRINT_LIST_ITEM (matewnck_window_is_skip_pager, _("skip pager"));
  /* Translators: "tasklist" is the list of running applications (the window
   * list) */
  PRINT_LIST_ITEM (matewnck_window_is_skip_tasklist, _("skip tasklist"));
  if (!free_buf)
    free_buf = g_strdup (_("normal"));
  g_print (_("State: %s\n"), free_buf);
  g_free (free_buf);

#define PRINT_FLAGS_ITEM(bitmask, flag, string)                 \
  if (bitmask & flag)                                           \
    {                                                           \
      char *tmp;                                                \
                                                                \
      /* Translators: we're building a list of items here.      \
       * The end result is something like "a, b, c"             \
       * In this case, the first string is "a, b", the second   \
       * string is ", " and the third string is "c" */          \
      tmp = g_strdup_printf (_("%1$s%2$s%3$s"),                 \
                             free_buf ? free_buf : "",          \
                             free_buf ? _(", ") : "",           \
                             string);                           \
      g_free (free_buf);                                        \
      free_buf = tmp;                                           \
    }
  free_buf = NULL;
  actions = matewnck_window_get_actions (window);
  PRINT_FLAGS_ITEM (actions, MATEWNCK_WINDOW_ACTION_MOVE, _("move"));
  PRINT_FLAGS_ITEM (actions, MATEWNCK_WINDOW_ACTION_RESIZE, _("resize"));
  PRINT_FLAGS_ITEM (actions, MATEWNCK_WINDOW_ACTION_SHADE, _("shade"));
  PRINT_FLAGS_ITEM (actions, MATEWNCK_WINDOW_ACTION_UNSHADE, _("unshade"));
  PRINT_FLAGS_ITEM (actions, MATEWNCK_WINDOW_ACTION_STICK, _("stick"));
  PRINT_FLAGS_ITEM (actions, MATEWNCK_WINDOW_ACTION_UNSTICK, _("unstick"));
  PRINT_FLAGS_ITEM (actions, MATEWNCK_WINDOW_ACTION_MAXIMIZE_HORIZONTALLY,
                    _("maximize horizontally"));
  PRINT_FLAGS_ITEM (actions, MATEWNCK_WINDOW_ACTION_UNMAXIMIZE_HORIZONTALLY,
                    _("unmaximize horizontally"));
  PRINT_FLAGS_ITEM (actions, MATEWNCK_WINDOW_ACTION_MAXIMIZE_VERTICALLY,
                    _("maximize vertically"));
  PRINT_FLAGS_ITEM (actions, MATEWNCK_WINDOW_ACTION_UNMAXIMIZE_VERTICALLY,
                    _("unmaximize vertically"));
  /* we're calling PRINT_FLAGS_ITEM() three times for l10n reasons */
  PRINT_FLAGS_ITEM (actions, MATEWNCK_WINDOW_ACTION_CHANGE_WORKSPACE,
                    _("change workspace"));
  PRINT_FLAGS_ITEM (actions, MATEWNCK_WINDOW_ACTION_CHANGE_WORKSPACE,
                    _("pin"));
  PRINT_FLAGS_ITEM (actions, MATEWNCK_WINDOW_ACTION_CHANGE_WORKSPACE,
                    _("unpin"));
  PRINT_FLAGS_ITEM (actions, MATEWNCK_WINDOW_ACTION_MINIMIZE, _("minimize"));
  PRINT_FLAGS_ITEM (actions, MATEWNCK_WINDOW_ACTION_UNMINIMIZE, _("unminimize"));
  PRINT_FLAGS_ITEM (actions, MATEWNCK_WINDOW_ACTION_MAXIMIZE, _("maximize"));
  PRINT_FLAGS_ITEM (actions, MATEWNCK_WINDOW_ACTION_UNMAXIMIZE, _("unmaximize"));
  PRINT_FLAGS_ITEM (actions, MATEWNCK_WINDOW_ACTION_FULLSCREEN,
                    _("change fullscreen mode"));
  PRINT_FLAGS_ITEM (actions, MATEWNCK_WINDOW_ACTION_CLOSE, _("close"));
  PRINT_FLAGS_ITEM (actions, MATEWNCK_WINDOW_ACTION_ABOVE,
                    _("make above"));
  PRINT_FLAGS_ITEM (actions, MATEWNCK_WINDOW_ACTION_ABOVE,
                    _("unmake above"));
  PRINT_FLAGS_ITEM (actions, MATEWNCK_WINDOW_ACTION_BELOW,
                    _("make below"));
  PRINT_FLAGS_ITEM (actions, MATEWNCK_WINDOW_ACTION_BELOW,
                    _("unmake below"));
  if (!free_buf)
    free_buf = g_strdup (_("no action possible"));
  g_print (_("Possible Actions: %s\n"), free_buf);
  g_free (free_buf);

}

static gboolean
wm_state_set (Display *display,
              Window   window)
{
  Atom    wm_state;
  gulong  nitems;
  gulong  bytes_after;
  gulong *prop;
  Atom    ret_type = None;
  int     ret_format;
  int     err, result;

  wm_state = gdk_x11_get_xatom_by_name ("WM_STATE");

  gdk_error_trap_push ();
  result = XGetWindowProperty (display,
                               window,
                               wm_state,
                               0, G_MAXLONG,
                               False, wm_state, &ret_type, &ret_format, &nitems,
                               &bytes_after, (gpointer) &prop);
  err = gdk_error_trap_pop ();
  if (err != Success ||
      result != Success)
    return FALSE;

  XFree (prop);

  if (ret_type != wm_state)
    return FALSE;

  return TRUE;
}

static MateWnckWindow *
find_managed_window (Display *display,
                     Window   window)
{
  Window      root;
  Window      parent;
  Window     *kids = NULL;
  MateWnckWindow *retval;
  guint       nkids;
  int         i, result;

  if (wm_state_set (display, window))
    return matewnck_window_get (window);

  gdk_error_trap_push ();
  result = XQueryTree (display, window, &root, &parent, &kids, &nkids);
  if (gdk_error_trap_pop () || !result)
    return NULL;

  retval = NULL;

  for (i = 0; i < nkids; i++)
    {
      if (wm_state_set (display, kids [i]))
        {
          retval = matewnck_window_get (kids [i]);
          break;
        }

      retval = find_managed_window (display, kids [i]);
      if (retval != NULL)
        break;
    }

  if (kids)
    XFree (kids);

  return retval;
}

static void 
handle_button_press_event (XKeyEvent *event)
{
  if (event->subwindow == None)
    return;

  got_from_user = find_managed_window (event->display, event->subwindow);
}

static GdkFilterReturn
target_filter (GdkXEvent *gdk_xevent,
               GdkEvent  *event,
               gpointer   data)
{
  XEvent *xevent = (XEvent *) gdk_xevent;

  switch (xevent->type)
    {
      case ButtonPress:
        handle_button_press_event (&xevent->xkey);
        clean_up ();
        return GDK_FILTER_REMOVE;
      case KeyPress:
        if (xevent->xkey.keycode == XKeysymToKeycode (xevent->xany.display, XK_Escape))
          {
            clean_up ();
            return GDK_FILTER_REMOVE;
          }
        break;
      default:
        break;
    }

  return GDK_FILTER_CONTINUE;
}

static gboolean
get_target (gpointer data)
{
  GdkGrabStatus  status;
  GdkCursor     *cross;
  GdkWindow     *root;

  root = gdk_get_default_root_window ();

  gdk_window_add_filter (root, (GdkFilterFunc) target_filter, NULL);

  cross = gdk_cursor_new (GDK_CROSS);
  status = gdk_pointer_grab (root, FALSE, GDK_BUTTON_PRESS_MASK,
                             NULL, cross, GDK_CURRENT_TIME);
  gdk_cursor_unref (cross);

  if (status != GDK_GRAB_SUCCESS)
    {
      g_warning ("Pointer grab failed.\n");
      clean_up ();
      return FALSE;
    }

  status = gdk_keyboard_grab (root, FALSE, GDK_CURRENT_TIME);
  if (status != GDK_GRAB_SUCCESS)
    {
      g_warning ("Keyboard grab failed.\n");
      clean_up ();
      return FALSE;
    }

  gdk_flush ();

  return FALSE;
}

static void
clean_up (void)
{
  GdkWindow *root;

  root = gdk_get_default_root_window ();
  gdk_window_remove_filter (root, (GdkFilterFunc) target_filter, NULL);

  gdk_pointer_ungrab (GDK_CURRENT_TIME);
  gdk_keyboard_ungrab (GDK_CURRENT_TIME);

  gtk_main_quit ();
}

int
main (int argc, char **argv)
{
  GOptionContext *ctxt;
  GOptionGroup   *group;
  GError         *error;
  MateWnckScreen     *screen;
  
  bindtextdomain (GETTEXT_PACKAGE, MATEWNCK_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  ctxt = g_option_context_new (NULL);
  g_option_context_set_translation_domain (ctxt, GETTEXT_PACKAGE);

  g_option_context_set_summary (ctxt,
                                N_("Print or modify the properties of a "
                                   "screen/workspace/window, or interact with "
                                   "it, following the EWMH specification.\n"
                                   "For information about this specification, "
                                   "see:\n\t"
                                   "http://freedesktop.org/wiki/Specifications/wm-spec"));

  g_option_context_add_main_entries (ctxt, main_entries, GETTEXT_PACKAGE);

  group = g_option_group_new ("list",
                              N_("Options to list windows or workspaces"),
                              N_("Show options to list windows or workspaces"),
                              NULL, NULL);
  g_option_group_add_entries (group, list_entries);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);
  g_option_context_add_group (ctxt, group);

  group = g_option_group_new ("window",
                              N_("Options to modify properties of a window"),
                              N_("Show options to modify properties of a window"),
                              NULL, NULL);
  g_option_group_add_entries (group, window_entries);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);
  g_option_context_add_group (ctxt, group);

  group = g_option_group_new ("workspace",
                              N_("Options to modify properties of a workspace"),
                              N_("Show options to modify properties of a workspace"),
                              NULL, NULL);
  g_option_group_add_entries (group, space_entries);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);
  g_option_context_add_group (ctxt, group);

  group = g_option_group_new ("screen",
                              N_("Options to modify properties of a screen"),
                              N_("Show options to modify properties of a screen"),
                              NULL, NULL);
  g_option_group_add_entries (group, screen_entries);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);
  g_option_context_add_group (ctxt, group);

  g_option_context_add_group (ctxt, gtk_get_option_group (TRUE));

  error = NULL;
  if (!g_option_context_parse (ctxt, &argc, &argv, &error))
    {
      g_printerr (_("Error while parsing arguments: %s\n"), error->message);
      g_option_context_free (ctxt);
      g_error_free (error);
      return 1;
    }

  g_option_context_free (ctxt);
  ctxt = NULL;

  if (!validate_options ())
    return 1;

  gtk_init (&argc, &argv);

  matewnck_set_client_type (MATEWNCK_CLIENT_TYPE_PAGER);

  if ((option_screen && interact_screen < 0) || !option_screen)
    screen = matewnck_screen_get_default ();
  else
    {
      screen = matewnck_screen_get (interact_screen);
      if (!screen)
        {
         g_printerr (_("Cannot interact with screen %d: "
                       "the screen does not exist\n"), interact_screen);
          return 0;
        }
    }

  /* because we don't respond to signals at the moment */
  matewnck_screen_force_update (screen);

  if (option_workspace && interact_space < 0)
    {
      MateWnckWorkspace *space;
      space = matewnck_screen_get_active_workspace (screen);
      if (space == NULL)
        interact_space = 0;
      else
        interact_space = matewnck_workspace_get_number (space);
    }

  if (get_from_user)
    {
      g_idle_add (get_target, NULL);

      gtk_main ();

      if (!got_from_user)
        return 0;
    }
  
  if (mode == SCREEN_READ_MODE)
    print_screen (screen);
  else if (mode == SCREEN_LIST_MODE)
    list_screen (screen);
  else if (mode == SCREEN_WRITE_MODE)
    update_screen (screen);
  else if (mode == WORKSPACE_READ_MODE || mode == WORKSPACE_LIST_MODE ||
           mode == WORKSPACE_WRITE_MODE)
    {
      MateWnckWorkspace *space;

      g_assert (interact_space != -1);

      space = matewnck_screen_get_workspace (screen, interact_space);

      if (space)
        {
          if (mode == WORKSPACE_READ_MODE)
            print_workspace (space);
          else if (mode == WORKSPACE_LIST_MODE)
            list_workspace (space);
          else if (mode == WORKSPACE_WRITE_MODE)
            update_workspace (space);
          else
            g_assert_not_reached ();
        }
      else
        g_printerr (_("Cannot interact with workspace %d: "
                      "the workspace cannot be found\n"), interact_space);
    }
  else if (mode == CLASS_GROUP_READ_MODE || mode == CLASS_GROUP_LIST_MODE)
    {
      MateWnckClassGroup *class_group;

      if (got_from_user)
        class_group = matewnck_window_get_class_group (got_from_user);
      else
        class_group = matewnck_class_group_get (interact_class_group);

      if (class_group)
        {
          if (mode == CLASS_GROUP_READ_MODE)
            print_class_group (class_group);
          else if (mode == CLASS_GROUP_LIST_MODE)
            list_class_group (class_group);
          else
            g_assert_not_reached ();
        }
      else
        /* Translators: A class is like a "family". E.g., all gvim windows are
         * of the same class. */
        g_printerr (_("Cannot interact with class group \"%s\": "
                      "the class group cannot be found\n"),
                    interact_class_group);
    }
  else if (mode == APPLICATION_READ_MODE || mode == APPLICATION_LIST_MODE)
    {
      MateWnckApplication *app;

      if (got_from_user)
        app = matewnck_window_get_application (got_from_user);
      else
        app = matewnck_application_get (interact_app_xid);

      if (app)
        {
          if (mode == APPLICATION_READ_MODE)
            print_application (app);
          else if (mode == APPLICATION_LIST_MODE)
            list_application (app);
          else
            g_assert_not_reached ();
        }
      else
        g_printerr (_("Cannot interact with application having its group "
                      "leader with XID %lu: the application cannot be found\n"),
                    interact_app_xid);
    }
  else
    {
      MateWnckWindow *window;

      if (got_from_user)
        window = got_from_user;
      else
        window = matewnck_window_get (xid);

      if (window)
        {
          if (mode == WINDOW_WRITE_MODE)
            update_window (window);
          else if (mode == WINDOW_READ_MODE)
            print_window (window);
          else
            g_assert_not_reached ();
        }
      else
        g_printerr (_("Cannot interact with window with XID %lu: "
                      "the window cannot be found\n"), xid);
    }
  
  return 0;
}