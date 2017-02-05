/*
 * Copyright (c) 2005-2006 Johannes Zellner, <webmaster@nebulon.de>
 * Copyright (c) 2010 Mike Massonnet, <mmassonnet@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

#include "settings.h"
#include "process-window.h"
#include "task-manager.h"

static XtmSettings *settings;
static GtkWidget *window;
static GtkStatusIcon *status_icon;
static XtmTaskManager *task_manager;
static gboolean timeout = 0;

static void
status_icon_activated (void)
{
	if (!(gtk_widget_get_visible (window)))
		gtk_widget_show (window);
	else
		gtk_widget_hide (window);
}

static void
status_icon_popup_menu (GtkStatusIcon *_status_icon, guint button, guint activate_time)
{
	static GtkWidget *menu = NULL;

	if (menu == NULL)
	{
		GtkWidget *mi;
		GtkWidget *icon;

		icon = gtk_image_new_from_icon_name ("gtk-quit", GTK_ICON_SIZE_MENU);
		menu = gtk_menu_new ();
		mi = gtk_image_menu_item_new_with_label (_("Quit"));
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mi), icon);
		g_signal_connect (mi, "activate", G_CALLBACK (gtk_main_quit), NULL);
		gtk_container_add (GTK_CONTAINER (menu), mi);
		gtk_widget_show_all (menu);
	}

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, gtk_status_icon_position_menu, _status_icon, button, activate_time);
}

static void
show_hide_status_icon (void)
{
	gboolean show_status_icon;
	g_object_get (settings, "show-status-icon", &show_status_icon, NULL);
	gtk_status_icon_set_visible (status_icon, show_status_icon);
}

static void
destroy_window (void)
{
	if (gtk_main_level () > 0)
		gtk_main_quit ();
}

static gboolean
delete_window (void)
{
	if (!gtk_status_icon_get_visible (status_icon))
	{
		gtk_main_quit ();
		return FALSE;
	}
	gtk_widget_hide (window);
	return TRUE;
}

static gboolean
init_timeout (void)
{
	guint num_processes;
	gfloat cpu, memory_percent, swap_percent;
	guint64 swap_used, swap_free, swap_total, memory_used, memory_total;
	gchar *used, *total, tooltip[1024], memory_info[64], swap_info[64];
	gboolean show_memory_in_xbytes;

	xtm_task_manager_get_system_info (task_manager, &num_processes, &cpu, &memory_used, &memory_total, &swap_used, &swap_total);

	memory_percent = (memory_total != 0) ? memory_used * 100 / (gdouble)memory_total : 0;
	swap_percent = (swap_total != 0) ? swap_used * 100 / (gdouble)swap_total : 0;

	g_object_get (settings, "show-memory-in-xbytes", &show_memory_in_xbytes, NULL);
	if (show_memory_in_xbytes) {
	  used = g_format_size_full(memory_used, G_FORMAT_SIZE_IEC_UNITS);
	  total = g_format_size_full(memory_total, G_FORMAT_SIZE_IEC_UNITS);
	  g_snprintf (memory_info, 64,"%s / %s", used, total);
	  g_free(used);
	  g_free(total);

	  used = g_format_size_full(swap_used, G_FORMAT_SIZE_IEC_UNITS);
	  total = g_format_size_full(swap_total, G_FORMAT_SIZE_IEC_UNITS);
	  g_snprintf (swap_info, 64,"%s / %s", used, total);
	  g_free(used);
	  g_free(total);
	} else {
	  g_snprintf (memory_info, 64, "%.0f%%", memory_percent);
	  g_snprintf (swap_info, 64, "%.0f%%", swap_percent);
	}

	xtm_process_window_set_system_info (XTM_PROCESS_WINDOW (window), num_processes, cpu, memory_percent, memory_info, swap_percent, swap_info);

	xtm_task_manager_get_swap_usage (task_manager, &swap_free, &swap_total);
	xtm_process_window_show_swap_usage (XTM_PROCESS_WINDOW (window), (swap_total > 0));

	if (gtk_status_icon_get_visible (status_icon))
	{
		g_snprintf (tooltip, 1024,
				_("Processes: %u\n"
				"CPU: %.0f%%\n"
				"Memory: %s\n"
				"Swap: %s"),
				num_processes, cpu, memory_info, swap_info);
		gtk_status_icon_set_tooltip (GTK_STATUS_ICON (status_icon), tooltip);
	}

	xtm_task_manager_update_model (task_manager);

	if (timeout == 0)
	{
		guint refresh_rate;
		g_object_get (settings, "refresh-rate", &refresh_rate, NULL);
		timeout = g_timeout_add (refresh_rate, (GSourceFunc)init_timeout, NULL);
	}

	return TRUE;
}

static void
force_timeout_update (void)
{
	init_timeout ();
}

static void
refresh_rate_changed (void)
{
	if (!g_source_remove (timeout))
	{
		g_critical ("Unable to remove source");
		return;
	}
	timeout = 0;
	init_timeout ();
}

int main (int argc, char *argv[])
{
	GApplication *app;
	GError *error = NULL;
#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	gtk_init (&argc, &argv);
	g_set_application_name (_("Task Manager"));

	app = g_application_new ("xfce.taskmanager", 0);
	g_application_register (G_APPLICATION (app), NULL, &error);
	if (error != NULL)
	{
		g_warning ("Unable to register GApplication: %s", error->message);
		g_error_free (error);
		error = NULL;
	}

	if (g_application_get_is_remote (G_APPLICATION (app)))
	{
		g_application_activate (G_APPLICATION (app));
		g_object_unref (app);
		return 0;
	}

	settings = xtm_settings_get_default ();

	status_icon = gtk_status_icon_new_from_icon_name ("utilities-system-monitor");
	show_hide_status_icon ();
	g_signal_connect (status_icon, "activate", G_CALLBACK (status_icon_activated), NULL);
	g_signal_connect (status_icon, "popup-menu", G_CALLBACK (status_icon_popup_menu), NULL);

	window = xtm_process_window_new ();
	gtk_widget_show (window);
	g_signal_connect_swapped (app, "activate", G_CALLBACK (xtm_process_window_show), window);

	task_manager = xtm_task_manager_new (xtm_process_window_get_model (XTM_PROCESS_WINDOW (window)));
	g_message ("Running as %s on %s", xtm_task_manager_get_username (task_manager), xtm_task_manager_get_hostname (task_manager));

	init_timeout ();
	g_signal_connect (settings, "notify::refresh-rate", G_CALLBACK (refresh_rate_changed), NULL);
	g_signal_connect_after (settings, "notify::more-precision", G_CALLBACK (force_timeout_update), NULL);
	g_signal_connect_after (settings, "notify::full-command-line", G_CALLBACK (force_timeout_update), NULL);
	g_signal_connect (settings, "notify::show-status-icon", G_CALLBACK (show_hide_status_icon), NULL);
	g_signal_connect (settings, "notify::show-memory-in-xbytes", G_CALLBACK (force_timeout_update), NULL);

	g_signal_connect (window, "destroy", G_CALLBACK (destroy_window), NULL);
	g_signal_connect (window, "delete-event", G_CALLBACK (delete_window), NULL);

	gtk_main ();

	if (timeout > 0)
		g_source_remove (timeout);
	g_object_unref (window);
	g_object_unref (status_icon);
	g_object_unref (task_manager);
	g_object_unref (settings);

	return 0;
}
