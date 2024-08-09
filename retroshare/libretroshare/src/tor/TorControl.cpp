/* Ricochet - https://ricochet.im/
 * Copyright (C) 2014, John Brooks <john.brooks@dereferenced.net>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *
 *    * Neither the names of the copyright owners nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <time.h>
#include <fstream>
#include "util/rsdir.h"

#include "retroshare/rstor.h"
#include "TorControl.h"
#include "TorControlSocket.h"
#include "HiddenService.h"
#include "ProtocolInfoCommand.h"
#include "AuthenticateCommand.h"
#include "SetConfCommand.h"
#include "GetConfCommand.h"
#include "AddOnionCommand.h"
#include "StrUtil.h"
#include "PendingOperation.h"

class nullstream: public std::ostream {};

using namespace Tor;

TorControl::TorControl()
    : mControlPort(0),mSocksPort(0),mStatus(NotConnected), mTorStatus(TorUnknown),mHasOwnership(false)
{
    mSocket = new TorControlSocket(this);
}

TorControl::~TorControl()
{
    delete(mSocket);
}

static RsTorConnectivityStatus torConnectivityStatus(Tor::TorControl::Status t)
{
    switch(t)
    {
    default:
    case TorControl::Error:              return RsTorConnectivityStatus::ERROR;
    case TorControl::NotConnected:       return RsTorConnectivityStatus::NOT_CONNECTED;
    case TorControl::Connecting:         return RsTorConnectivityStatus::CONNECTING;
    case TorControl::SocketConnected:    return RsTorConnectivityStatus::SOCKET_CONNECTED;
    case TorControl::Authenticating:     return RsTorConnectivityStatus::AUTHENTICATING;
    case TorControl::Authenticated:      return RsTorConnectivityStatus::AUTHENTICATED;
    case TorControl::HiddenServiceReady: return RsTorConnectivityStatus::HIDDEN_SERVICE_READY;
    case TorControl::Unknown:            return RsTorConnectivityStatus::UNKNOWN;
    }
}
static RsTorStatus torStatus(Tor::TorControl::TorStatus t)
{
    switch(t)
    {
    default:
    case TorControl::TorUnknown:   return RsTorStatus::UNKNOWN;
    case TorControl::TorOffline:   return RsTorStatus::OFFLINE;
    case TorControl::TorReady:     return RsTorStatus::READY;
    }
}

void TorControl::setStatus(TorControl::Status n)
{
    if (n == mStatus)
        return;

    TorControl::Status old = mStatus;
    mStatus = n;

    if (old == TorControl::Error)
        mErrorMessage.clear();

    if(rsEvents)
    {
        auto ev = std::make_shared<RsTorManagerEvent>();

        ev->mTorManagerEventType    = RsTorManagerEventCode::TOR_STATUS_CHANGED;
        ev->mTorStatus = ::torStatus(mTorStatus);
        ev->mTorConnectivityStatus  = torConnectivityStatus(mStatus);

        rsEvents->sendEvent(ev);
    }
    mStatusChanged_callback(mStatus, old);
}

void TorControl::setTorStatus(TorControl::TorStatus n)
{
    if (n == mTorStatus)
        return;

    RsDbg() << "Setting TorStatus=" << n ;
    mTorStatus = n;

    if(rsEvents)
    {
        auto ev = std::make_shared<RsTorManagerEvent>();

        ev->mTorManagerEventType = RsTorManagerEventCode::TOR_STATUS_CHANGED;
        ev->mTorStatus = ::torStatus(mTorStatus);
        ev->mTorConnectivityStatus  = torConnectivityStatus(mStatus);

        rsEvents->sendEvent(ev);
    }
}

void TorControl::setError(const std::string &message)
{
    mErrorMessage = message;
    setStatus(TorControl::Error);

    RsWarn() << "torctrl: Error:" << mErrorMessage;
}

TorControl::Status TorControl::status() const
{
    return mStatus;
}

TorControl::TorStatus TorControl::torStatus() const
{
    return mTorStatus;
}

std::string TorControl::torVersion() const
{
    return mTorVersion;
}

std::string TorControl::errorMessage() const
{
    return mErrorMessage;
}

bool TorControl::hasConnectivity() const
{
    return torStatus() == TorReady && !mSocksAddress.empty();
}

std::string TorControl::socksAddress() const
{
    return mSocksAddress;
}

uint16_t TorControl::socksPort() const
{
    return mSocksPort;
}

std::list<HiddenService*> TorControl::hiddenServices() const
{
    return mServices;
}

std::map<std::string,std::string> TorControl::bootstrapStatus() const
{
    return mBootstrapStatus;
}

void TorControl::setAuthPassword(const ByteArray &password)
{
    mAuthPassword = password;
}

void TorControl::connect(const std::string &address, uint16_t port)
{
    if (status() > Connecting)
    {
        RsDbg() << "Ignoring TorControl::connect due to existing connection" ;
        return;
    }

    mTorAddress = address;
    mControlPort = port;
    setTorStatus(TorUnknown);

    if(mSocket->isRunning())
        mSocket->fullstop();

    setStatus(Connecting);

    if(mSocket->connectToHost(address, port))
    {
        setStatus(SocketConnected);
        setTorStatus(TorOffline);	// connected and running, but not yet ready
    }
}

void TorControl::reconnect()
{
    assert(!mTorAddress.empty() && mControlPort);

    if (mTorAddress.empty() || !mControlPort || status() >= Connecting)
        return;

    setStatus(Connecting);
    mSocket->connectToHost(mTorAddress, mControlPort);
}

void TorControl::authenticateReply(TorControlCommand *sender)
{
    AuthenticateCommand *command = dynamic_cast<AuthenticateCommand*>(sender);
    assert(command);
    assert(mStatus == TorControl::Authenticating);
    if (!command)
        return;

    if (!command->isSuccessful()) {
        setError(command->errorMessage());
        return;
    }

    RsDbg() << "  Authentication successful" ;
    setStatus(TorControl::Authenticated);

    TorControlCommand *clientEvents = new TorControlCommand;
    clientEvents->set_replyLine_callback([this](int code, const ByteArray &data) { statusEvent(code,data);});

    mSocket->registerEvent(ByteArray("STATUS_CLIENT"), clientEvents);

    getTorInfo();
    publishServices();

    // XXX Fix old configurations that would store unwanted options in torrc.
    // This can be removed some suitable amount of time after 1.0.4.
    if (mHasOwnership)
        saveConfiguration();
}


void TorControl::authenticate()
{
    assert(mStatus == TorControl::SocketConnected);

    setStatus(TorControl::Authenticating);
    RsInfo() << "  Connected socket; querying information for authentication" ;

    ProtocolInfoCommand *command = new ProtocolInfoCommand(this);

    command->set_finished_callback( [this](TorControlCommand *sender) { protocolInfoReply(sender); });
    command->set_replyLine_callback([this](int code, const ByteArray &data) { statusEvent(code,data); });

    mSocket->sendCommand(command, command->build());
}

void TorControl::socketDisconnected()
{
    /* Clear some internal state */
    mTorVersion.clear();
    mSocksAddress.clear();
    mSocksPort = 0;
    setTorStatus(TorControl::TorUnknown);

    /* This emits the disconnected() signal as well */
    setStatus(TorControl::NotConnected);
}

void TorControl::socketError(const std::string& s)
{
    setError("Connection failed: " + s);
}

void TorControl::protocolInfoReply(TorControlCommand *sender)
{
    ProtocolInfoCommand *info = dynamic_cast<ProtocolInfoCommand*>(sender);
    if (!info)
        return;

    mTorVersion = info->torVersion();

    if (mStatus == TorControl::Authenticating)
    {
        AuthenticateCommand *auth = new AuthenticateCommand;

        auth->set_finished_callback( [this](TorControlCommand *sender) { authenticateReply(sender); });

        ByteArray data;
        ProtocolInfoCommand::AuthMethod methods = info->authMethods();

        if(methods & ProtocolInfoCommand::AuthNull)
        {
            RsInfo() << "  Using null authentication" ;
            data = auth->build();
        }
        else if ((methods & ProtocolInfoCommand::AuthCookie) && !info->cookieFile().empty())
        {
            std::string cookieFile = info->cookieFile();
            std::string cookieError;
            RsInfo() << "  Using cookie authentication with file" << cookieFile ;

            FILE *f = fopen(cookieFile.c_str(),"r");

            if(f)
            {
                std::string cookie;
                signed char c;	// in some systems (android), char is not signed, and here EOF=-1.
                while((c=getc(f))!=EOF)
                    cookie += c;
                fclose(f);

                /* Simple test to avoid a vulnerability where any process listening on what we think is
                 * the control port could trick us into sending the contents of an arbitrary file */
                if (cookie.size() == 32)
                    data = auth->build(cookie);
                else
                    cookieError = "Unexpected file size";
            }
            else
                cookieError = "Cannot open file " + cookieFile + ". errno=" + RsUtil::NumberToString(errno);

            if (!cookieError.empty() || data.isNull())
            {
                /* If we know a password and password authentication is allowed, try using that instead.
                 * This is a strange corner case that will likely never happen in a normal configuration,
                 * but it has happened. */
                if ((methods & ProtocolInfoCommand::AuthHashedPassword) && !mAuthPassword.empty())
                {
                    RsWarn() << "  Unable to read authentication cookie file:" << cookieError ;
                    goto usePasswordAuth;
                }

                setError("Unable to read authentication cookie file: " + cookieError);
                delete auth;
                return;
            }
        }
        else if ((methods & ProtocolInfoCommand::AuthHashedPassword) && !mAuthPassword.empty())
        {
            usePasswordAuth:
            RsInfo() << "  Using hashed password authentication" ;
            data = auth->build(mAuthPassword);
        }
        else
        {
            if (methods & ProtocolInfoCommand::AuthHashedPassword)
                setError("Tor requires a control password to connect, but no password is configured.");
            else
                setError("Tor is not configured to accept any supported authentication methods.");
            delete auth;
            return;
        }

        mSocket->sendCommand(auth, data);
    }
}

void TorControl::getTorInfo()
{
    assert(isConnected());

    GetConfCommand *command = new GetConfCommand(GetConfCommand::GetInfo);
    //connect(command, &TorControlCommand::finished, this, &TorControl::getTorInfoReply);
    command->set_finished_callback( [this](TorControlCommand *sender) { getTorInfoReply(sender); });
    command->set_replyLine_callback([this](int code, const ByteArray &data) { statusEvent(code,data); });

    std::list<std::string> keys{ "status/circuit-established","status/bootstrap-phase" };

    keys.push_back("net/listeners/socks");

    mSocket->sendCommand(command, command->build(keys));
}

void TorControl::getTorInfoReply(TorControlCommand *sender)
{
    GetConfCommand *command = dynamic_cast<GetConfCommand*>(sender);
    if (!command)
        return;

    std::list<ByteArray> listenAddresses = splitQuotedStrings(command->get("net/listeners/socks").front(), ' ');

    for (const auto& add:listenAddresses) {
        ByteArray value = unquotedString(add);
        int sepp = value.indexOf(':');
        std::string address(value.mid(0, sepp).toString());
        uint16_t port = (uint16_t)value.mid(sepp+1).toInt();

        /* Use the first address that matches the one used for this control connection. If none do,
         * just use the first address and rely on the user to reconfigure if necessary (not a problem;
         * their setup is already very customized) */
        if (mSocksAddress.empty() || address == mSocket->peerAddress()) {
            mSocksAddress = address;
            mSocksPort = port;
            if (address == mSocket->peerAddress())
                break;
        }
    }

    /* It is not immediately an error to have no SOCKS address; when DisableNetwork is set there won't be a
     * listener yet. To handle that situation, we'll try to read the socks address again when TorReady state
     * is reached. */
    if (!mSocksAddress.empty()) {
        RsInfo() << "  SOCKS address is " << mSocksAddress << ":" << mSocksPort ;

        if(rsEvents)
        {
            auto ev = std::make_shared<RsTorManagerEvent>();

            ev->mTorManagerEventType = RsTorManagerEventCode::TOR_CONNECTIVITY_CHANGED;
            ev->mTorConnectivityStatus  = torConnectivityStatus(mStatus);
            ev->mTorStatus = ::torStatus(mTorStatus);
            rsEvents->sendEvent(ev);
        }
    }

    if (ByteArray(command->get("status/circuit-established").front()).toInt() == 1)
    {
        RsInfo() << "  Tor indicates that circuits have been established; state is TorReady" ;
        setTorStatus(TorControl::TorReady);
    }
//    else
//        setTorStatus(TorControl::TorOffline);

    auto bootstrap = command->get("status/bootstrap-phase");
    if (!bootstrap.empty())
        updateBootstrap(splitQuotedStrings(bootstrap.front(), ' '));
}

void TorControl::addHiddenService(HiddenService *service)
{
    if (std::find(mServices.begin(),mServices.end(),service) != mServices.end())
        return;

    mServices.push_back(service);
}

void TorControl::publishServices()
{
    RsInfo() << "Publishing Services... " ;

    assert(isConnected());
    if (mServices.empty())
	{
        RsErr() << "  No service registered!" ;
        return;
	}

    if (torVersionAsNewAs("0.2.7")) {
        for(HiddenService *service: mServices)
        {
            if (service->hostname().empty())
                RsInfo() << "  Creating a new hidden service" ;
            else
                RsInfo() << "  Publishing hidden service: " << service->hostname() ;
            AddOnionCommand *onionCommand = new AddOnionCommand(service);
            //protocolInfoReplyQObject::connect(onionCommand, &AddOnionCommand::succeeded, service, &HiddenService::servicePublished);
            onionCommand->set_succeeded_callback( [this,service]() { checkHiddenService(service) ; });
            mSocket->sendCommand(onionCommand, onionCommand->build());
        }
    } else {
        RsInfo() << "  Using legacy SETCONF hidden service configuration for tor" << mTorVersion ;
        SetConfCommand *command = new SetConfCommand;
        std::list<std::pair<std::string,std::string> > torConfig;

        for(HiddenService *service: mServices)
        {
            if (service->dataPath().empty())
                continue;

            if (service->privateKey().isLoaded() && !RsDirUtil::fileExists(service->dataPath() + "/private_key")) {
                // This case can happen if tor is downgraded after the profile is created
                RsWarn() << "  Cannot publish ephemeral hidden services with this version of tor; skipping";
                continue;
            }

            RsInfo() << "  Configuring hidden service at" << service->dataPath() ;

            torConfig.push_back(std::make_pair("HiddenServiceDir", service->dataPath()));

            const std::list<HiddenService::Target> &targets = service->targets();
            for (auto tit:targets)
            {
                std::string target = RsUtil::NumberToString(tit.servicePort) + " "
                                    +tit.targetAddress + ":"
                                    +RsUtil::NumberToString(tit.targetPort);
                torConfig.push_back(std::make_pair("HiddenServicePort", target));
            }

            command->set_ConfSucceeded_callback( [this,service]() { checkHiddenService(service); });
            //QObject::connect(command, &SetConfCommand::setConfSucceeded, service, &HiddenService::servicePublished);
        }

        if (!torConfig.empty())
            mSocket->sendCommand(command, command->build(torConfig));
    }
}

void TorControl::checkHiddenService(HiddenService *service)
{
    service->servicePublished();

    if(service->status() == HiddenService::Online)
    {
        RsDbg() << "Hidden service published and ready!" ;

        setStatus(TorControl::HiddenServiceReady);
    }
}

void TorControl::shutdown()
{
    if (!hasOwnership()) {
        RsWarn() << "torctrl: Ignoring shutdown command for a tor instance I don't own";
        return;
    }

    mSocket->sendCommand(ByteArray("SIGNAL SHUTDOWN\r\n"));
}

void TorControl::shutdownSync()
{
    if (!hasOwnership()) {
        RsWarn() << "torctrl: Ignoring shutdown command for a tor instance I don't own";
        return;
    }

    shutdown();
    while (mSocket->moretowrite(0))
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    mSocket->close();
}

void TorControl::statusEvent(int /* code */, const ByteArray &data)
{
    std::list<ByteArray> tokens = splitQuotedStrings(data.trimmed(), ' ');
    if (tokens.size() < 3)
        return;

    const ByteArray& tok2 = *(++(++tokens.begin()));

    if(mSocket && mSocket->mVerbose)
        RsInfo() << "  status event:" << data.trimmed().toString() << " tok2=\"" << tok2.toString() << "\"" ;

    if (tok2 == "CIRCUIT_ESTABLISHED")
        setTorStatus(TorControl::TorReady);
    else if (tok2 == "CIRCUIT_NOT_ESTABLISHED")
        setTorStatus(TorControl::TorOffline);
    else if (tok2 == "BOOTSTRAP")
    {
        tokens.pop_front();
        updateBootstrap(tokens);
    }
}

void TorControl::updateBootstrap(const std::list<ByteArray> &data)
{
    mBootstrapStatus.clear();
    // WARN or NOTICE
    mBootstrapStatus["severity"] = (*data.begin()).toString();

    auto dat = data.begin();
    ++dat;

    for(;dat!=data.end();++dat) {               // for(int i = 1; i < data.size(); i++) {
        int equals = (*dat).indexOf('=');
        ByteArray key = (*dat).mid(0, equals);
        ByteArray value;

        if (equals >= 0)
            value = unquotedString((*dat).mid(equals + 1));

        mBootstrapStatus[key.toLower().toString()] = value.toString();
    }

    if(rsEvents)
    {
        auto ev = std::make_shared<RsTorManagerEvent>();

        ev->mTorManagerEventType = RsTorManagerEventCode::BOOTSTRAP_STATUS_CHANGED;
        ev->mTorConnectivityStatus  = torConnectivityStatus(mStatus);
        ev->mTorStatus = ::torStatus(mTorStatus);
        rsEvents->sendEvent(ev);
    }
}

TorControlCommand *TorControl::getConfiguration(const std::string& options)
{
    GetConfCommand *command = new GetConfCommand(GetConfCommand::GetConf);
    command->set_replyLine_callback([this](int code, const ByteArray &data) { statusEvent(code,data); });
    mSocket->sendCommand(command, command->build(options));

    return command;
}

TorControlCommand *TorControl::setConfiguration(const std::list<std::pair<std::string,std::string> >& options)
{
    SetConfCommand *command = new SetConfCommand;
    command->setResetMode(true);
    mSocket->sendCommand(command, command->build(options));

    return command;
}

namespace Tor {

class SaveConfigOperation : public PendingOperation
{
public:
    SaveConfigOperation()
        : PendingOperation(), command(0)
    {
    }

    void start(TorControlSocket *socket)
    {
        assert(!command);
        command = new GetConfCommand(GetConfCommand::GetInfo);
        command->set_finished_callback([this](TorControlCommand *sender){ configTextReply(sender); });

        socket->sendCommand(command, command->build(std::list<std::string> { "config-text" , "config-file" } ));
    }

    void configTextReply(TorControlCommand * /*sender*/)
    {
        assert(command);
        if (!command)
            return;

        auto lpath = command->get("config-file");
        std::string path = (lpath.empty()?std::string():lpath.front());

        if (path.empty()) {
            finishWithError("Cannot write torrc without knowing its path");
            return;
        }

        // Out of paranoia, refuse to write any file not named 'torrc', or if the
        // file doesn't exist

        auto filename = RsDirUtil::getFileName(path);

        if(filename != "torrc" || !RsDirUtil::fileExists(path))
        {
            finishWithError("Refusing to write torrc to unacceptable path " + path);
            return;
        }

        std::ofstream file(path);

        if (!file.is_open()) {
            finishWithError("Failed opening torrc file for writing: permissions error?");
            return;
        }

        // Remove these keys when writing torrc; they are set at runtime and contain
        // absolute paths or port numbers
        static const char *bannedKeys[] = {
            "ControlPortWriteToFile",
            "DataDirectory",
            "HiddenServiceDir",
            "HiddenServicePort",
            0
        };

        auto configText = command->get("config-text") ;

        for(const auto& value: configText)
        {
            ByteArray line(value);

            bool skip = false;
            for (const char **key = bannedKeys; *key; key++) {
                if (line.startsWith(*key)) {
                    skip = true;
                    break;
                }
            }
            if (skip)
                continue;

            file << line.toString() << std::endl;
        }

        file.close();

        RsInfo() << "  Wrote torrc file" ;
        finishWithSuccess();
    }

private:
    GetConfCommand *command;
};

}

PendingOperation *TorControl::saveConfiguration()
{
    if (!hasOwnership()) {
        RsWarn() << "torctrl: Ignoring save configuration command for a tor instance I don't own";
        return 0;
    }

    SaveConfigOperation *operation = new SaveConfigOperation();

    operation->set_finished_callback( [operation]() { delete operation; });
    operation->start(mSocket);

    return operation;
}

bool TorControl::hasOwnership() const
{
    return mHasOwnership;
}

void TorControl::takeOwnership()
{
    mHasOwnership = true;
    mSocket->sendCommand(ByteArray("TAKEOWNERSHIP\r\n"));

    // Reset PID-based polling
    std::list<std::pair<std::string,std::string> > options;
    options.push_back(std::make_pair("__OwningControllerProcess",std::string()));
    setConfiguration(options);
}

bool TorControl::torVersionAsNewAs(const std::string& match) const
{
    auto split = ByteArray(torVersion()).split(ByteArray(".-"));
    auto matchSplit = ByteArray(match).split(ByteArray(".-"));

    int split_size = split.size();
    auto b_split(split.begin());
    auto b_matchsplit(matchSplit.begin());

    for(int i=0;;)
    {
        int currentVal,matchVal;
        bool ok1 = RsUtil::StringToInt((*b_split).toString(),currentVal);
        bool ok2 = RsUtil::StringToInt((*b_matchsplit).toString(),matchVal);

        if (!ok1 || !ok2)
            return false;
        if (currentVal > matchVal)
            return true;
        if (currentVal < matchVal)
            return false;

        ++i;

        if(i >= split_size)
            return false;

        ++b_split;
        ++b_matchsplit;
    }

    // Versions are equal, up to the length of match
    return true;
}


