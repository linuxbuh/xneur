/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  Copyright (C) 2006-2010 XNeur Team
 *
 */

#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "xnconfig_files.h"

#include "window.h"
#include "keymap.h"

#include "types.h"
#include "list_char.h"
#include "text.h"

#define NEW_LANG_DIR	"new"
#define NEW_LANG_TEXT	"new.text"

extern struct _window *main_window;
int need_skip(char ch) {
	return isblank(ch) || iscntrl(ch) || isspace(ch) || ispunct(ch) || isdigit(ch);
}
void generate(struct _keymap *keymap, struct _list_char *proto2, struct _list_char *proto3, const char* text, int i, int group, int state) {
	char *sym_i = keymap->keycode_to_symbol(keymap, i, group, state);
	if (need_skip(sym_i[0])) {
		free(sym_i);
		return;
	}
	const size_t sym_i_len = strlen(sym_i);
	for (int j = 0; j < 100; j++)
	{
		char *sym_j = keymap->keycode_to_symbol(keymap, j, group, state);
		if (need_skip(sym_j[0])) {
			free(sym_j);
			continue;
		}

		char buffer[256 + 1];
		const size_t BUF_SIZE = sizeof(buffer) / sizeof(buffer[0]);

		strncpy(buffer, sym_i, BUF_SIZE);
		strncat(buffer, sym_j, BUF_SIZE - sym_i_len);

		if (proto2->exist(proto2, buffer, BY_PLAIN)) {
			free(sym_j);
			continue;
		}

		if (strstr(text, buffer) == NULL)
		{
			proto2->add(proto2, buffer);
			free(sym_j);
			continue;
		}

		for (int k = 0; k < 100; k++)
		{
			char *sym_k = keymap->keycode_to_symbol(keymap, k, group, state);
			if (need_skip(sym_k[0])) {
				free(sym_k);
				continue;
			}

			snprintf(buffer, BUF_SIZE, "%s%s%s", sym_i, sym_j, sym_k);
			free(sym_k);

			if (proto3->exist(proto3, buffer, BY_PLAIN))
				continue;

			if (strstr(text, buffer) != NULL)
				continue;

			proto3->add(proto3, buffer);
		}
		free(sym_j);
	}
	free(sym_i);
}
void generate_protos(void)
{
	printf("THIS OPTION FOR DEVELOPERS ONLY!\n");
	printf("\nPlease, define new language group and press Enter to continue...\n");
	printf("(see above keyboard layouts groups presented in system): \n");

	struct _keymap *keymap = main_window->keymap;

	int new_lang_group;
	if (!scanf("%d", &new_lang_group))
		exit(EXIT_SUCCESS);

	if (new_lang_group < 0 || new_lang_group > 3)
	{
		printf("New language group is bad! Aborting!\n");
		exit(EXIT_SUCCESS);
	}

	printf("\nSpecified new language group: %d\n", new_lang_group);
	char *path = get_file_path_name(NEW_LANG_DIR, NEW_LANG_TEXT);
	if (path == NULL)
		return;
	char *text = get_file_content(path);
	free(path);
	if (text == NULL)
	{
		printf("New language text file not find! Aborting!\n");
		exit(EXIT_FAILURE);
	}

	struct _list_char *proto2 = list_char_init();//-V656
	struct _list_char *proto3 = list_char_init();//-V656

	for (int i = 0; i < 100; i++)
	{
		printf("%d\n", i);

		generate(keymap, proto2, proto3, text, i, new_lang_group, 0);
		generate(keymap, proto2, proto3, text, i, new_lang_group, 1 << 7);
	}

	free(text);

	char *proto_file_path = get_file_path_name(NEW_LANG_DIR, "proto");
	FILE *stream = fopen(proto_file_path, "w");
	if (stream == NULL)
	{
		free(proto_file_path);
		proto2->uninit(proto2);
		proto3->uninit(proto3);
		return;
	}
	proto2->save(proto2, stream);
	printf("Short proto writed (%d) to %s\n", proto2->data_count, proto_file_path);
	fclose(stream);
	free(proto_file_path);

	char *proto3_file_path = get_file_path_name(NEW_LANG_DIR, "proto3");
	stream = fopen(proto3_file_path, "w");
	if (stream == NULL)
	{
		free(proto3_file_path);
		proto2->uninit(proto2);
		proto3->uninit(proto3);
		return;
	}
	proto3->save(proto3, stream);
	printf("Big proto writed (%d) to %s\n", proto3->data_count, proto3_file_path);
	fclose(stream);
	free(proto3_file_path);

	proto2->uninit(proto2);
	proto3->uninit(proto3);

	printf("End of generation!\n");
}
