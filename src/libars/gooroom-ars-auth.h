/**
 *
 * Copyright (C) 2020 Hancom Gooroom <gooroom@hancom.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>

#ifndef __GOOROOM_ARS_AUTH__H_
#define __GOOROOM_ARS_AUTH__H_

G_BEGIN_DECLS

/**
 * ARS_ERROR:
 */
#define ARS_ERROR (ars_error_quark())

GQuark ars_error_quark (void);

typedef enum
{
  ARS_ERROR_CODE_0001 =    1, // 인증처리중
  ARS_ERROR_CODE_0002 =    2, // 인증처리내역 없음
  ARS_ERROR_CODE_1001 = 1001, // 입력값오류
  ARS_ERROR_CODE_1002 = 1002,
  ARS_ERROR_CODE_1102 = 1102,
  ARS_ERROR_CODE_1103 = 1103,
  ARS_ERROR_CODE_1104 = 1104,
  ARS_ERROR_CODE_1105 = 1105,
  ARS_ERROR_CODE_1106 = 1106,
  ARS_ERROR_CODE_2201 = 2201,
  ARS_ERROR_CODE_2202 = 2202,
  ARS_ERROR_CODE_2203 = 2203,
  ARS_ERROR_CODE_2204 = 2204,
  ARS_ERROR_CODE_2205 = 2205,
  ARS_ERROR_CODE_2206 = 2206,
  ARS_ERROR_CODE_2501 = 2501, // DB 내부오류 (TRAN_ID 오류)
  ARS_ERROR_CODE_2502 = 2502,
  ARS_ERROR_CODE_2503 = 2503,
  ARS_ERROR_CODE_2504 = 2504,
  ARS_ERROR_CODE_2505 = 2505,
  ARS_ERROR_CODE_4002 = 4002,
  ARS_ERROR_CODE_4003 = 4003,
  ARS_ERROR_CODE_4011 = 4011,
  ARS_ERROR_CODE_4012 = 4012,
  ARS_ERROR_CODE_4013 = 4013,
  ARS_ERROR_CODE_4014 = 4014,
  ARS_ERROR_CODE_4015 = 4015
} ArsErrorCode;


gchar* gooroom_ars_confirm (const char  *tran_id,
                            gboolean     is_test,
                            GError     **error);
gchar* gooroom_ars_authentication (const char  *tran_id,
                                   const char  *auth_tel_no,
                                   gboolean     is_test,
                                   GError     **error);

G_END_DECLS

#endif
