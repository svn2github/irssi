/*
 translation.c : irssi

    Copyright (C) 1999-2000 Timo Sirainen

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "module.h"
#include "signals.h"
#include "line-split.h"
#include "misc.h"
#include "settings.h"

unsigned char translation_in[256], translation_out[256];

void translation_reset(void)
{
	int n;

	for (n = 0; n < 256; n++)
		translation_in[n] = (unsigned char) n;
	for (n = 0; n < 256; n++)
		translation_out[n] = (unsigned char) n;
}

void translate_output(char *text)
{
	while (*text != '\0') {
		*text = (char) translation_out[(int) (unsigned char) *text];
		text++;
	}
}

#define gethex(a) \
	(isdigit(a) ? ((a)-'0') : (toupper(a)-'A'+10))

void translation_parse_line(const char *str, int *pos)
{
	const char *ptr;
	int value;

	for (ptr = str; *ptr != '\0'; ptr++) {
		if (ptr[0] != '0' || ptr[1] != 'x')
			break;
		ptr += 2;

		value = (gethex(ptr[0]) << 4) + gethex(ptr[1]);
		if (*pos < 256)
			translation_in[*pos] = (unsigned char) value;
		else
			translation_out[*pos-256] = (unsigned char) value;
		(*pos)++;

		ptr += 2;
		if (*ptr != ',') break;
	}
}

int translation_read(const char *file)
{
	char tmpbuf[1024], *str, *path;
	LINEBUF_REC *buffer;
	int f, pos, ret, recvlen;

	g_return_val_if_fail(file != NULL, FALSE);

	path = convert_home(file);
	f = open(file, O_RDONLY);
	g_free(path);

	if (f == -1) return FALSE;

	pos = 0; buffer = NULL;
	while (pos < 512) {
		recvlen = read(f, tmpbuf, sizeof(tmpbuf));

		ret = line_split(tmpbuf, recvlen, &str, &buffer);
		if (ret <= 0) break;

                translation_parse_line(str, &pos);
	}
	line_split_free(buffer);

	close(f);
	if (pos != 512)
		translation_reset();
	return pos == 512;
}

static void read_settings(void)
{
	translation_read(settings_get_str("translation"));
}

void translation_init(void)
{
	translation_reset();

	settings_add_str("misc", "translation", "");
	signal_add("setup changed", (SIGNAL_FUNC) read_settings);

	read_settings();
}

void translation_deinit(void)
{
	read_settings();
	signal_remove("setup changed", (SIGNAL_FUNC) read_settings);
}
