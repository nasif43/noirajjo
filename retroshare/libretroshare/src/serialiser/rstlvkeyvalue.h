/*******************************************************************************
 * libretroshare/src/serialiser: rstlvkeyvalue.h                               *
 *                                                                             *
 * libretroshare: retroshare core library                                      *
 *                                                                             *
 * Copyright 2008 by Robert Fernie,Chris Parker <retroshare@lunamutt.com>      *
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

/*******************************************************************
 * These are the Compound TLV structures that must be (un)packed.
 *
 ******************************************************************/

#include "serialiser/rstlvitem.h"
#include <list>

class RsTlvKeyValue: public RsTlvItem
{
	public:
	 RsTlvKeyValue() { return; }
	 RsTlvKeyValue(const std::string& k,const std::string& v): key(k),value(v) {}
virtual ~RsTlvKeyValue() { return; }
virtual uint32_t TlvSize() const;
virtual void	 TlvClear();
virtual bool     SetTlv(void *data, uint32_t size, uint32_t *offset) const; 
virtual bool     GetTlv(void *data, uint32_t size, uint32_t *offset); 
virtual std::ostream &print(std::ostream &out, uint16_t indent) const;

	std::string key;	/// Mandatory : For use in hash tables
	std::string value;	/// Mandatory : For use in hash tables
};

class RsTlvKeyValueSet: public RsTlvItem
{
	public:
	 RsTlvKeyValueSet() { return; }
virtual ~RsTlvKeyValueSet() { return; }
virtual uint32_t TlvSize() const;
virtual void	 TlvClear();
virtual bool     SetTlv(void *data, uint32_t size, uint32_t *offset) const; 
virtual bool     GetTlv(void *data, uint32_t size, uint32_t *offset);
virtual std::ostream &print(std::ostream &out, uint16_t indent) const;

	std::list<RsTlvKeyValue> pairs; /// For use in hash tables 
};


