/*******************************************************************************
 * libretroshare/src/util: rsstd.h                                             *
 *                                                                             *
 * libretroshare: retroshare core library                                      *
 *                                                                             *
 *  Copyright (C) 2015 Retroshare Team <contact@retroshare.cc>          *
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
#ifndef RSSTD_H_
#define RSSTD_H_

namespace rsstd {

template<typename _IIter>
void delete_all(_IIter iter_begin, _IIter iter_end)
{
	for (_IIter it = iter_begin; it != iter_end; ++it) {
		delete(*it);
	}
}

}

#endif // RSSTD_H_
