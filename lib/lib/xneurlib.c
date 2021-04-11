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
 *  Copyright (C) 2006-2016 XNeur Team
 *
 */


#include <X11/XKBlib.h>
#include <X11/extensions/XInput2.h>

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef WITH_ASPELL
#	include <aspell.h>
#endif

#ifdef WITH_ENCHANT
#	include <enchant/enchant.h>
#endif

#include "xneur.h"
#include "window.h"

#include "xnconfig_files.h"

#include "list_char.h"
#include "types.h"

#include "switchlang.h"

#include "detection.h"

#include "log.h"

#include "buffer.h"

struct _window *main_window;

struct _xneur_config *xconfig				= NULL;

/*static int get_group(Display *dpy) {
	XkbStateRec state[1];
	memset(state, 0, sizeof(state));
	XkbGetState(dpy, XkbUseCoreKbd, state);
	return state->group;
}

static int get_layout(Display *dpy, char **names) {
	XkbDescRec desc[1];
	int gc;
	memset(desc, 0, sizeof(desc));
	desc->device_spec = XkbUseCoreKbd;
	XkbGetControls(dpy, XkbGroupsWrapMask, desc);
	XkbGetNames(dpy, XkbGroupNamesMask, desc);
	XGetAtomNames(dpy, desc->names->groups, gc = desc->ctrls->num_groups, names);
	XkbFreeControls(desc, XkbGroupsWrapMask, True);
	XkbFreeNames(desc, XkbGroupNamesMask, True);
	return gc;
}

static void free_layout(char **names, int gc) {
	for (; gc--; ++names)
		if (*names) {
			XFree(*names);
			*names = NULL;
		}
}*/

#if  defined(WITH_ASPELL) || defined(WITH_ENCHANT)
/// Returns name of dictionary for language
static const char* spell_name(const struct _xneur_language* lang)
{
	static const char* LAYOUT_NAMES[] =
	{
		"am","bg","by","cz","de","gr","ee","en","es","fr","ge","gb","kz","lt","lv",
		"pl","ro","ru","ua","us","uz"
	};

	static const char* SPELL_NAMES[] =
	{
		"hy","bg","be","cs","de","el","et","en","es","fr","ka","en","kk","lt","lv",
		"pl","ro","ru","uk","en","uz"
	};

	static const size_t NAMES_LEN = sizeof(LAYOUT_NAMES) / sizeof(LAYOUT_NAMES[0]);

	size_t i = 0;
	for (; i < NAMES_LEN; ++i)
	{
		if (strcmp(LAYOUT_NAMES[i], lang->dir) == 0) {
			break;
		}
	}
	return i != NAMES_LEN ? SPELL_NAMES[i] : NULL;
}
#endif

static long get_next_property_value (unsigned char **pointer, long unsigned *length, int size, char **string)
{
	if (size != 8)
		return 0;

	int len = 0; *string = (char *)*pointer;
	while ((len++, --*length, *((*pointer)++)) && *length>0);

	return len;
}

struct _xneur_handle *xneur_handle_create (void)
{
	struct _xneur_handle *handle = (struct _xneur_handle *) malloc(sizeof(struct _xneur_handle));
	if (handle == NULL)
		return NULL;

	XkbDescPtr kbd_desc_ptr;

	if (!(kbd_desc_ptr = XkbAllocKeyboard()))
	{
		free(handle);
		return NULL;
	}

	Display *display = XOpenDisplay(NULL);

	/*char *names[XkbNumKbdGroups];
	int gc = get_layout(display, names);
	int g = get_group(display);
	for (int i = 0; i < gc; i++)
	{
		printf("%d) %c %s\n", i, i == g ? '*' : ' ', names[i]);
	}
	free_layout(names, gc);*/

	XkbGetNames(display, XkbAllNamesMask, kbd_desc_ptr);

	if (kbd_desc_ptr->names == NULL)
	{
		XCloseDisplay(display);
		XkbFreeKeyboard(kbd_desc_ptr, XkbAllComponentsMask, True);
		free(handle);
		return NULL;
	}

	int groups_count = 0;
	for (; groups_count < XkbNumKbdGroups; groups_count++)
	{
		if (kbd_desc_ptr->names->groups[groups_count] == None)
			break;
	}

	if (groups_count == 0)
	{
		XCloseDisplay(display);
		XkbFreeKeyboard(kbd_desc_ptr, XkbAllComponentsMask, True);
		free(handle);
		return NULL;
	}

	Atom _XKB_RULES_NAMES = XInternAtom(display, "_XKB_RULES_NAMES", 1);
	if (_XKB_RULES_NAMES == None)
	{
		XCloseDisplay(display);
		XkbFreeKeyboard(kbd_desc_ptr, XkbAllComponentsMask, True);
		free(handle);
		return NULL;
	}
	Window rw = RootWindow(display, DefaultScreen(display));
	Atom type;
	int size;
	unsigned long nitems;
	unsigned long bytes_after;
	unsigned char *prop;
	int status;

	status = XGetWindowProperty(display, rw, _XKB_RULES_NAMES, 0, (10000+3)/4,
				False, AnyPropertyType, &type,
				&size, &nitems, &bytes_after,
				&prop);
	if (status != Success)
	{
		XCloseDisplay(display);
		XkbFreeKeyboard(kbd_desc_ptr, XkbAllComponentsMask, True);
		free(handle);
		return NULL;
	}

	int prop_count = 0;
	char *prop_value = NULL;
	while (nitems >= 1)
	{
		int prop_value_len = get_next_property_value(&prop, &nitems, size, &prop_value);
		if (prop_value_len == 0)
		{
			XCloseDisplay(display);
			XkbFreeKeyboard(kbd_desc_ptr, XkbAllComponentsMask, True);
			free(handle);
			return NULL;
		}

		prop_count++;
		// 1 - Keyboard Driver
		// 2 - Keyboard Model
		// 3 - Keyboard Layouts
		// 4 - Keyboard Variants
		// 5 - Keyboard Led and Grp
		if (prop_count == 3)
			break;
	}
	if (prop_count != 3)
	{
		XCloseDisplay(display);
		XkbFreeKeyboard(kbd_desc_ptr, XkbAllComponentsMask, True);
		free(handle);
		return NULL;
	}

	handle->languages = (struct _xneur_language *) malloc(sizeof(struct _xneur_language));
	handle->total_languages = 0;

	for (int group = 0; group < groups_count; group++)
	{
		Atom  group_atom = kbd_desc_ptr->names->groups[group];
		char *group_name = XGetAtomName(display, group_atom);
		char *short_name = strsep(&prop_value, ",");

		// Check double layout
		//
		for (int lang = 0; lang < handle->total_languages; lang++)
		{
			if (strcmp(handle->languages[lang].dir, short_name) == 0)
			{
				goto function_end;
			}
		}

		void *tmp = realloc(handle->languages, (handle->total_languages + 1) * sizeof(struct _xneur_language));
		if (tmp == NULL)
			continue;
		handle->languages = (struct _xneur_language *) tmp;
		memset(&(handle->languages[handle->total_languages]), 0, sizeof(struct _xneur_language));

		handle->languages[handle->total_languages].name	= strdup(group_name);
		handle->languages[handle->total_languages].dir	= strdup(short_name);
		tmp = realloc (handle->languages[handle->total_languages].dir, sizeof(char) * 3); // 'xx' + '\0'
		if (tmp == NULL)
			continue;
		handle->languages[handle->total_languages].dir	= (char *)tmp;
		handle->languages[handle->total_languages].dir[2] = NULLSYM;
		handle->languages[handle->total_languages].group	= group;
		handle->languages[handle->total_languages].excluded	= FALSE;
		handle->languages[handle->total_languages].disable_auto_detection	= FALSE;
		handle->total_languages++;

		if (prop_value == NULL)
			break;
		function_end:;
	}

	XCloseDisplay(display);
	XkbFreeKeyboard(kbd_desc_ptr, XkbAllComponentsMask, True);

	if (handle->total_languages == 0)
	{
		free(handle);
		return NULL;
	}

#ifdef WITH_ASPELL
	// init aspell spellers
	handle->spell_config = new_aspell_config();
	handle->spell_checkers = (AspellSpeller**) calloc(handle->total_languages, sizeof(AspellSpeller*));
#endif

#ifdef WITH_ENCHANT
	// init enchant brocker and dicts
	handle->enchant_broker = enchant_broker_init();
	handle->enchant_dicts = (EnchantDict**) calloc(handle->total_languages, sizeof(EnchantDict*));
#endif

	for (int lang = 0; lang < handle->total_languages; lang++)
	{
		struct _xneur_language* l = &handle->languages[lang];
		int path_len = strlen(LANGUAGEDIR) + strlen(l->dir) + 2;
		char *lang_dir = (char *) malloc(path_len * sizeof(char));
		snprintf(lang_dir, path_len, "%s/%s", LANGUAGEDIR, l->dir);

		l->dictionary = load_list(lang_dir, DICT_NAME, TRUE);
		l->proto      = load_list(lang_dir, PROTO_NAME, TRUE);
		l->big_proto  = load_list(lang_dir, BIG_PROTO_NAME, TRUE);
		l->pattern    = load_list(lang_dir, PATTERN_NAME, TRUE);
		l->temp_dictionary = l->dictionary->clone(l->dictionary);

		free(lang_dir);
	}

#ifdef WITH_ASPELL
	for (int lang = 0; lang < handle->total_languages; lang++)
	{
		const struct _xneur_language* l = &handle->languages[lang];
		// initialize aspell checker for current language
		const char* dict = spell_name(l);
		if (dict != NULL)
		{
			aspell_config_replace(handle->spell_config, "lang", dict);
			AspellCanHaveError *possible_err = new_aspell_speller(handle->spell_config);

			int aspell_error = aspell_error_number(possible_err);

			if (aspell_error != 0)
			{
				//printf("   [!] Error initialize %s aspell dictionary\n", l->name);
				delete_aspell_can_have_error(possible_err);
			}
			else
			{
				//printf("   [!] Initialize %s aspell dictionary\n", l->name);
				handle->spell_checkers[lang] = to_aspell_speller(possible_err);
			}
		}
	}
#endif

#ifdef WITH_ENCHANT
	for (int lang = 0; lang < handle->total_languages; lang++)
	{
		const struct _xneur_language* l = &handle->languages[lang];
		// initialize enchant checker for current language
		const char* dict = spell_name(l);
		if (dict != NULL)
		{
			size_t len1 = strlen(dict);
			size_t len2 = strlen(l->dir);
			// +1 for "_" and +1 for trailing zero
			char *dict_name = malloc(len1 + 1 + len2 + 1);
			if (dict_name == NULL)
				continue;
			dict_name[0] = '\0';
			strncat(dict_name, dict, len1);
			strncat(dict_name, "_", 1);
			strncat(dict_name, l->dir, len2);
			dict_name[3] = toupper(dict_name[3]);
			dict_name[4] = toupper(dict_name[4]);
			//printf("   [!] Try load dict %s\n", dict_name);
			if (!enchant_broker_dict_exists(handle->enchant_broker, dict_name))
			{
				dict_name[2] = '\0';
				//printf("   [!] Try load dict %s\n", dict_name);
				if (!enchant_broker_dict_exists(handle->enchant_broker, dict_name))
				{
					free(dict_name);
					continue;
				}
			}

			//printf("   [!] Loaded dict %s\n", dict_name);
			handle->enchant_dicts[lang] = enchant_broker_request_dict(handle->enchant_broker, dict_name);

			free(dict_name);
		}
	}
#endif

	for (int lang = 0; lang < handle->total_languages; lang++)
	{
		struct _xneur_language* l = &handle->languages[lang];

		l->disable_auto_detection |=
			l->excluded || (
				l->dictionary->data_count == 0 &&
				l->proto->data_count == 0 &&
				l->big_proto->data_count == 0
#ifdef WITH_ASPELL
				&& handle->spell_checkers[lang] == NULL
#endif
#ifdef WITH_ENCHANT
				&& handle->enchant_dicts[lang] == NULL
#endif
			);
	}

	return handle;
}

void xneur_handle_destroy (struct _xneur_handle *handle)
{
	if (handle == NULL)
		return;

	for (int lang = 0; lang < handle->total_languages; lang++)
	{
#ifdef WITH_ASPELL
		if (handle->spell_checkers[lang])
			delete_aspell_speller(handle->spell_checkers[lang]);
#endif

#ifdef WITH_ENCHANT
		if (handle->enchant_dicts[lang] != NULL)
			enchant_broker_free_dict(handle->enchant_broker, handle->enchant_dicts[lang]);
#endif

		struct _xneur_language* l = &handle->languages[lang];

		l->temp_dictionary->uninit(l->temp_dictionary);
		l->dictionary->uninit(l->dictionary);
		l->proto->uninit(l->proto);
		l->big_proto->uninit(l->big_proto);
		l->pattern->uninit(l->pattern);

		free(l->name);
		free(l->dir);
	}
	free(handle->languages);

#ifdef WITH_ASPELL
	delete_aspell_config(handle->spell_config);
	free(handle->spell_checkers);
#endif

#ifdef WITH_ENCHANT
	enchant_broker_free(handle->enchant_broker);
	free(handle->enchant_dicts);
#endif

	free(handle);
}

int xneur_get_layout (struct _xneur_handle *handle, char *word)
{
	if (!word || handle == NULL)
		return -1;
	struct _buffer *buffer = buffer_init(handle, main_window->keymap);

	buffer->set_content(buffer, word);
	int cur_lang = get_curr_keyboard_group();
	int new_lang = check_lang(handle, buffer, cur_lang, FALSE);

	buffer->uninit(buffer);

	// The word is suitable for all languages, return -1
	if (new_lang == NO_LANGUAGE)
		new_lang = -1;

	return new_lang;
}

char *xneur_get_word (struct _xneur_handle *handle, char *word)
{
	if (!word || handle == NULL)
		return NULL;

	char *result = NULL;

	struct _buffer *buffer = buffer_init(handle, main_window->keymap);

	buffer->set_content(buffer, word);
	int cur_lang = get_curr_keyboard_group();
	int new_lang = check_lang(handle, buffer, cur_lang, FALSE);
	if (new_lang == NO_LANGUAGE)
		result = strdup(word);
	else
		buffer->set_lang_mask(buffer, new_lang),
		result = buffer->get_utf_string(buffer);

	buffer->uninit(buffer);

	return result;
}


