/*
 * Copyright (c) 2010 Mike Massonnet, <mmassonnet@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "settings-menu.h"
#include "settings-dialog.h"
#include "settings.h"

static void
show_settings_dialog (GtkButton *button)
{
	GtkWidget *parent_window = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_WINDOW);
	GtkWidget *dialog = xtm_settings_dialog_new (GTK_WINDOW (parent_window));
	xtm_settings_dialog_run (XTM_SETTINGS_DIALOG (dialog));
	g_object_unref (dialog);
}

static void
refresh_rate_toggled (GtkCheckMenuItem *mi, XtmSettings *settings)
{
	if(gtk_check_menu_item_get_active(mi))
	{
		guint refresh_rate = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (mi), "refresh-rate"));
		g_object_set (settings, "refresh-rate", refresh_rate, NULL);
	}
}

static void
menu_refresh_rate_append_item (GtkMenu *menu, gchar *title, guint refresh_rate, XtmSettings *settings)
{
	GtkWidget *mi;
	guint cur_refresh_rate;
	static GSList *group = NULL;

	g_object_get (settings, "refresh-rate", &cur_refresh_rate, NULL);

	mi = gtk_radio_menu_item_new_with_label (group, title);
	group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM (mi));
	if (cur_refresh_rate == refresh_rate)
	{
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mi), TRUE);
	}
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
	g_object_set_data (G_OBJECT (mi), "refresh-rate", GUINT_TO_POINTER (refresh_rate));
	g_signal_connect (mi, "activate", G_CALLBACK (refresh_rate_toggled), settings);
}

static GtkWidget *
build_refresh_rate_menu (XtmSettings *settings)
{
	GtkWidget *menu;

	menu = gtk_menu_new ();

	/* TRANSLATORS: The next values are in seconds or milliseconds */
	menu_refresh_rate_append_item (GTK_MENU (menu), _("500ms"), 500, settings);
	menu_refresh_rate_append_item (GTK_MENU (menu), _("750ms"), 750, settings);
	menu_refresh_rate_append_item (GTK_MENU (menu), _("1s"), 1000, settings);
	menu_refresh_rate_append_item (GTK_MENU (menu), _("2s"), 2000, settings);
	menu_refresh_rate_append_item (GTK_MENU (menu), _("5s"), 5000, settings);
	menu_refresh_rate_append_item (GTK_MENU (menu), _("10s"), 10000, settings);

	return menu;
}

static void
item_toggled (GtkCheckMenuItem *mi, XtmSettings *settings)
{
	gboolean active = gtk_check_menu_item_get_active (mi);
	gchar *setting_name = g_object_get_data (G_OBJECT (mi), "setting-name");
	g_object_set (settings, setting_name, active, NULL);
}

static void
settings_notify (GObject *object, GParamSpec *pspec, GtkCheckMenuItem *mi)
{
	gboolean active;
	g_object_get (object, pspec->name, &active, NULL);
	gtk_check_menu_item_set_active (mi, active);
}

static void
menu_append_item (GtkMenu *menu, gchar *title, gchar *setting_name, XtmSettings *settings)
{
	GtkWidget *mi;
	gboolean active = FALSE;
	gchar *notify_name;

	g_object_get (settings, setting_name, &active, NULL);

	mi = gtk_check_menu_item_new_with_label (title);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mi), active);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
	g_object_set_data (G_OBJECT (mi), "setting-name", setting_name);
	g_signal_connect (mi, "toggled", G_CALLBACK (item_toggled), settings);

	notify_name = g_strdup_printf ("notify::%s", setting_name);
	g_signal_connect (settings, notify_name, G_CALLBACK (settings_notify), mi);
	g_free (notify_name);
}

static void
show_about_dialog (GtkWindow *window)
{
	const gchar *authors[] = {
		"(c) 2016 Simon Steinbeiss",
		"(c) 2014 Landry Breuil",
		"(c) 2014 Harald Judt",
		"(c) 2014 Peter de Ridder",
		"(c) 2008-2010 Mike Massonnet",
		"(c) 2005-2008 Johannes Zellner",
		"",
		"FreeBSD",
		"  \342\200\242 Mike Massonnet",
		"  \342\200\242 Oliver Lehmann",
		"",
		"OpenBSD",
		"  \342\200\242 Landry Breuil",
		"",
		"Linux",
		"  \342\200\242 Johannes Zellner",
		"  \342\200\242 Mike Massonnet",
		"",
		"OpenSolaris",
		"  \342\200\242 Mike Massonnet",
		"  \342\200\242 Peter Tribble",
		NULL };
	const gchar *license =
		"This program is free software; you can redistribute it and/or modify\n"
		"it under the terms of the GNU General Public License as published by\n"
		"the Free Software Foundation; either version 2 of the License, or\n"
		"(at your option) any later version.\n";

	//gtk_about_dialog_set_url_hook (url_hook_about_dialog, NULL, NULL);
	gtk_show_about_dialog (window,
		"program-name", _("Task Manager"),
		"version", PACKAGE_VERSION,
		"copyright", "Copyright \302\251 2005-2016 The Xfce development team",
		"logo-icon-name", "utilities-system-monitor",
		"comments", _("Easy to use task manager"),
		"license", license,
		"authors", authors,
		"translator-credits", _("translator-credits"),
		"website", "http://goodies.xfce.org/projects/applications/xfce4-taskmanager",
		"website-label", "goodies.xfce.org",
		NULL);
}

GtkWidget *construct_menu (void)
{
	XtmSettings *settings = xtm_settings_get_default ();
	GtkWidget *menu = gtk_menu_new ();
	GtkWidget *refresh_rate_menu;
	GtkWidget *mi;
	GtkWidget *window;

	menu_append_item (GTK_MENU (menu), _("Show all processes"), "show-all-processes", settings);

	refresh_rate_menu = build_refresh_rate_menu (settings);
	mi = gtk_menu_item_new_with_label (_("Refresh rate"));
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (mi), refresh_rate_menu);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);

	mi = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);

	menu_append_item (GTK_MENU (menu), _("PID"), "column-pid", settings);
	menu_append_item (GTK_MENU (menu), _("PPID"), "column-ppid", settings);
	menu_append_item (GTK_MENU (menu), _("State"), "column-state", settings);
	menu_append_item (GTK_MENU (menu), _("Virtual Bytes"), "column-vsz", settings);
	menu_append_item (GTK_MENU (menu), _("Private Bytes"), "column-rss", settings);
	menu_append_item (GTK_MENU (menu), _("UID"), "column-uid", settings);
	menu_append_item (GTK_MENU (menu), _("CPU"), "column-cpu", settings);
	menu_append_item (GTK_MENU (menu), _("Priority"), "column-priority", settings);

	mi = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);

	mi = gtk_menu_item_new_with_label (_("Settings"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
	g_signal_connect (mi, "activate", G_CALLBACK (show_settings_dialog), NULL);

	mi = gtk_menu_item_new_with_label (_("About"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
	window = gtk_widget_get_ancestor (mi, GTK_TYPE_WINDOW);
	g_signal_connect (mi, "activate", G_CALLBACK (show_about_dialog), GTK_WINDOW (window));

	mi = gtk_menu_item_new_with_label (_("Quit"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
	g_signal_connect (mi, "activate", G_CALLBACK (gtk_main_quit), NULL);

	gtk_widget_show_all (menu);

	return menu;
}
