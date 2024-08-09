/*
 * RetroShare JSON API
 *
 * Copyright (C) 2018-2019  Gioacchino Mazzurco <gio@eigenlab.org>
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 * See the GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>
 *
 * SPDX-FileCopyrightText: 2004-2019 RetroShare Team <contact@retroshare.cc>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <string>
#include "util/rstime.h"
#include <cstdint>
#include <set>

#include "rsitems/rsitem.h"
#include "rsitems/rsserviceids.h"
#include "serialiser/rsserializer.h"
#include "serialiser/rsserializable.h"

enum class JsonApiItemsType : uint8_t {
    AuthTokenItem_deprecated = 0,
    AuthTokenItem            = 1,
    ConfigItem               = 2,
};

struct JsonApiServerAuthTokenStorage : RsItem
{
	JsonApiServerAuthTokenStorage() :
	    RsItem( RS_PKT_VERSION_SERVICE, RS_SERVICE_TYPE_JSONAPI,
	            static_cast<uint8_t>(JsonApiItemsType::AuthTokenItem) ) {}

	/// @see RsSerializable
	virtual void serial_process(RsGenericSerializer::SerializeJob j,
	                            RsGenericSerializer::SerializeContext& ctx)
	{
		RS_SERIAL_PROCESS(mAuthorizedTokens);
	}

	/// @see RsItem
	virtual void clear() { mAuthorizedTokens.clear(); }

	std::map<std::string,std::string> mAuthorizedTokens;
};

struct JsonApiServerConfigItem : RsItem
{
    JsonApiServerConfigItem() : RsItem( RS_PKT_VERSION_SERVICE, RS_SERVICE_TYPE_JSONAPI,
                static_cast<uint8_t>(JsonApiItemsType::ConfigItem) ) {}

    /// @see RsSerializable
    virtual void serial_process(RsGenericSerializer::SerializeJob j,
                                RsGenericSerializer::SerializeContext& ctx)
    {
        RS_SERIAL_PROCESS(mListeningPort);
        RS_SERIAL_PROCESS(mBindingAddress);
    }

    /// @see RsItem
    virtual void clear() {}

    uint16_t mListeningPort;
    std::string mBindingAddress;
};



struct JsonApiConfigSerializer : RsServiceSerializer
{
	JsonApiConfigSerializer() : RsServiceSerializer(RS_SERVICE_TYPE_JSONAPI) {}
	virtual ~JsonApiConfigSerializer() {}

	RsItem* create_item(uint16_t service_id, uint8_t item_sub_id) const
	{
		if(service_id != RS_SERVICE_TYPE_JSONAPI) return nullptr;

		switch(static_cast<JsonApiItemsType>(item_sub_id))
		{
		case JsonApiItemsType::AuthTokenItem: return new JsonApiServerAuthTokenStorage();
        case JsonApiItemsType::ConfigItem: return new JsonApiServerConfigItem();
        default: return nullptr;
		}
	}
};


