/*******************************************************************************
 * libretroshare/src/util: rsfile.h                                            *
 *                                                                             *
 * libretroshare: retroshare core library                                      *
 *                                                                             *
 * Copyright (C) 2021 Retroshare Team <contact@retroshare.cc>           *
 *                                                                             *
 * This program is free software: you can redistribute it and/or modify        *
 * it under the terms of the GNU Lesser General Public License as              *
 * published by the Free Software Foundation, either version 3 of the          *
 * License, or (at your option) any later version.                             *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                *
 * GNU Lesser General Public License for more details.                         *
 *                                                                             *
 * You should have received a copy of the GNU Lesser General Public License    *
 * along with this program. If not, see <https://www.gnu.org/licenses/>.       *
 *                                                                             *
 *******************************************************************************/

#pragma once

#include <stdio.h>

#ifndef WINDOWS_SYS
#include <sys/types.h>
#include <sys/socket.h>
#endif

namespace RsFileUtil {

int set_fd_nonblock(int fd);
ssize_t rs_getline(char **lineptr, size_t *n, FILE *stream);

}
