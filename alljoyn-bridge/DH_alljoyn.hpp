/** @file
@brief The simple gateway example.
@author Sergey Polichnoy <sergey.polichnoy@dataart.com>
@see @ref page_simple_gw
*/
#ifndef __DH_ALLJOYN_HPP_
#define __DH_ALLJOYN_HPP_

#include <DeviceHive/gateway.hpp>
#include <DeviceHive/restful.hpp>
#include <DeviceHive/websocket.hpp>
#include "../examples/basic_app.hpp"

#include <alljoyn/BusAttachment.h>
#include <alljoyn/ProxyBusObject.h>
#include <alljoyn/InterfaceDescription.h>

#include <alljoyn/about/AnnounceHandler.h>
#include <alljoyn/about/AnnouncementRegistrar.h>

#include "hexUtils.hpp"
#include <functional>

// constants
static const char* BUS_NAME = "DeviceHiveToAllJoynGatewayConnector";
static const uint32_t LINK_TIMEOUT = 20;

namespace DH_alljoyn
{
    using namespace hive;

/// @brief Various contants and timeouts.
enum Timeouts
{
    SERVER_RECONNECT_TIMEOUT    = 10000,    ///< @brief Try to open server connection each X milliseconds.
    RETRY_TIMEOUT               = 5000,     ///< @brief Common retry timeout, milliseconds.
    DEVICE_OFFLINE_TIMEOUT      = 0
};


/**
 * @brief Check AllJoyn status code and throw an exception if it's not ER_OK.
 */
inline void AJ_check(QStatus status, const char *text)
{
    if (ER_OK != status)
    {
        std::ostringstream ess;
        ess << text << ": " << QCC_StatusText(status);
        throw std::runtime_error(ess.str());
    }
}



/**
 * @brief The simple gateway application.
 *
 * This application controls multiple devices connected via AllJoyn!
 */
class Application:
    public basic_app::Application,
        public ajn::BusListener,
        public ajn::SessionListener,
        public ajn::services::AnnounceHandler,
        public devicehive::IDeviceServiceEvents
{
    typedef basic_app::Application Base; ///< @brief The base type.
    typedef Application This; ///< @brief The type alias.

protected:

    /// @brief The default constructor.
    Application()
        : m_disableWebsockets(false)
        , m_disableWebsocketPingPong(false)
        , m_web_timeout(0)
        , m_log_AJ("AllJoyn")
    {}

public:

    /// @brief The shared pointer type.
    typedef boost::shared_ptr<Application> SharedPtr;


    /**
     * @brief The factory method.
     * @param[in] argc The number of command line arguments.
     * @param[in] argv The command line arguments.
     * @return The new application instance.
     */
    static SharedPtr create(int argc, const char* argv[])
    {
        SharedPtr pthis(new This());

        String gatewayId = "AJ_gateway1";
        String gatewayKey = "4ce8e040-7175-11e4-82f8-0800200c9a66";

        String networkName = "C++ AllJoyn network";
        String networkKey = "";
        String networkDesc = "C++ device test network";

        String baseUrl = "http://devicehive1-java/devicehive/rest";
        size_t web_timeout = 0; // zero - don't change
        String http_version;

        bool http_keep_alive = true;

        // custom device properties
        for (int i = 1; i < argc; ++i) // skip executable name
        {
            if (boost::algorithm::iequals(argv[i], "--help"))
            {
                std::cout << argv[0] << " [options]";
                std::cout << "\t--networkName <network name>\n";
                std::cout << "\t--networkKey <network authentication key>\n";
                std::cout << "\t--networkDesc <network description>\n";
                std::cout << "\t--server <server URL>\n";
                std::cout << "\t--web-timeout <timeout, seconds>\n";
                std::cout << "\t--no-ws disable automatic websocket service switching\n";
                std::cout << "\t--no-ws-ping-pong disable websocket ping/pong messages\n";
                std::cout << "\t--http-version <major.minor HTTP version>\n";
                std::cout << "\t--http-no-keep-alive disable keep-alive connections\n";
                std::cout << "\t--log <log file name>\n"; // see below in main()

                exit(1);
            }
            else if (boost::algorithm::iequals(argv[i], "--networkName") && i+1 < argc)
                networkName = argv[++i];
            else if (boost::algorithm::iequals(argv[i], "--networkKey") && i+1 < argc)
                networkKey = argv[++i];
            else if (boost::algorithm::iequals(argv[i], "--networkDesc") && i+1 < argc)
                networkDesc = argv[++i];
            else if (boost::algorithm::iequals(argv[i], "--server") && i+1 < argc)
                baseUrl = argv[++i];
            else if (boost::algorithm::iequals(argv[i], "--web-timeout") && i+1 < argc)
                web_timeout = boost::lexical_cast<UInt32>(argv[++i]);
            else if (boost::algorithm::iequals(argv[i], "--http-version") && i+1 < argc)
                http_version = argv[++i];
            else if (boost::iequals(argv[i], "--no-ws"))
                pthis->m_disableWebsockets = true;
            else if (boost::iequals(argv[i], "--no-ws-ping-pong"))
                pthis->m_disableWebsocketPingPong = true;
            else if (boost::iequals(argv[i], "--http-no-keep-alive"))
                http_keep_alive = false;
        }

        pthis->m_network = devicehive::Network::create(networkName, networkKey, networkDesc);
        pthis->m_gw_dev = devicehive::Device::create(gatewayId, "AllJoyn gateway connector", gatewayKey,
                                                     devicehive::Device::Class::create("AllJyon gateway", "0.1"),
                                                     pthis->m_network);
        pthis->m_gw_dev->status = "Online";
        pthis->m_defaultBaseUrl =  baseUrl;
        pthis->m_web_timeout = web_timeout;

        pthis->m_http = http::Client::create(pthis->m_ios);
        pthis->m_http->enableKeepAliveConnections(http_keep_alive);

        pthis->AJ_init();

        if (1) // create service
        {
            http::Url url(baseUrl);

            if (boost::iequals(url.getProtocol(), "ws")
                || boost::iequals(url.getProtocol(), "wss"))
            {
                if (pthis->m_disableWebsockets)
                    throw std::runtime_error("websockets are disabled by --no-ws switch");

                HIVELOG_INFO_STR(pthis->m_log, "WebSocket service is used");
                devicehive::WebsocketService::SharedPtr service = devicehive::WebsocketService::create(
                    http::Client::create(pthis->m_ios), baseUrl, pthis);
                service->setPingPongEnabled(!pthis->m_disableWebsocketPingPong);
                if (0 < web_timeout)
                    service->setTimeout(web_timeout*1000); // seconds -> milliseconds

                pthis->m_service = service;
            }
            else
            {
                HIVELOG_INFO_STR(pthis->m_log, "RESTful service is used");
                devicehive::RestfulService::SharedPtr service = devicehive::RestfulService::create(
                    http::Client::create(pthis->m_ios), baseUrl, pthis);
                if (0 < web_timeout)
                    service->setTimeout(web_timeout*1000); // seconds -> milliseconds
                if (!http_version.empty())
                {
                    int major = 1, minor = 1;
                    parseVersion(http_version, major, minor);
                    service->setHttpVersion(major, minor);
                }

                pthis->m_service = service;
            }
        }

        return pthis;
    }


    /// @brief Get the shared pointer.
    /**
    @return The shared pointer to this instance.
    */
    SharedPtr shared_from_this()
    {
        return boost::dynamic_pointer_cast<This>(Base::shared_from_this());
    }

private:

    /**
     * @brief Init AllJoyn infrastructure.
     */
    void AJ_init()
    {
        HIVELOG_TRACE(m_log_AJ, "creating BusAttachment");
        m_AJ_bus.reset(new ajn::BusAttachment(BUS_NAME, true));

        HIVELOG_TRACE(m_log_AJ, "register bus listener&start");
        m_AJ_bus->RegisterBusListener(*this);
        QStatus status = m_AJ_bus->Start();
        AJ_check(status, "failed to start AllJoyn bus");

        // connect
        HIVELOG_TRACE(m_log_AJ, "connecting");
        status = m_AJ_bus->Connect();
        AJ_check(status, "failed to connect AllJoyn bus");
        HIVELOG_INFO(m_log_AJ, "connected to BUS: " << m_AJ_bus->GetUniqueName().c_str());

        // watch for announcements
        status = ajn::services::AnnouncementRegistrar::RegisterAnnounceHandler(*m_AJ_bus, *this,
                                                                               NULL, 0); // all!
        AJ_check(status, "failed to register announce handler");
    }

protected:

    /**
     * @brief Start the application.
     */
    virtual void start()
    {
        Base::start();

        m_service->asyncConnect();
    }


    /**
     * @brief Stop the application.
     */
    virtual void stop()
    {
        m_service->cancelAll();

        HIVELOG_INFO(m_log_AJ, "disconnecting BUS: " << m_AJ_bus->GetUniqueName().c_str());
        QStatus status = m_AJ_bus->Disconnect();
        AJ_check(status, "failed to disconnect AllJoyn bus");

        HIVELOG_INFO(m_log_AJ, "stopping bus...");
        status = m_AJ_bus->Stop();
        AJ_check(status, "failed to stop bus attachment");

        Base::stop();
    }


private: // devicehive::IDeviceServiceEvents

    /// @copydoc devicehive::IDeviceServiceEvents::onConnected()
    virtual void onConnected(ErrorCode err)
    {
        HIVELOG_TRACE_BLOCK(m_log, "onConnected()");

        if (!err)
        {
            HIVELOG_INFO_STR(m_log, "connected to the server");
            m_service->asyncGetServerInfo();
        }
        else
            handleError(err, "connection");
    }


    /// @copydoc devicehive::IDeviceServiceEvents::onServerInfo()
    virtual void onServerInfo(boost::system::error_code err, devicehive::ServerInfo info)
    {
        if (!err)
        {
            m_lastCommandTimestamp = info.timestamp;

            // try to switch to websocket protocol
            if (!m_disableWebsockets && !info.alternativeUrl.empty())
                if (devicehive::RestfulService::SharedPtr rest = boost::dynamic_pointer_cast<devicehive::RestfulService>(m_service))
            {
                HIVELOG_INFO(m_log, "switching to Websocket service: " << info.alternativeUrl);
                rest->cancelAll();

                devicehive::WebsocketService::SharedPtr service = devicehive::WebsocketService::create(
                    rest->getHttpClient(), info.alternativeUrl, shared_from_this());
                service->setPingPongEnabled(!m_disableWebsocketPingPong);
                service->setTimeout(rest->getTimeout());
                m_service = service;

                // connect again as soon as possible
                m_delayed->callLater(boost::bind(&devicehive::IDeviceService::asyncConnect, m_service));
                return;
            }

            m_service->asyncRegisterDevice(m_gw_dev);
        }
        else
            handleError(err, "getting server info");
    }


    /// @copydoc devicehive::IDeviceServiceEvents::onRegisterDevice()
    virtual void onRegisterDevice(boost::system::error_code err, devicehive::DevicePtr device)
    {
        if (!err)
        {
            m_service->asyncSubscribeForCommands(m_gw_dev, m_lastCommandTimestamp);
            // TODO: notify pending announces
        }
        else
            handleError(err, "registering device");
    }


    /// @copydoc devicehive::IDeviceServiceEvents::onInsertCommand()
    virtual void onInsertCommand(ErrorCode err, devicehive::DevicePtr device, devicehive::CommandPtr command)
    {
        if (!err)
        {
            m_lastCommandTimestamp = command->timestamp;
            bool processed = true;

            try
            {
                throw std::runtime_error("unknown command");
            }
            catch (std::exception const& ex)
            {
                HIVELOG_ERROR(m_log, "handle command error: "
                    << ex.what());

                command->status = "Failed";
                command->result = ex.what();
            }

            if (processed)
                m_service->asyncUpdateCommand(device, command);
        }
        else
            handleError(err, "polling command");
    }

private:

    /// @brief Handle the communication error.
    /**
    @param[in] err The error code.
    @param[in] hint The custom hint.
    */
    void handleError(boost::system::error_code err, const char *hint)
    {
        if (!terminated())
        {
            HIVELOG_ERROR(m_log, (hint ? hint : "something")
                << " failed: [" << err << "] " << err.message());

            m_service->cancelAll();

            HIVELOG_DEBUG_STR(m_log, "try to connect later...");
            m_delayed->callLater(SERVER_RECONNECT_TIMEOUT,
                boost::bind(&devicehive::IDeviceService::asyncConnect, m_service));
        }
    }

private: // ajn::BusListener interface

    virtual void FoundAdvertisedName(const char* name, ajn::TransportMask transport, const char* namePrefix)
    {
        HIVELOG_INFO(m_log_AJ, "found advertised name:\"" << name << "\", prefix:\"" << namePrefix << "\"");
        HIVE_UNUSED(transport);
    }


    virtual void LostAdvertisedName(const char* name, ajn::TransportMask transport, const char* namePrefix)
    {
        HIVELOG_INFO(m_log_AJ, "advertised name is lost:\"" << name << "\", prefix:\"" << namePrefix << "\"");
        HIVE_UNUSED(transport);
    }


    virtual void NameOwnerChanged(const char* busName, const char* previousOwner, const char* newOwner)
    {
        HIVELOG_INFO(m_log_AJ, "bus:\"" << busName << "\" name owner changed from \""
                     << (previousOwner?previousOwner:"<null>") << "\" to \""
                     << (newOwner?newOwner:"<null>") << "\"");

//        if (previousOwner && !newOwner)
//        {
//            // do it on main thread!
//            m_ios.post(boost::bind(&Application::doSessionNameLost,
//                shared_from_this(), hive::String(previousOwner)));
//        }
    }


    virtual void BusStopping()
    {
        HIVELOG_INFO(m_log_AJ, "bus stopping");
    }


    virtual void BusDisconnected()
    {
        HIVELOG_INFO(m_log_AJ, "bus disconnected");
    }

private: // ajn::SessionListener interface

    virtual void SessionLost(ajn::SessionId sessionId, SessionLostReason reason)
    {
        HIVELOG_INFO(m_log_AJ, "session #" << sessionId << " lost");
        HIVE_UNUSED(reason);

        // do it on main thread!
        //m_ios.post(boost::bind(&Application::doSessionLost, shared_from_this(), sessionId));
    }


    virtual void SessionMemberAdded(ajn::SessionId sessionId, const char* uniqueName)
    {
        HIVELOG_INFO(m_log_AJ, "session #" << sessionId << " member added:\"" << uniqueName << "\"");
    }


    virtual void SessionMemberRemoved(ajn::SessionId sessionId, const char* uniqueName)
    {
        HIVELOG_INFO(m_log_AJ, "session #" << sessionId << " member removed:\"" << uniqueName << "\"");
    }

private: // ajn::services::AnnounceHandler

    virtual void Announce(uint16_t version, uint16_t port, const char* busName,
                          const ObjectDescriptions& objectDescs,
                          const AboutData& aboutData)
    {
        HIVELOG_INFO(m_log_AJ, "Announce version:" << version << ", port:" << port
                  << ", bus:\"" << busName << "\"");

        String bus_name(busName);
        std::map<String, std::vector<String> > objects;

        ObjectDescriptions::const_iterator od = objectDescs.begin();
        for (; od != objectDescs.end(); ++od)
        {
            // copy interface names
            std::vector<String> interfaces;
            interfaces.reserve(od->second.size());
            for (size_t i = 0; i < od->second.size(); ++i)
                interfaces.push_back(od->second[i].c_str());

            String obj_name = od->first.c_str();
            objects[obj_name] = interfaces;
        }
        HIVE_UNUSED(aboutData);

        // do processing on main thread!
        m_ios.post(boost::bind(&This::safeAnnounce, shared_from_this(),
                               bus_name, port, objects));
    }

    void safeAnnounce(const String busName, int port, const std::map<String, std::vector<String> > &objs)
    {
        HIVE_UNUSED(busName);
        HIVE_UNUSED(port);
        HIVE_UNUSED(objs);
    }

private:
    bool m_disableWebsockets;       ///< @brief No automatic websocket switch.
    bool m_disableWebsocketPingPong; ///< @brief Disable websocket PING/PONG messages.
    http::ClientPtr m_http;

    devicehive::IDeviceServicePtr m_service; ///< @brief The cloud service.
    devicehive::NetworkPtr m_network;
    devicehive::DevicePtr m_gw_dev;  // gateway device
    String m_lastCommandTimestamp; ///< @brief The timestamp of the last received command.
    String m_defaultBaseUrl;
    int m_web_timeout;

private:
    boost::shared_ptr<ajn::BusAttachment> m_AJ_bus;

private:
    hive::log::Logger m_log_AJ;
};


/// @brief The simple gateway application entry point.
/**
Creates the Application instance and calls its Application::run() method.

@param[in] argc The number of command line arguments.
@param[in] argv The command line arguments.
*/
inline void main(int argc, const char* argv[])
{
    { // configure logging
        using namespace hive::log;

        hive::String log_file_name = "/tmp/DH_alljoyn.log";

        for (int i = 1; i < argc; ++i)
        {
            if (boost::iequals(argv[i], "--log") && (i+1 < argc))
                log_file_name = argv[i+1];
        }

        Target::File::SharedPtr log_file = Target::File::create(log_file_name);
        Target::SharedPtr log_console = Logger::root().getTarget();
        Logger::root().setTarget(Target::Tie::create(log_file, log_console));
        Logger::root().setLevel(LEVEL_TRACE);
        Logger("/devicehive/rest").setTarget(log_file); // disable annoying messages
        Logger("/hive/websocket").setTarget(log_file).setLevel(LEVEL_DEBUG); // disable annoying messages
        Logger("/hive/http").setTarget(log_file).setLevel(LEVEL_INFO); // disable annoying messages
        log_console->setFormat(Format::create("%N %L %M\n"));
        log_console->setMinimumLevel(LEVEL_DEBUG);
        //log_file->setAutoFlushLevel(LEVEL_TRACE);
        log_file->setMaxFileSize(10*1024*1024);
        log_file->setNumberOfBackups(1);
        log_file->setFormat(Format::create("%T %N %L [%I] %M\n"));
        log_file->startNew();
    }

    Application::create(argc, argv)->run();
}

} // DH_alljoyn namespace


#endif // __DH_ALLJOYN_HPP_
