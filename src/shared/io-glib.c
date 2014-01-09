/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2012  Intel Corporation. All rights reserved.
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

#include "src/shared/io.h"

struct io {
	GIOChannel *channel;
	guint read_watch;
	io_callback_func_t read_callback;
	io_destroy_func_t read_destroy;
	void *read_data;
	guint write_watch;
	io_callback_func_t write_callback;
	io_destroy_func_t write_destroy;
	void *write_data;
};

struct io *io_new(int fd)
{
	struct io *io;

	if (fd < 0)
		return NULL;

	io = g_try_new0(struct io, 1);
	if (!io)
		return NULL;

	io->channel = g_io_channel_unix_new(fd);

	g_io_channel_set_encoding(io->channel, NULL, NULL);
	g_io_channel_set_buffered(io->channel, FALSE);

	g_io_channel_set_close_on_unref(io->channel, FALSE);

	io->read_watch = 0;
	io->read_callback = NULL;
	io->read_destroy = NULL;
	io->read_data = NULL;

	io->write_watch = 0;
	io->write_callback = NULL;
	io->write_destroy = NULL;
	io->write_data = NULL;

	return io;
}

void io_destroy(struct io *io)
{
	if (!io)
		return;

	if (io->read_watch > 0)
		g_source_remove(io->read_watch);

	if (io->write_watch > 0)
		g_source_remove(io->write_watch);

	g_io_channel_unref(io->channel);

	g_free(io);
}

int io_get_fd(struct io *io)
{
	if (!io)
		return -1;

	return g_io_channel_unix_get_fd(io->channel);
}

bool io_set_close_on_destroy(struct io *io, bool do_close)
{
	if (!io)
		return false;

	if (do_close)
		g_io_channel_set_close_on_unref(io->channel, TRUE);
	else
		g_io_channel_set_close_on_unref(io->channel, FALSE);

	return true;
}

static void read_watch_destroy(gpointer user_data)
{
	struct io *io = user_data;

	if (io->read_destroy)
		io->read_destroy(io->read_data);

	io->read_watch = 0;
	io->read_callback = NULL;
	io->read_destroy = NULL;
	io->read_data = NULL;
}

static gboolean read_callback(GIOChannel *channel, GIOCondition cond,
							gpointer user_data)
{
	struct io *io = user_data;
	bool result;

	if (cond & (G_IO_HUP | G_IO_ERR | G_IO_NVAL))
		return FALSE;

	if (io->read_callback)
		result = io->read_callback(io, io->read_data);
	else
		result = false;

	return result ? TRUE : FALSE;
}

bool io_set_read_handler(struct io *io, io_callback_func_t callback,
				void *user_data, io_destroy_func_t destroy)
{
	if (!io)
		return false;

	if (io->read_watch > 0) {
		g_source_remove(io->read_watch);
		io->read_watch = 0;
	}

	if (!callback)
		goto done;

	io->read_watch = g_io_add_watch_full(io->channel, G_PRIORITY_DEFAULT,
				G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
				read_callback, io, read_watch_destroy);
	if (io->read_watch == 0)
		return false;

done:
	io->read_callback = callback;
	io->read_destroy = destroy;
	io->read_data = user_data;

	return true;
}

static void write_watch_destroy(gpointer user_data)
{
	struct io *io = user_data;

	if (io->write_destroy)
		io->write_destroy(io->write_data);

	io->write_watch = 0;
	io->write_callback = NULL;
	io->write_destroy = NULL;
	io->write_data = NULL;
}

static gboolean write_callback(GIOChannel *channel, GIOCondition cond,
							gpointer user_data)
{
	struct io *io = user_data;
	bool result;

	if (cond & (G_IO_HUP | G_IO_ERR | G_IO_NVAL))
		return FALSE;

	if (io->write_callback)
		result = io->write_callback(io, io->write_data);
	else
		result = false;

	return result ? TRUE : FALSE;
}

bool io_set_write_handler(struct io *io, io_callback_func_t callback,
				void *user_data, io_destroy_func_t destroy)
{
	if (!io)
		return false;

	if (io->write_watch > 0) {
		g_source_remove(io->write_watch);
		io->write_watch = 0;
	}

	if (!callback)
		goto done;

	io->write_watch = g_io_add_watch_full(io->channel, G_PRIORITY_DEFAULT,
				G_IO_OUT | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
				write_callback, io, write_watch_destroy);
	if (io->write_watch == 0)
		return false;

done:
	io->write_callback = callback;
	io->write_destroy = destroy;
	io->write_data = user_data;

	return true;
}

bool io_set_disconnect_handler(struct io *io, io_callback_func_t callback,
				void *user_data, io_destroy_func_t destroy)
{
	return false;
}