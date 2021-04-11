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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#include "switchlang.h"

#include "keymap.h"
#include "window.h"

#include "types.h"
#include "utils.h"
#include "list_char.h"
#include "log.h"
#include "text.h"

#include "detection.h"

#define PROTO_LEN	2
#define BIG_PROTO_LEN	3
#define LEVENSHTEIN_LEN	3

static int get_dictionary_lang(struct _xneur_handle *handle, char **word)
{
	for (int lang = 0; lang < handle->total_languages; lang++)
	{
		if (handle->languages[lang].disable_auto_detection || handle->languages[lang].excluded)
			continue;
		if (strlen(word[lang]) == 0)
			continue;
		if (handle->languages[lang].dictionary->exist(handle->languages[lang].dictionary, word[lang], BY_REGEXP))
		{
			log_message(DEBUG, _("   [+] Found this word in %s language dictionary"), handle->languages[lang].name);
			return lang;
		}
	}

	log_message(DEBUG, _("   [-] This word not found in any dictionaries"));
	return NO_LANGUAGE;
}

#ifdef WITH_ASPELL
static int get_aspell_hits(struct _xneur_handle *handle, char **word, int **sym_len, int cur_lang)
{
	// check for current language first
	if (handle->spell_checkers[cur_lang])
	{
		const struct _xneur_language* l = &handle->languages[cur_lang];
		size_t word_len = strlen(word[cur_lang]);
		if (word_len > 0
		 && word_len / sym_len[cur_lang][0] > 1)
		 && !l->disable_auto_detection
		 && !l->excluded
		 && aspell_speller_check(handle->spell_checkers[cur_lang], word[cur_lang], word_len)
		) {
			log_message(DEBUG, _("   [+] Found this word in %s aspell dictionary"), l->name);
			return cur_lang;
		}
	}
	else
	{
		log_message(DEBUG, _("   [!] Now we don't support aspell dictionary for %s layout"), handle->languages[cur_lang].name);
	}

	// check for another languages
	for (int lang = 0; lang < handle->total_languages; lang++)
	{
		const struct _xneur_language* l = &handle->languages[lang];
		size_t word_len = strlen(word[lang]);
		if (l->disable_auto_detection || l->excluded || lang == cur_lang || word_len == 0)
			continue;

		if (!handle->spell_checkers[lang])
		{
			log_message(DEBUG, _("   [!] Now we don't support aspell dictionary for %s layout"), l->name);
			continue;
		}

		if (word_len / sym_len[lang][0] > 1
		 && aspell_speller_check(handle->spell_checkers[lang], word[lang], word_len)
		) {
			log_message(DEBUG, _("   [+] Found this word in %s aspell dictionary"), l->name);
			return lang;
		}
	}

	log_message(DEBUG, _("   [-] This word has no hits for all aspell dictionaries"));
	return NO_LANGUAGE;
}
#endif

#ifdef WITH_ENCHANT
static int get_enchant_hits(struct _xneur_handle *handle, char **word, int **sym_len, int cur_lang)
{
	// check for current language first
	EnchantDict* cur_dict = handle->enchant_dicts[cur_lang];
	if (cur_dict)
	{
		const struct _xneur_language* l = &handle->languages[cur_lang];
		size_t word_len = strlen(word[cur_lang]);
		if (word_len > 0
		 && word_len / sym_len[cur_lang][0] > 1
		 && !l->disable_auto_detection
		 && !l->excluded
		 && !enchant_dict_check(cur_dict, word[cur_lang], word_len)
		) {
			log_message(DEBUG, _("   [+] Found this word in %s enchant wrapper dictionary"), l->name);
			return cur_lang;
		}
	}
	else
	{
		log_message(DEBUG, _("   [!] Now we don't support enchant wrapper dictionary for %s layout"), handle->languages[cur_lang].name);
	}

	// check for another languages
	for (int lang = 0; lang < handle->total_languages; lang++)
	{
		const struct _xneur_language* l = &handle->languages[lang];
		size_t word_len = strlen(word[lang]);
		if (l->disable_auto_detection || l->excluded || lang == cur_lang || word_len <= 0)
			continue;

		EnchantDict* dict = handle->enchant_dicts[lang];
		if (!dict)
		{
			log_message(DEBUG, _("   [!] Now we don't support enchant wrapper dictionary for %s layout"), l->name);
			continue;
		}

		if (word_len / sym_len[lang][0] > 1 && !enchant_dict_check(dict, word[lang], word_len))
		{
			log_message(DEBUG, _("   [+] Found this word in %s enchant wrapper dictionary"), l->name);
			return lang;
		}
	}

	log_message(DEBUG, _("   [-] This word has no hits for all enchant wrapper dictionaries"));
	return NO_LANGUAGE;
}
#endif

static int get_proto_hits(struct _list_char *p, int proto_len, char *word, int *sym_len, int len, int offset)
{
	int n_bytes = 0;
	for (int i = 0; i < proto_len; i++)
		n_bytes += sym_len[i];

	char *proto = (char *) malloc((n_bytes + 1) * sizeof(char));

	int local_offset = 0;
	for (int i = 0; i <= len - offset - proto_len; ++i)
	{
		strncpy(proto, word + local_offset, n_bytes);
		proto[n_bytes] = '\0';

		if (p->exist(p, proto, BY_PLAIN))
		{
			free(proto);
			return TRUE;
		}

		local_offset += sym_len[i];
	}

	free(proto);
	return FALSE;
}

static int get_proto_lang(struct _xneur_handle *handle, char **word, int **sym_len, int len, int offset, int cur_lang, int proto_len)
{
	if (len < proto_len)
	{
		log_message(DEBUG, _("   [-] Skip checking by language proto of size %d (word is very short)"), proto_len);
		return NO_LANGUAGE;
	}

	struct _xneur_language* cur_l = &handle->languages[cur_lang];
	struct _list_char* cur_proto = proto_len == PROTO_LEN
		? cur_l->proto
		: cur_l->big_proto;

	// NOLINTNEXTLINE(clang-analyzer-core.CallAndMessage): clang-tidy false-positive uninitialized access
	int hits = get_proto_hits(cur_proto, proto_len, word[cur_lang], sym_len[cur_lang], len, offset);
	if (hits == 0)
	{
		log_message(DEBUG, _("   [-] This word is ok for %s proto of size %d"), cur_l->name, proto_len);
		return cur_lang;
	}

	log_message(DEBUG, _("   [*] This word has hits for %s proto of size %d"), cur_l->name, proto_len);

	for (int lang = 0; lang < handle->total_languages; lang++)
	{
		struct _xneur_language* l = &handle->languages[lang];
		struct _list_char* proto = proto_len == PROTO_LEN
			? l->proto
			: l->big_proto;

		if (lang == cur_lang || l->disable_auto_detection || l->excluded)
			continue;

		// NOLINTNEXTLINE(clang-analyzer-core.CallAndMessage): clang-tidy false-positive uninitialized access
		if (strlen(word[lang]) == 0)
			continue;

		int hits = get_proto_hits(proto, proto_len, word[lang], sym_len[lang], len, offset);
		if (hits != 0)
		{
			log_message(DEBUG, _("   [*] This word has hits for %s language proto of size %d"), l->name, proto_len);
			continue;
		}

		log_message(DEBUG, _("   [+] This word has no hits for %s language proto of size %d"), l->name, proto_len);
		return lang;
	}

	log_message(DEBUG, _("   [-] This word has hits in all languages proto of size %d"), proto_len);
	return NO_LANGUAGE;
}

static int get_similar_words(struct _xneur_handle *handle, struct _buffer *p)
{
	int min_levenshtein = LEVENSHTEIN_LEN;
	char *possible_words = NULL;
	int possible_lang = NO_LANGUAGE;

	int lang = 0;
	for (lang = 0; lang < handle->total_languages; lang++)
	{
		char *word = strdup(p->get_last_word(p, p->i18n_content[lang].content));
		if (word == NULL)
			continue;

		del_final_numeric_char(word);

		if (handle->languages[lang].disable_auto_detection || handle->languages[lang].excluded)
		{
			free(possible_words);
			free(word);
			continue;
		}

		int word_len = strlen(p->get_last_word(p, p->content));

		if ((word_len > 250) || (word_len < 2))
		{
			free(possible_words);
			free(word);
			continue;
		}

		word_len = strlen(word);

		int offset = 0;
		for (offset = 0; offset < word_len; offset++)
		{
			if (!ispunct(word[offset]))
				break;
		}

#ifdef WITH_ENCHANT
		EnchantDict* dict = handle->enchant_dicts[lang];
		if (!dict)
		{
			free(possible_words);
			free(word);
			continue;
		}

		size_t count = 0;
		char **suggs = enchant_dict_suggest(dict, word+offset, strlen(word+offset), &count);
		if (count > 0)
		{
			for (size_t i = 0; i < count; i++)
			{
				int tmp_levenshtein = levenshtein(word+offset, suggs[i]);
				if (tmp_levenshtein < min_levenshtein)
				{
					min_levenshtein = tmp_levenshtein;
					free(possible_words);
					possible_words = strdup(suggs[i]);
					possible_lang = lang;
				}
				//log_message (ERROR, "    %d. %s (%d)", i+1, suggs[i], levenshtein(word, suggs[i]));
			}
		}
		enchant_dict_free_string_list(dict, suggs);
#endif

#ifdef WITH_ASPELL
		if (!handle->spell_checkers[lang])
		{
			free(possible_words);
			free(word);
			continue;
		}
		const AspellWordList *suggestions = aspell_speller_suggest(handle->spell_checkers[lang], (const char *) word+offset, strlen(word+offset));
		if (! suggestions)
		{
			free(possible_words);
			free(word);
			continue;
		}

		AspellStringEnumeration *elements = aspell_word_list_elements(suggestions);
		const char *sugg_word;
		int i = 0;
		while ((sugg_word = aspell_string_enumeration_next (elements)) != NULL)
		{
			int tmp_levenshtein = levenshtein(word+offset, sugg_word);
			if (tmp_levenshtein < min_levenshtein)
			{
				min_levenshtein = tmp_levenshtein;
				free(possible_words);
				possible_words = strdup(sugg_word);
				possible_lang = lang;

			}
			i++;
			//log_message (ERROR, "    %d. %s (%d) (%d)", i, sugg_word, levenshtein(word, sugg_word), damerau_levenshtein(word, sugg_word, 1, 1, 1, 1));
		}

		delete_aspell_string_enumeration (elements);
#endif
		free(word);
	}

	if (possible_words == NULL)
	{
		log_message(DEBUG, _("   [-] This word has no suggest for all dictionaries"));
		return possible_lang;

	}

	log_message(DEBUG, _("   [+] Found suggest word '%s' in %s dictionary (Levenshtein distance = %d)"),
						possible_words, handle->languages[possible_lang].name, min_levenshtein);
	free(possible_words);
	return possible_lang;
}

int check_lang(struct _xneur_handle *handle, struct _buffer *p, int cur_lang, int check_similar_words)
{
	char **word = (char **) malloc(handle->total_languages * sizeof(char *));
	char **word_base = (char **) malloc(handle->total_languages * sizeof(char *));
	char **word_unchanged = (char **) malloc(handle->total_languages * sizeof(char *));
	char **word_unchanged_base = (char **) malloc(handle->total_languages * sizeof(char *));
	int **sym_len = (int **) malloc(handle->total_languages * sizeof(int *));

	log_message(DEBUG, _("Processing word:"));
	for (int i = 0; i < handle->total_languages; i++)
	{
		char* last_word           = strdup(p->get_last_word(p, p->i18n_content[i].content));
		char* last_word_unchanged = strdup(p->get_last_word(p, p->i18n_content[i].content_unchanged));
		del_final_numeric_char(last_word);
		del_final_numeric_char(last_word_unchanged);

		// Offset of first non-punctuation or digit symbol
		size_t offset = 0;
		for (; offset < strlen(last_word); ++offset)
		{
			if (!ispunct(last_word[offset]) && (!isdigit(last_word[offset])))
				break;
		}
		word[i] = last_word + offset;
		word_base[i] = last_word;

		word_unchanged[i] = last_word_unchanged + offset;
		word_unchanged_base[i] = last_word_unchanged;

		log_message(DEBUG, _("   '%s' on layout '%s'"), word_unchanged[i], handle->languages[i].dir);

		sym_len[i] = p->i18n_content[i].symbol_len + p->get_last_word_offset(p, p->content, strlen(p->content));
	}

	log_message(DEBUG, _("Start word processing..."));

	// Check by dictionary
	int lang = get_dictionary_lang(handle, word);

#ifdef WITH_ENCHANT
	// Check by enchant wrapper
	if (lang == NO_LANGUAGE)
		lang = get_enchant_hits(handle, word, sym_len, cur_lang);
#endif

#ifdef WITH_ASPELL
	// Check by aspell
	if (lang == NO_LANGUAGE)
		lang = get_aspell_hits(handle, word, sym_len, cur_lang);
#endif

	// Check misprint
	if (check_similar_words && lang == NO_LANGUAGE)
		lang = get_similar_words (handle, p);

	// If not found in dictionary, try to find in proto
	int len = strlen(p->content);
	int offset = p->get_last_word_offset(p, p->content, len);
	if (lang == NO_LANGUAGE)
		lang = get_proto_lang(handle, word, sym_len, len, offset, cur_lang, PROTO_LEN);

	if (lang == NO_LANGUAGE)
		lang = get_proto_lang(handle, word, sym_len, len, offset, cur_lang, BIG_PROTO_LEN);

	log_message(DEBUG, _("End word processing."));

	for (int i = 0; i < handle->total_languages; i++)
	{
		// NOLINTNEXTLINE(clang-analyzer-core.CallAndMessage): clang-tidy false-positive uninitialized access
		free(word_base[i]);
		free(word_unchanged_base[i]);
	}
	free(word);
	free(word_unchanged);
	free(word_base);
	free(word_unchanged_base);
	free(sym_len);
	return lang;
}

