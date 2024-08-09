/*******************************************************************************
 * libretroshare/src/util: extaddrfinder.cc                                    *
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

//#define EXTADDRSEARCH_DEBUG

#include "extaddrfinder.h"

#include "pqi/pqinetwork.h"
#include "rsdebug.h"
#include "util/rsstring.h"
#include "util/rsmemory.h"

#ifndef WIN32
#include <netdb.h>
#endif

#include <string.h>
#include <string>
#include <iostream>
#include <set>
#include <vector>
#include <algorithm>
#include <stdio.h>
#include "util/rstime.h"

#include <map>

const uint32_t MAX_IP_STORE = 300; /* seconds ip address timeout */

class ZeroInt
{
public:
	ZeroInt() : n(0) {}
	uint32_t n ;
};

inline bool isIn0200Prefix(const struct in6_addr* addr) {
    const UCHAR* bytes = addr->s6_addr;
    return ((bytes[0] & 0xFE) == 0x02);
}

void ExtAddrFinder::run()
{
	std::vector<std::string> res ;
	std::vector<sockaddr_storage> addrs ;

	mFoundV4 = false;
	mFoundV6 = false;

	getLocalAddresses(addrs);
	for (auto addr : addrs) {
		if (addr.ss_family == AF_INET6) {
			const struct sockaddr_in6 *ptr1 = (const struct sockaddr_in6 *) &addr;
			if (isIn0200Prefix(&(ptr1->sin6_addr))) {
				sockaddr_storage_clear(mAddrV6);
				mFoundV6 = true;
				mSearching = false ;
				mFoundTS = time(NULL) ;
				mAddrV6 = addr;
				return ;
			}
		}
	}
	reset();
	return ;
}

void ExtAddrFinder::start_request()
{
	if (!isRunning())
		start("ExtAddrFinder");
}

bool ExtAddrFinder::hasValidIPV4(struct sockaddr_storage &addr)
{
#ifdef EXTADDRSEARCH_DEBUG
	RS_DBG("Getting ip.");
#endif

	{
		RS_STACK_MUTEX(mAddrMtx) ;
		if(mFoundV4)
		{
#ifdef EXTADDRSEARCH_DEBUG
			RS_DBG("Has stored ip responding with this ip:", sockaddr_storage_iptostring(mAddrV4)) ;
#endif
			sockaddr_storage_copyip(addr,mAddrV4);	// just copy the IP so we dont erase the port.
		}
	}

	testTimeOut();

	RS_STACK_MUTEX(mAddrMtx) ;
	return mFoundV4;
}

bool ExtAddrFinder::hasValidIPV6(struct sockaddr_storage &addr)
{
#ifdef EXTADDRSEARCH_DEBUG
	RS_DBG("Getting ip.");
#endif

	{
		RS_STACK_MUTEX(mAddrMtx) ;
		if(mFoundV6)
		{
#ifdef EXTADDRSEARCH_DEBUG
			RS_DBG("Has stored ip responding with this ip:", sockaddr_storage_iptostring(mAddrV6)) ;
#endif
			sockaddr_storage_copyip(addr,mAddrV6);	// just copy the IP so we dont erase the port.
		}
	}

	testTimeOut();

	RS_STACK_MUTEX(mAddrMtx) ;
	return mFoundV6;
}

void ExtAddrFinder::testTimeOut()
{
	bool timeOut;
	{
		RS_STACK_MUTEX(mAddrMtx) ;
		//timeout the current ip
		timeOut = (mFoundTS + MAX_IP_STORE < time(NULL));
	}
	if(timeOut || mFirstTime) {//launch a research
		if( mAddrMtx.trylock())
		{
			if(!mSearching)
			{
#ifdef EXTADDRSEARCH_DEBUG
				RS_DBG("No stored ip: Initiating new search.");
#endif
				mSearching = true ;
				start_request() ;
			}
#ifdef EXTADDRSEARCH_DEBUG
			else
				RS_DBG("Already searching.");
#endif
			mFirstTime = false;
			mAddrMtx.unlock();
		}
#ifdef EXTADDRSEARCH_DEBUG
		else
			RS_DBG("(Note) Could not acquire lock. Busy.");
#endif
	}
}

void ExtAddrFinder::reset(bool firstTime /*=false*/)
{
#ifdef EXTADDRSEARCH_DEBUG
	RS_DBG("firstTime=", firstTime);
#endif
	RS_STACK_MUTEX(mAddrMtx) ;

	mSearching = false ;
	mFoundV4 = false ;
	mFoundV6 = false ;
	mFirstTime = firstTime;
	mFoundTS = time(nullptr);
	sockaddr_storage_clear(mAddrV4);
	sockaddr_storage_clear(mAddrV6);
}

ExtAddrFinder::~ExtAddrFinder()
{
#ifdef EXTADDRSEARCH_DEBUG
	RS_DBG("Deleting ExtAddrFinder.");
#endif
}

ExtAddrFinder::ExtAddrFinder() : mAddrMtx("ExtAddrFinder")
{
#ifdef EXTADDRSEARCH_DEBUG
	RS_DBG("Creating new ExtAddrFinder.");
#endif
	reset( true );

//https://unix.stackexchange.com/questions/22615/how-can-i-get-my-external-ip-address-in-a-shell-script
	//Enter direct ip so local DNS cannot change it.
	//DNS servers must recognize "myip.opendns.com"
	_ip_servers.push_back(std::string( "208.67.222.222" )) ;//resolver1.opendns.com
	_ip_servers.push_back(std::string( "208.67.220.220" )) ;//resolver2.opendns.com
	_ip_servers.push_back(std::string( "208.67.222.220" )) ;//resolver3.opendns.com
	_ip_servers.push_back(std::string( "208.67.220.222" )) ;//resolver4.opendns.com
	_ip_servers.push_back(std::string( "2620:119:35::35" )) ;//resolver1.opendns.com
	_ip_servers.push_back(std::string( "2620:119:53::53" )) ;//resolver2.opendns.com
}
