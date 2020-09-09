/* gooroom-ars-auth.c
 *
 * Copyright 2020 donghun
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

//#include "gooroom-ars-test-config.h"

#include "config.h"
#include "ars-infos.h"
#include "gooroom-ars-auth.h"

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <stdlib.h>

#include <time.h>
#include <curl/curl.h>
#include <json-glib/json-glib.h>

//#define JSON_FILE "gooroom-ars/infos.json"
#define CONTENT_TYPE "Content-Type: application/x-www-form-urlencoded"


typedef struct {
  char *error_code_string;
  int   error_code;
  char *error_message;
} ArsError;

static ArsError ars_error_tables [] = {
  { "0001", ARS_ERROR_CODE_0001, N_("") },
  { "0002", ARS_ERROR_CODE_0002, N_("") },
  { "1001", ARS_ERROR_CODE_1001, N_("") },
  { "1002", ARS_ERROR_CODE_1002, N_("") },
  { "1102", ARS_ERROR_CODE_1102, N_("") },
  { "1103", ARS_ERROR_CODE_1103, N_("") },
  { "1104", ARS_ERROR_CODE_1104, N_("") },
  { "1105", ARS_ERROR_CODE_1105, N_("") },
  { "1106", ARS_ERROR_CODE_1106, N_("") },
  { "2201", ARS_ERROR_CODE_2201, N_("") },
  { "2202", ARS_ERROR_CODE_2202, N_("") },
  { "2203", ARS_ERROR_CODE_2203, N_("") },
  { "2204", ARS_ERROR_CODE_2204, N_("") },
  { "2205", ARS_ERROR_CODE_2205, N_("") },
  { "2206", ARS_ERROR_CODE_2206, N_("") },
  { "2501", ARS_ERROR_CODE_2501, N_("") },
  { "2502", ARS_ERROR_CODE_2502, N_("") },
  { "2503", ARS_ERROR_CODE_2503, N_("") },
  { "2504", ARS_ERROR_CODE_2504, N_("") },
  { "2505", ARS_ERROR_CODE_2505, N_("") },
  { "4002", ARS_ERROR_CODE_4002, N_("") },
  { "4003", ARS_ERROR_CODE_4003, N_("") },
  { "4011", ARS_ERROR_CODE_4011, N_("") },
  { "4012", ARS_ERROR_CODE_4012, N_("") },
  { "4013", ARS_ERROR_CODE_4013, N_("") },
  { "4014", ARS_ERROR_CODE_4014, N_("") },
  { "4015", ARS_ERROR_CODE_4015, N_("") },
  { NULL  , 0,                   NULL   }
};



GQuark
ars_error_quark (void)
{
	return g_quark_from_static_string ("ars-error-quark");
}

static ArsError *
find_ars_error (const char *error_code_string)
{
  ArsError *ars_error = NULL;

  for (ars_error = ars_error_tables; ars_error->error_code_string != NULL; ars_error++)
  {
    if (g_str_equal (ars_error->error_code_string, error_code_string))
      break;
  }

  return ars_error;
}

struct result_data
{
    char* data;
    size_t size;
};

static size_t 
write_data (void *ptr, size_t size, size_t nmemb, void *stream)
{
    size_t realsize = size * nmemb;
    struct result_data *mem = (struct result_data *)stream;

    mem->data = (char*)realloc(mem->data, mem->size + realsize + 1);
    if (mem->data)
    {
        memcpy(&(mem->data[mem->size]), ptr, realsize);
        mem->size += realsize;
        mem->data[mem->size] = '\0';
    }

    return realsize;
}

static gboolean
get_result (gchar* data, gchar** code, gchar**  res, gboolean is_auth)
{
    GError *error;
    JsonNode *json_root;
    JsonNode *json_node;
    JsonObject *json_item;
    g_autoptr(JsonParser) json_parser = NULL;

    error = NULL;
    json_parser = json_parser_new ();

    if (!json_parser_load_from_data (json_parser, data, -1, &error))
        goto error;

    json_root = json_parser_get_root (json_parser);
    if (json_root == NULL)
        goto error;

    if (json_node_get_node_type (json_root) != JSON_NODE_OBJECT)
        goto error;

    json_item = json_node_get_object (json_root);
    if (json_item == NULL)
        goto error;

    if (json_object_has_member (json_item, "resultCode"))
    {
        json_node = json_object_get_member (json_item, "resultCode");
        if (json_node != NULL)
            *code = g_strdup (json_node_get_string (json_node));
//            strcpy (code, g_strdup (json_node_get_string (json_node)));
    }

    if (json_object_has_member (json_item, "authNo"))
    {
        json_node = json_object_get_member (json_item, "authNo");
        if (json_node != NULL)
            *res = g_strdup (json_node_get_string (json_node));
//            strcpy (res, g_strdup (json_node_get_string (json_node)));
    }

    if (json_object_has_member (json_item, "arsUseTime"))
    {
        json_node = json_object_get_member (json_item, "arsUseTime");
        if (json_node != NULL)
            *res = g_strdup (json_node_get_string (json_node));
//            strcpy (res, g_strdup (json_node_get_string (json_node)));
    }

    if (is_auth)
    {
g_print ("*codeeeeee = %s\n", *code);
        if (g_strcmp0 (*code, "1000") == 0)
            return TRUE;
    }
    else
    {
        if (g_strcmp0 (*code, "0000") == 0)
            return TRUE;
    }

    return FALSE;
error :
    g_error_free (error);
    return FALSE;
}

static gchar*
get_url (gboolean is_auth, gboolean is_test)
{
    gchar *url;

    GError *error;
//    gchar *filename;
    JsonNode *json_root;
    JsonNode *json_node;
    JsonObject *json_item;
    g_autoptr(JsonParser) json_parser = NULL;

    url = NULL;
    error = NULL;
    json_parser = json_parser_new ();
//    filename = g_strdup_printf ("%s/%s", LIBDIR, JSON_FILE);

//    if (!json_parser_load_from_file (json_parser, filename, &error))
    if (!json_parser_load_from_data (json_parser, ars_infos, -1, &error))
        goto error;

    json_root = json_parser_get_root (json_parser);
    if (json_root == NULL)
        goto error;

    if (json_node_get_node_type (json_root) != JSON_NODE_OBJECT)
        goto error;

    json_item = json_node_get_object (json_root);
    if (json_item == NULL)
        goto error;

    if (json_object_has_member (json_item, "url"))
    {
        json_item = json_object_get_object_member (json_item, "url");

        if (is_auth)
        {
            if (is_test)
            {
                json_node = json_object_get_member (json_item, "authTestURL");
            }
            else
            {
                json_node = json_object_get_member (json_item, "authURL");
            }
        }
        else
        {
            if (is_test)
            {
                json_node = json_object_get_member (json_item, "confirmTestURL");
            }
            else
            {
                json_node = json_object_get_member (json_item, "confirmURL");
            }
        }

        if (json_node != NULL)
            url = g_strdup (json_node_get_string (json_node));

        return url;
    }

    return NULL;
error :
    g_error_free (error);
    return NULL;
}

static gchar*
get_post_auth_data (const gchar* tran_id, const gchar* auth_tel_no)
{
    GError *error;
//    gchar *filename;
    JsonNode *json_root;
    JsonNode *json_node;
    JsonReader *reader;
    JsonObject *json_item;
    g_autoptr(JsonParser) json_parser = NULL;

    gchar *data;
    data = malloc (sizeof(gchar) * 1024);

    error = NULL;
    json_parser = json_parser_new ();
//    filename = g_strdup_printf ("%s/%s", LIBDIR, JSON_FILE);

//    if (!json_parser_load_from_file (json_parser, filename, &error))
    if (!json_parser_load_from_data (json_parser, ars_infos, -1, &error))
        goto error;

    json_root = json_parser_get_root (json_parser);
    if (json_root == NULL)
        goto error;

    if (json_node_get_node_type (json_root) != JSON_NODE_OBJECT)
        goto error;

    json_item = json_node_get_object (json_root);
    if (json_item == NULL)
        goto error;

    json_node = json_object_get_member (json_item, "request");
    reader = json_reader_new (json_node);

    if (reader == NULL)
        goto error;

    char date[10];
    time_t now = time(NULL);
    strftime(date, 10, "%Y%m%d", localtime(&now));

    gchar* escape_date = curl_escape (date, 0);
    strcpy (data, g_strdup_printf ("%s=%s", "reqDate", escape_date));
    g_free (escape_date);

    gchar* escape_tran_id = curl_escape (tran_id, 0);
    strcat (data, g_strdup_printf ("&%s=%s", "tranId", escape_tran_id));
    g_free (escape_tran_id);

    gchar* escape_auth_tel_no = curl_escape (auth_tel_no, 0);
    strcat (data, g_strdup_printf ("&%s=%s", "authTelNo", escape_auth_tel_no));
    g_free (escape_auth_tel_no);

    gint i, cnt = 0;
    cnt = json_reader_count_members (reader);

    for (i = 0; i < cnt; i++)
    {
        json_reader_read_element (reader, i);

        const gchar* key  = json_reader_get_member_name (reader);
        const gchar* val = json_reader_get_string_value (reader);

        gchar* escape_val  = curl_escape (val, 0);
        strcat (data, g_strdup_printf ("&%s=%s", key, escape_val));
        g_free (escape_val);

        json_reader_end_element (reader);
    }

    g_print ("%s\n", data);

    return data;

error :
    g_error_free (error);
    return NULL;
}

static gchar*
get_post_confirm_data (const gchar* tran_id )
{
    GError *error;
//    gchar *filename;
    JsonNode *json_root;
    JsonNode *json_node;
    JsonReader *reader;
    JsonObject *json_item;
    g_autoptr(JsonParser) json_parser = NULL;

    gchar *data;
    data = malloc (sizeof(gchar) * 1024);

    error = NULL;
    json_parser = json_parser_new ();
//    filename = g_strdup_printf ("%s/%s", LIBDIR, JSON_FILE);

//    if (!json_parser_load_from_file (json_parser, filename, &error))
    if (!json_parser_load_from_data (json_parser, ars_infos, -1, &error))
        goto error;

    json_root = json_parser_get_root (json_parser);
    if (json_root == NULL)
        goto error;

    if (json_node_get_node_type (json_root) != JSON_NODE_OBJECT)
        goto error;

    json_item = json_node_get_object (json_root);
    if (json_item == NULL)
        goto error;

    json_node = json_object_get_member (json_item, "request");
    reader = json_reader_new (json_node);

    if (reader == NULL)
        goto error;

    char date[10];
    time_t now = time(NULL);
    strftime(date, 10, "%Y%m%d", localtime(&now));

    gchar* escape_date = curl_escape (date, 0);
    strcpy (data, g_strdup_printf ("%s=%s", "reqDate", escape_date));
    g_free (escape_date);

    gchar* escape_tran_id = curl_escape (tran_id, 0);
    strcat (data, g_strdup_printf ("&%s=%s", "tranId", escape_tran_id));
    g_free (escape_tran_id);

    gint i, cnt = 0;
    cnt = json_reader_count_members (reader);

    for (i = 0; i < cnt; i++)
    {
        json_reader_read_element (reader, i);

        const gchar* key  = json_reader_get_member_name (reader);
        const gchar* val = json_reader_get_string_value (reader);

        if ((g_strcmp0 (key, "responseResultURL") == 0) ||
            (g_strcmp0 (key, "authNoMethod") == 0) ||
            (g_strcmp0 (key, "authNo") == 0))
        {
            json_reader_end_element (reader);
            continue;
        }

        gchar* escape_val  = curl_escape (val, 0);
        strcat (data, g_strdup_printf ("&%s=%s", key, escape_val));
        g_free (escape_val);

        json_reader_end_element (reader);
    }

    g_print ("%s\n", data);

    return data;

error :
    g_error_free (error);
    return NULL;
}

gchar* 
gooroom_ars_authentication (const char  *tran_id,
                            const char  *auth_tel_no,
                            gboolean     is_test,
                            GError     **error)
{
    CURL *curl;
    CURLcode res;
    gchar *url;
    struct curl_slist *list = NULL;
    struct result_data data = {0,0};

    curl = curl_easy_init ();

    if(curl)
    {
      url = get_url (TRUE, is_test);
      curl_easy_setopt(curl, CURLOPT_URL, url);

      curl_easy_setopt (curl, CURLOPT_WRITEDATA, &data);
      curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, write_data);

      list = curl_slist_append(list, CONTENT_TYPE);
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);

      gchar *attr = get_post_auth_data (tran_id, auth_tel_no);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, attr);
      curl_easy_setopt(curl, CURLOPT_POST, 1L);

      res = curl_easy_perform(curl);
      curl_slist_free_all(list);

      /* Check for errors */
      if(res != CURLE_OK)
        g_print ("curl_easy_perform() failed: %s\n",curl_easy_strerror(res));

      /* always cleanup */
      g_free (url);
      g_free (attr);
      curl_easy_cleanup(curl);
    }

    gboolean ret;
    gchar *code= NULL;
    gchar *auth_no = NULL;
//    code = malloc (sizeof(gchar) * 128);
//    auth_no = malloc (sizeof(gchar) * 128);

    g_print ("request result data : %s\n", data.data);
    ret = get_result (data.data, &code, &auth_no, TRUE);

    if (ret) {
      g_print ("result : %s : %s\n", auth_no, code);
    } else {
      ArsError *ars_error = find_ars_error (code);
      g_set_error_literal (error,
                           ARS_ERROR,
                           ars_error->error_code,
                           ars_error->error_message);

      g_print ("error : %s ", ars_error->error_code_string);
	}

    g_free (data.data);
    g_free (code);

    return auth_no;
}

gchar* 
gooroom_ars_confirm (const char  *tran_id,
                     gboolean     is_test,
                     GError     **error)
{
    CURL *curl;
    CURLcode res;
    gchar *url;
    struct curl_slist *list = NULL;
    struct result_data data = {0,0};

    curl = curl_easy_init ();

    if(curl) 
    {
      url = get_url (FALSE, is_test);
      curl_easy_setopt(curl, CURLOPT_URL, url);

      curl_easy_setopt (curl, CURLOPT_WRITEDATA, &data);
      curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, write_data);

      list = curl_slist_append(list, CONTENT_TYPE);
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);

      gchar *attr = get_post_confirm_data (tran_id);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, attr);
      curl_easy_setopt(curl, CURLOPT_POST, 1L);
 
      res = curl_easy_perform(curl);
      curl_slist_free_all(list);
 
      /* Check for errors */
      if(res != CURLE_OK)
        g_print ("curl_easy_perform() failed: %s\n",curl_easy_strerror(res));
 
      /* always cleanup */
      g_free (url);
      g_free (attr);
      curl_easy_cleanup(curl);
    }

    gboolean ret;
    gchar *code = NULL;
    gchar *ars_time = NULL;
//    code = malloc (sizeof(gchar) * 128);
//    ars_time = malloc (sizeof(gchar) * 128);

    g_print ("data: %s\n", data.data);
    ret = get_result (data.data, &code, &ars_time, FALSE);

    if (ret)
        g_print ("result : %s : %s\n", ars_time, code);
    else
    {
      ArsError *ars_error = find_ars_error (code);
      g_set_error_literal (error,
                           ARS_ERROR,
                           ars_error->error_code,
                           ars_error->error_message);

      g_print ("error : %s\n", ars_error->error_code_string);
    }

    g_free (data.data);
    g_free (ars_time);

    return code;
}
