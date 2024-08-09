/*******************************************************************************
 * libretroshare/src/file_sharing: fsitem.h                                    *
 *                                                                             *
 * libretroshare: retroshare core library                                      *
 *                                                                             *
 * Copyright 2021 by retroshare team <contact@retroshare.cc>            *
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
 ******************************************************************************/

#pragma once

#include "serialiser/rsserial.h"
#include "serialiser/rsserializer.h"
#include "retroshare/rsfriendserver.h"

#include "rsitems/rsitem.h"
#include "serialiser/rstlvbinary.h"
#include "rsitems/rsserviceids.h"
#include "rsitems/itempriorities.h"

const uint8_t RS_PKT_SUBTYPE_FS_CLIENT_PUBLISH             = 0x01 ;
const uint8_t RS_PKT_SUBTYPE_FS_CLIENT_REMOVE              = 0x02 ;
const uint8_t RS_PKT_SUBTYPE_FS_SERVER_RESPONSE            = 0x03 ;
const uint8_t RS_PKT_SUBTYPE_FS_SERVER_ENCRYPTED_RESPONSE  = 0x04 ;
const uint8_t RS_PKT_SUBTYPE_FS_SERVER_STATUS              = 0x05 ;

class RsFriendServerItem: public RsItem
{
public:
    RsFriendServerItem(uint8_t item_subtype) : RsItem(RS_PKT_VERSION_SERVICE,RS_SERVICE_TYPE_FRIEND_SERVER,item_subtype)
	{
		setPriorityLevel(QOS_PRIORITY_DEFAULT) ;
	}
    virtual ~RsFriendServerItem() {}
    virtual void clear()  override {}
};

class RsFriendServerClientPublishItem: public RsFriendServerItem
{
public:
    RsFriendServerClientPublishItem() : RsFriendServerItem(RS_PKT_SUBTYPE_FS_CLIENT_PUBLISH),n_requested_friends(0) {}

    void serial_process(RsGenericSerializer::SerializeJob j,RsGenericSerializer::SerializeContext& ctx) override
    {
        RS_SERIAL_PROCESS(n_requested_friends);
        RS_SERIAL_PROCESS(short_invite);
        RS_SERIAL_PROCESS(pgp_public_key_b64);
        RS_SERIAL_PROCESS(already_received_peers);
    }
    virtual void clear()  override
    {
        pgp_public_key_b64.clear();
        short_invite.clear();
        n_requested_friends=0;
    }

    // specific members for that item

    uint32_t           n_requested_friends;
    std::string        short_invite;
    std::string        pgp_public_key_b64;
    std::map<RsPeerId,RsFriendServer::PeerFriendshipLevel> already_received_peers;
};

class RsFriendServerStatusItem: public RsFriendServerItem
{
public:
    RsFriendServerStatusItem() : RsFriendServerItem(RS_PKT_SUBTYPE_FS_SERVER_STATUS),status(UNKNOWN) {}

    void serial_process(RsGenericSerializer::SerializeJob j,RsGenericSerializer::SerializeContext& ctx) override
    {
        RS_SERIAL_PROCESS(status);
    }

    enum ConnectionStatus: uint8_t
    {
        UNKNOWN             = 0x00,
        END_OF_TRANSMISSION = 0x01
    };

    // specific members for that item

    ConnectionStatus status;
};

class RsFriendServerClientRemoveItem: public RsFriendServerItem
{
public:
    RsFriendServerClientRemoveItem() : RsFriendServerItem(RS_PKT_SUBTYPE_FS_CLIENT_REMOVE),unique_identifier(0) {}

    void serial_process(RsGenericSerializer::SerializeJob j,RsGenericSerializer::SerializeContext& ctx)
    {
        RS_SERIAL_PROCESS(peer_id);
        RS_SERIAL_PROCESS(unique_identifier);
    }

    // Peer ID for the peer to remove.

    RsPeerId peer_id;

    // Nonce that was returned by the server after the last client request. Should match in order to proceed. This prevents
    // a malicious actor from removing peers from the server. Since the nonce is sent through Tor tunnels, it cannot be known by
    // anyone else than the client.

    uint64_t unique_identifier;
};

class RsFriendServerEncryptedServerResponseItem: public RsFriendServerItem
{
public:
    RsFriendServerEncryptedServerResponseItem() : RsFriendServerItem(RS_PKT_SUBTYPE_FS_SERVER_ENCRYPTED_RESPONSE),
                bin_data(nullptr),bin_len(0) {}

    void serial_process(RsGenericSerializer::SerializeJob j,RsGenericSerializer::SerializeContext& ctx) override
    {
        RsTypeSerializer::RawMemoryWrapper prox(bin_data, bin_len);
        RsTypeSerializer::serial_process(j, ctx, prox, "data");
    }

    virtual void clear() override
    {
        free(bin_data);
        bin_len = 0;
        bin_data = nullptr;
    }
    //

    void *bin_data;
    uint32_t bin_len;
};

class RsFriendServerServerResponseItem: public RsFriendServerItem
{
public:
    RsFriendServerServerResponseItem() : RsFriendServerItem(RS_PKT_SUBTYPE_FS_SERVER_RESPONSE), unique_identifier(0) {}

    void serial_process(RsGenericSerializer::SerializeJob j,RsGenericSerializer::SerializeContext& ctx) override
    {
        RS_SERIAL_PROCESS(unique_identifier);
        RS_SERIAL_PROCESS(friend_invites);
    }

    virtual void clear() override
    {
        friend_invites.clear();
        unique_identifier = 0;
    }
    // specific members for that item

    uint64_t unique_identifier; // This value will be used once for every client but
                                // will be re-used by the client. It acts as some kind of
                                // identifier for the server to quickly know who's talking.

    // The PeerFriendshipLevel determines what the peer has done with
    // our profile: accepted or not, or even not received at all yet.

    std::map<std::string,RsFriendServer::PeerFriendshipLevel> friend_invites;
};

struct FsSerializer : RsServiceSerializer
{
    FsSerializer(RsSerializationFlags flags = RsSerializationFlags::NONE): RsServiceSerializer(RS_SERVICE_TYPE_FRIEND_SERVER, flags) {}

    virtual RsItem *create_item(uint16_t service_id,uint8_t item_sub_id) const
    {
        if(service_id != static_cast<uint16_t>(RsServiceType::FRIEND_SERVER))
            return nullptr;

        switch(item_sub_id)
        {
        case RS_PKT_SUBTYPE_FS_CLIENT_REMOVE:             return new RsFriendServerClientRemoveItem();
        case RS_PKT_SUBTYPE_FS_CLIENT_PUBLISH:            return new RsFriendServerClientPublishItem();
        case RS_PKT_SUBTYPE_FS_SERVER_RESPONSE:           return new RsFriendServerServerResponseItem();
        case RS_PKT_SUBTYPE_FS_SERVER_STATUS:             return new RsFriendServerStatusItem();
        case RS_PKT_SUBTYPE_FS_SERVER_ENCRYPTED_RESPONSE: return new RsFriendServerEncryptedServerResponseItem();
        default:
            RsErr() << "Unknown subitem type " << item_sub_id << " in FsSerialiser" ;
            return nullptr;
        }

    }
};
