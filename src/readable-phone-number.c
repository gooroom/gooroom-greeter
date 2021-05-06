/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <ctype.h>

#include "readable-phone-number.h"


typedef struct
{
    gchar			*id;
    gchar			*country_code;
    gchar			*national_prefix;
    gchar			*pattern;
    gchar			*format;
	const gchar     *input_string;
	gchar           *output_string;
    GList           *leading_digits;

} PhoneBookParser;

static void
handle_start_element (GMarkupParseContext  *context,
                      const gchar          *element_name,
                      const gchar         **attribute_names,
                      const gchar         **attribute_values,
                      gpointer              user_data,
                      GError              **error)
{
	int i;
	PhoneBookParser *parser = user_data;

	if (parser->output_string)
		return;

	if (g_str_equal (element_name, "territory"))
	{
		for (i = 0; attribute_names[i] != NULL; i++)
		{
			if (g_str_equal (attribute_names[i], "id")) {
				if (!parser->id)
					parser->id = g_strdup (attribute_values[i]);
			}
			else if (g_str_equal (attribute_names[i], "countryCode"))
			{
				if (!parser->country_code)
					parser->country_code = g_strdup (attribute_values[i]);
			}
			else if (g_str_equal (attribute_names[i], "nationalPrefix"))
			{
				if (!parser->national_prefix)
					parser->national_prefix = g_strdup (attribute_values[i]);
			}
		}
	}
	else if (g_str_equal (element_name, "numberFormat"))
	{
		for (i = 0; attribute_names[i] != NULL; i++)
		{
			if (g_str_equal (attribute_names[i], "pattern"))
			{
				if (!parser->pattern) {
					parser->pattern = g_strdup (attribute_values[i]);
				}
			}
		}

		parser->leading_digits = NULL;
	}
}

static void
handle_end_element (GMarkupParseContext  *context,
                    const gchar          *element_name,
                    gpointer              user_data,
                    GError              **error)
{
	int i;
	GRegex *regex;
	GList *l = NULL;
	gchar *format = NULL, *word = NULL;
	GMatchInfo *match_info = NULL;
	PhoneBookParser *parser = user_data;

	if (g_str_equal (element_name, "territory"))
	{
		g_clear_pointer (&parser->id, g_free);
		g_clear_pointer (&parser->country_code, g_free);
		g_clear_pointer (&parser->national_prefix, g_free);
		g_clear_pointer (&parser->format, g_free);
		g_clear_pointer (&parser->pattern, g_free);
		g_list_free_full (g_steal_pointer (&parser->leading_digits), g_free);
		return;
	}

	if (parser->output_string)
		return;

	if (!g_str_equal (element_name, "numberFormat"))
		return;

	for (l = parser->leading_digits; l; l = l->next)
	{
		gchar *leading_digit = (gchar *)l->data;

		//////////////1.country code 2.leading digits ////////////////////////////
		format = g_strdup_printf ("%s(%s)\\d{5,}", parser->country_code, leading_digit);
		regex = g_regex_new (format, 0, 0, NULL);
		g_regex_match (regex, parser->input_string, 0, &match_info);

		if (!g_match_info_matches (match_info))
		{
			g_free (format);
			g_regex_unref (regex);
			g_match_info_free (match_info);
			continue;
		}

		word = g_match_info_fetch (match_info, 0);
		if (!g_str_equal (parser->input_string, word))
		{
			g_free (word);
			g_free (format);
			g_regex_unref (regex);
			g_match_info_free (match_info);
			continue;
		}
		g_free (word);
		g_free (format);
		g_regex_unref (regex);
		g_match_info_free (match_info);
		match_info = NULL;

		///////////////////////1.country code 2.pattern///////////////////////////
		format = g_strdup_printf("%s%s", parser->country_code, parser->pattern);
		regex = g_regex_new (format, 0, 0, NULL);
		g_regex_match (regex, parser->input_string, 0, &match_info);

		gchar** split = g_regex_split_simple (".+(?=\\d{1})", parser->format, 0,0);
		gint format_length = g_ascii_digit_value (split[1][0]);
		g_strfreev (split);

		GPtrArray *cmp_ptr = g_ptr_array_new ();
		GString *string = g_string_new (parser->country_code);
		for (i = 0; i < format_length; i++)
		{
			gchar *data = g_match_info_fetch (match_info, i+1);
			if(data)
			{
				g_string_append (string, g_strdup (data));
				g_ptr_array_add (cmp_ptr, g_strdup (data));
			}
			g_free (data);
		}

		if (!g_str_equal (parser->input_string, string->str))
		{
			g_string_free (string, TRUE);
			g_ptr_array_free (cmp_ptr, TRUE);
			g_regex_unref (regex);
			g_match_info_free (match_info);
			match_info = NULL;
			continue;
		}

		g_string_free (string, TRUE);
		g_regex_unref (regex);
		g_match_info_free (match_info);
		match_info = NULL;

		////////////////////last compare////////////////////////////
		string = g_string_new ("");
		regex = g_regex_new ("\\D", 0, 0, NULL);
		g_regex_match (regex, parser->format, 0, &match_info);
		g_string_append_printf (string, "+%s ", parser->country_code);

		i = 0;
		while (g_match_info_matches (match_info))
		{
			gchar *word = g_match_info_fetch (match_info, 0);

			if (g_str_equal (word,"$"))
			{
				g_string_append (string, g_strdup (cmp_ptr->pdata[i++]));
			}
			else
			{
				g_string_append (string, g_strdup (word));
			}
			g_free (word);

			g_match_info_next (match_info, NULL);
		}

		parser->output_string = g_strdup (string->str);

		g_ptr_array_free (cmp_ptr, TRUE);
		g_string_free (string, TRUE);
		g_regex_unref (regex);
		g_match_info_free (match_info);

		break;
	}

	/* reset */
	g_clear_pointer (&parser->format, g_free);
	g_clear_pointer (&parser->pattern, g_free);
	g_list_free_full (g_steal_pointer (&parser->leading_digits), g_free);
}

static void
handle_text (GMarkupParseContext *context,
                const gchar      *text,
                gsize             text_len,
                gpointer          user_data,
                GError          **error)
{
	PhoneBookParser *parser = user_data;

	if (parser->output_string)
		return;

	if (g_str_equal (g_markup_parse_context_get_element (context),"leadingDigits"))
	{
		int i,j;
		gchar tmp[text_len];
		memset (tmp, 0x00, text_len);

		j = 0;

		for (i = 0; i < text_len; i++)
		{
			if (!isspace(text[i]))
			{
				tmp[j] = text[i];
				tmp[j+1] = '\0';
				j++;
			}
		}

		parser->leading_digits = g_list_append (parser->leading_digits, g_strdup (tmp));
	}

	if (g_str_equal (g_markup_parse_context_get_element (context),"format"))
		parser->format = g_strdup (text);
}

static const GMarkupParser rule_parser =
  {
    .start_element = handle_start_element,
    .end_element = handle_end_element,
    .text = handle_text
  };

static gchar *
match_korea_phone_number (const gchar *data)
{
	GRegex *regex;
	GMatchInfo *match_info = NULL;
	gchar *ret = NULL, *word = NULL;

	regex = g_regex_new ("(02|0\\d{2})(\\d{3,4})(\\d{4})", 0, 0, NULL);
	g_regex_match (regex, data, 0, &match_info);

	if (!g_match_info_matches (match_info))
		goto out;

	word = g_match_info_fetch (match_info, 0);
	if (g_str_equal (data, word))
	{
		ret = g_strdup_printf ("%s-%s-%s",
                               g_match_info_fetch (match_info, 1),
                               g_match_info_fetch (match_info, 2), 
                               g_match_info_fetch (match_info, 3));
	}
	g_free (word);
		
out:
	g_regex_unref (regex);
	g_match_info_free (match_info);

	return ret;
}

static gboolean
read_phone_book_file (GFile         *file,
                      const gchar   *input_string,
                      gchar        **output_string,
                      GError       **error)
{
	gsize size;
	gboolean ret = FALSE;
	gchar *buffer = NULL, *match = NULL;

	if (!g_file_load_contents (file, NULL, &buffer, &size, NULL, error))
		goto out;

	match = match_korea_phone_number (input_string);
	if (match)
	{
		ret = TRUE;
		*output_string = strdup (match);
	}
	else
	{
		PhoneBookParser parser;
		GMarkupParseContext *parse_context;

		parser.id = NULL;
		parser.country_code = NULL;
		parser.national_prefix = NULL;
		parser.pattern = NULL;
		parser.format = NULL;
		parser.leading_digits = NULL;
		parser.input_string = input_string;
		parser.output_string = NULL;

		parse_context = g_markup_parse_context_new (&rule_parser,
                                                    G_MARKUP_TREAT_CDATA_AS_TEXT |
                                                    G_MARKUP_PREFIX_ERROR_POSITION,
                                                    &parser, NULL);

		if (g_markup_parse_context_parse (parse_context, buffer, size, error))
		{
			if (parser.output_string)
			{
				ret = TRUE;

				*output_string = g_strdup (parser.output_string);

				g_clear_pointer (&parser.id, g_free);
				g_clear_pointer (&parser.country_code, g_free);
				g_clear_pointer (&parser.national_prefix, g_free);
				g_clear_pointer (&parser.pattern, g_free);
				g_clear_pointer (&parser.format, g_free);
				g_clear_pointer (&parser.output_string, g_free);
				g_list_free_full (g_steal_pointer (&parser.leading_digits), g_free);
			}
		}
		g_markup_parse_context_free (parse_context);
	}

out:
	g_clear_pointer (&buffer, g_free);

	return ret;
}

gchar *
get_readable_phone_number (const gchar *input_string)
{
	GFile *phone_book;
	gint i;
	gchar *output_string = NULL;

	for (i = 0; i < strlen (input_string); i++)
	{
		if (!isdigit (input_string[i]))
			return NULL;
	}

	phone_book = g_file_new_for_path (PHONE_NUMBER_XML);

	if (g_file_test (PHONE_NUMBER_XML, G_FILE_TEST_EXISTS))
	{
		GError *error = NULL;
		if (!read_phone_book_file (phone_book, input_string, &output_string, &error))
		{
			if (error)
			{
				g_warning ("Failed to parse %s : %s", PHONE_NUMBER_XML, error->message);
				g_error_free (error);
			}
			else
				g_warning ("Failed to parse : %s", PHONE_NUMBER_XML);
		}
	}
	else
		g_warning ("No such file : %s\n", PHONE_NUMBER_XML);

	return output_string;
}
