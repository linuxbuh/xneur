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

#ifndef _SWITCHLANG_H_
#define _SWITCHLANG_H_

#include "xneur.h"

int  get_curr_keyboard_group(void);
void set_keyboard_group(int layout_group);
void set_next_keyboard_group(struct _xneur_handle *handle);
void set_prev_keyboard_group(struct _xneur_handle *handle);

#endif /* _SWITCHLANG_H_ */
