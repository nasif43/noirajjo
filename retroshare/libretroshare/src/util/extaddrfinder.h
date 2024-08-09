/*******************************************************************************
 * libretroshare/src/util: extaddrfinder.h                                     *
 *                                                                             *
 * libretroshare: retroshare core library                                      *
 *                                                                             *
 * Copyright (C) 2017 Retroshare Team <contact@retroshare.cc>           *
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

#include <list>
#include <string>

#include "util/rsthreads.h"
#include "util/rsnet.h"
#include "util/rstime.h"

struct sockaddr ;

class ExtAddrFinder: public RsThread
{
	public:
		ExtAddrFinder() ;
		~ExtAddrFinder() ;

		bool hasValidIPV4(struct sockaddr_storage &addr) ;
		bool hasValidIPV6(struct sockaddr_storage &addr) ;
		void getIPServersList(std::list<std::string>& ip_servers) { ip_servers = _ip_servers ; }

		void start_request() ;

		void reset(bool firstTime = false) ;

	private:
		virtual void run();
		void testTimeOut();

		RsMutex mAddrMtx;
		bool mSearching;
		bool mFoundV4;
		bool mFoundV6;
		bool mFirstTime;
		rstime_t mFoundTS;
		struct sockaddr_storage mAddrV4;
		struct sockaddr_storage mAddrV6;
		std::list<std::string> _ip_servers;
};
