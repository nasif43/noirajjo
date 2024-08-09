/*******************************************************************************
 * libretroshare/src/file_sharing: fsbio.cc                                    *
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

#include "util/rsprint.h"
#include "util/rsfile.h"
#include "pqi/pqifdbin.h"

//#define DEBUG_FS_BIN

#ifdef DEBUG_FS_BIN
#include "util/rsprint.h"
#endif

RsFdBinInterface::RsFdBinInterface(int file_descriptor, bool is_socket)
    : mCLintConnt(file_descriptor),mIsSocket(is_socket),mIsActive(false)
{
    mTotalReadBytes=0;
    mTotalInBufferBytes=0;
    mTotalWrittenBytes=0;
    mTotalOutBufferBytes=0;

    if(file_descriptor!=0)
        setSocket(file_descriptor);
}

void RsFdBinInterface::setSocket(int s)
{
    if(mIsActive != 0)
    {
        RsErr() << "Changing socket to active FsBioInterface! Canceling all pending R/W data." ;
        close();
    }
#ifndef WINDOWS_SYS
    int flags = fcntl(s,F_GETFL);

    if(!(flags & O_NONBLOCK))
    {
        RsWarn() << "Trying to use a blocking file descriptor in RsFdBinInterface. This is not going to work! Setting the socket to be non blocking.";
        unix_fcntl_nonblock(s);
    }

#else
    // On windows, there is no way to determine whether a socket is blocking or not, so we set it to non blocking whatsoever.
    if (mIsSocket) {
        unix_fcntl_nonblock(s);
    } else {
        RsFileUtil::set_fd_nonblock(s);
    }
#endif

    mCLintConnt = s;
    mIsActive = (s!=0);
}
int RsFdBinInterface::tick()
{
    if(!mIsActive)
    {
        static rstime_t last(0);

        rstime_t now = time(nullptr);
        if(now > last + 10)
        {
            last = now;
            RsErr() << "Ticking a non active FsBioInterface!" ;
        }

        return 0;
    }
    // 2 - read incoming data pending on existing connections

    int res=0;

    res += read_pending();
    res += write_pending();

    return res;
}

int RsFdBinInterface::read_pending()
{
    char inBuffer[1025];
    memset(inBuffer,0,1025);

    ssize_t readbytes;
#if WINDOWS_SYS
    if (mIsSocket)
        // Windows needs recv for sockets
        readbytes = recv(mCLintConnt, inBuffer, sizeof(inBuffer), 0);
    else
#endif
    readbytes = read(mCLintConnt, inBuffer, sizeof(inBuffer));	// Needs read instead of recv which is only for sockets.
                                                                // Sockets should be set to non blocking by the client process.
//#endif WINDOWS_SYS

    if(readbytes == 0)
    {
        RsDbg() << "Reached END of the stream!" ;
        RsDbg() << "Closing socket! mTotalInBufferBytes = " << mTotalInBufferBytes ;

        close();
        return mTotalInBufferBytes;
    }
    if(readbytes < 0)
    {
#ifdef WINDOWS_SYS
        int lastError = WSAGetLastError();
        if (lastError == WSAEWOULDBLOCK) {
            errno = EWOULDBLOCK;
        }
#endif
        if(errno != 0 && errno != EWOULDBLOCK && errno != EAGAIN)
#ifdef WINDOWS_SYS
            // A non blocking read to file descriptor gets ERROR_NO_DATA for empty data
            if (mIsSocket == true || WSAGetLastError() != ERROR_NO_DATA)
#endif
            RsErr() << "read() failed. Errno=" << errno ;

        return mTotalInBufferBytes;
    }

#ifdef DEBUG_FS_BIN
    RsDbg() << "clintConnt: " << mCLintConnt << ", readbytes: " << readbytes ;
#endif

    // display some debug info

    if(readbytes > 0)
    {
#ifdef DEBUG_FS_BIN
        RsDbg() << "Received the following bytes: size=" << readbytes << " data=" << RsUtil::BinToHex( reinterpret_cast<unsigned char*>(inBuffer),readbytes,50) << std::endl;
        //RsDbg() << "Received the following bytes: size=" << readbytes << " len=" << std::string(inBuffer,readbytes) << std::endl;
#endif

        void *ptr = malloc(readbytes);

        if(!ptr)
            throw std::runtime_error("Cannot allocate memory! Go buy some RAM!");

        memcpy(ptr,inBuffer,readbytes);

        in_buffer.push_back(std::make_pair(ptr,readbytes));
        mTotalInBufferBytes += readbytes;
        mTotalReadBytes += readbytes;

#ifdef DEBUG_FS_BIN
        RsDbg() << "Socket: " << mCLintConnt << ". Total read: " << mTotalReadBytes << ". Buffer size: " << mTotalInBufferBytes ;
#endif
    }
#ifdef DEBUG_FS_BIN
    RsDbg() << "End of read_pending: mTotalInBufferBytes = " << mTotalInBufferBytes;
#endif
    return mTotalInBufferBytes;
}

int RsFdBinInterface::write_pending()
{
    if(out_buffer.empty())
        return mTotalOutBufferBytes;

    auto& p = out_buffer.front();
#ifdef DEBUG_FS_BIN
    std::cerr << "RsFdBinInterface -- SENDING --- len=" << p.second << " data=" << RsUtil::BinToHex((uint8_t*)p.first,p.second)<< std::endl;
#endif

    int written;
#if WINDOWS_SYS
    if (mIsSocket)
        // Windows needs send for sockets
        written = send(mCLintConnt, (char*) p.first, p.second, 0);
    else
#endif
    written = write(mCLintConnt, p.first, p.second);
//#endif WINDOWS_SYS

    if(written < 0)
    {
#ifdef WINDOWS_SYS
        int lastError = WSAGetLastError();
        if (lastError == WSAEWOULDBLOCK) {
            errno = EWOULDBLOCK;
        }
#endif
        if(errno != EWOULDBLOCK && errno != EAGAIN)
            RsErr() << "write() failed. Errno=" << errno ;

        return mTotalOutBufferBytes;
    }

    if(written == 0)
    {
        RsErr() << "write() failed. Nothing sent.";
        return mTotalOutBufferBytes;
    }

#ifdef DEBUG_FS_BIN
    RsDbg() << "clintConnt: " << mCLintConnt << ", written: " << written ;
#endif

    // display some debug info

#ifdef DEBUG_FS_BIN
    RsDbg() << "Sent the following bytes: " << RsUtil::BinToHex( reinterpret_cast<unsigned char*>(p.first),written,50) << std::endl;
#endif

    if(written < p.second)
    {
        void *ptr = malloc(p.second - written);

        if(!ptr)
            throw std::runtime_error("Cannot allocate memory! Go buy some RAM!");

        memcpy(ptr,static_cast<unsigned char *>(p.first) + written,p.second - written);
        free(p.first);

        out_buffer.front().first = ptr;
        out_buffer.front().second = p.second - written;
    }
    else
    {
        free(p.first);
        out_buffer.pop_front();
    }

    mTotalOutBufferBytes -= written;
    mTotalWrittenBytes += written;

    return mTotalOutBufferBytes;
}

RsFdBinInterface::~RsFdBinInterface()
{
    close();
    clean();
}

void RsFdBinInterface::clean()
{
    for(auto p:in_buffer) free(p.first);
    for(auto p:out_buffer) free(p.first);

    in_buffer.clear();
    out_buffer.clear();
}

int RsFdBinInterface::readline(void *data, int len)
{
    int n=0;

    for(auto p:in_buffer)
        for(int i=0;i<p.second;++i,++n)
            if((n+1==len) || static_cast<unsigned char*>(p.first)[i] == '\n')
                return readdata(data,n+1);

    return 0;
}

int RsFdBinInterface::readdata(void *data, int len)
{
    // Expected behavior of BinInterface: when the full amount of bytes (len bytes) can not be provided, we keep the data.

    if((int)mTotalInBufferBytes < len)
    {
        std::cerr << "RsFdBinInterface -- READ --- not enough data to fill " << len << " bytes. Current buffer is " << mTotalInBufferBytes << " bytes." << std::endl;
        return mTotalInBufferBytes;
    }

    // read incoming bytes in the buffer

    int total_len = 0;

    while(total_len < len)
    {
        // If the remaining buffer is too large, chop of the beginning of it.

        if(total_len + in_buffer.front().second >= len)
        {
            int bytes_in  = len - total_len;
            int bytes_out = in_buffer.front().second - bytes_in;

            memcpy(&(static_cast<unsigned char *>(data)[total_len]),in_buffer.front().first,bytes_in);

            auto tmp_ptr = in_buffer.front().first;

            if(bytes_out > 0)
            {
                void *ptr = malloc(bytes_out);
                memcpy(ptr,&(static_cast<unsigned char*>(in_buffer.front().first)[bytes_in]),bytes_out);
                in_buffer.front().first = ptr;
            }

            free(tmp_ptr);
            in_buffer.front().second -= bytes_in;

            if(in_buffer.front().second == 0)
                in_buffer.pop_front();

            mTotalInBufferBytes -= len;
#ifdef DEBUG_FS_BIN
            std::cerr << "RsFdBinInterface -- READ --- len=" << len << " data=" << RsUtil::BinToHex((uint8_t*)data,len)<< std::endl;
#endif
            return len;
        }
        else // copy everything
        {
            memcpy(&(static_cast<unsigned char *>(data)[total_len]),in_buffer.front().first,in_buffer.front().second);

            total_len += in_buffer.front().second;

            free(in_buffer.front().first);
            in_buffer.pop_front();
        }
    }
#ifdef DEBUG_FS_BIN
    std::cerr << "RsFdBinInterface: ERROR. Shouldn't be here!" << std::endl;
#endif
    return 0;
}

int RsFdBinInterface::senddata(void *data, int len)
{
    // shouldn't we better send in multiple packets, similarly to how we read?

#ifdef DEBUG_FS_BIN
    std::cerr << "RsFdBinInterface -- QUEUEING OUT --- len=" << len << " data=" << RsUtil::BinToHex((uint8_t*)data,len)<< std::endl;
#endif
    if(len == 0)
    {
        RsErr() << "Calling FsBioInterface::senddata() with null size or null data pointer";
        return 0;
    }
    void *ptr = malloc(len);

    if(!ptr)
    {
        RsErr() << "Cannot allocate data of size " << len ;
        return 0;
    }

    memcpy(ptr,data,len);
    out_buffer.push_back(std::make_pair(ptr,len));

    mTotalOutBufferBytes += len;
    return len;
}
int RsFdBinInterface::netstatus()
{
    return mIsActive; // dummy response.
}

int RsFdBinInterface::isactive()
{
    return mIsActive || mTotalInBufferBytes>0;
}

bool RsFdBinInterface::moretoread(uint32_t /* usec */)
{
    return mTotalInBufferBytes > 0;
}

bool RsFdBinInterface::moretowrite(uint32_t /* usec */)
{
    return mTotalOutBufferBytes > 0 ;
}

bool RsFdBinInterface::cansend(uint32_t)
{
    return isactive();
}

int RsFdBinInterface::close()
{
    RsDbg() << "Stopping network interface" << std::endl;
    if(moretoread(0) || moretowrite(0))
        RsWarn() << "Interface still has " << mTotalInBufferBytes << " / " << mTotalOutBufferBytes << "bytes in/out buffers" << std::endl;

    mIsActive = false;
    mCLintConnt = 0;

    return 1;
}


