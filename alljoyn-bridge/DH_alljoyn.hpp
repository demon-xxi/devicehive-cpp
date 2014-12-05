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
static const uint32_t PING_TIMEOUT = 20;
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
        public ajn::BusAttachment::PingAsyncCB,
        public devicehive::IDeviceServiceEvents
{
    typedef basic_app::Application Base; ///< @brief The base type.
    typedef Application This; ///< @brief The type alias.

protected:

    /// @brief The default constructor.
    Application()
        : m_disableWebsockets(false)
        , m_disableWebsocketPingPong(false)
        , m_gw_dev_registered(false)
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

        pthis->m_http = http::Client::create(pthis->m_ios);
        pthis->m_http->enableKeepAliveConnections(http_keep_alive);

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

        pthis->AJ_init();
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

        HIVELOG_TRACE(m_log_AJ, "registering bus listener and starting");
        m_AJ_bus->RegisterBusListener(*this);
        QStatus status = m_AJ_bus->Start();
        AJ_check(status, "failed to start AllJoyn bus");

        HIVELOG_TRACE(m_log_AJ, "connecting");
        status = m_AJ_bus->Connect();
        AJ_check(status, "failed to connect AllJoyn bus");
        HIVELOG_INFO(m_log_AJ, "connected to bus:\""
            << m_AJ_bus->GetUniqueName().c_str() << "\"");
    }

protected:

    /**
     * @brief Start the application.
     */
    virtual void start()
    {
        Base::start();

        // TODO: connect to alljoyn bus here
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

        // release objects
        m_bus_proxies.clear();
        m_AJ_bus.reset();

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

            HIVELOG_INFO_STR(m_log, "got server info, registering...");
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
            HIVELOG_INFO_STR(m_log, "registered, subscribing for commands...");
            m_service->asyncSubscribeForCommands(m_gw_dev, m_lastCommandTimestamp);
            m_gw_dev_registered = true;

            sendPendingNotifications();
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
            command->status = "Success";

            try
            {
                const      String &cmd_name   = command->name;
                const json::Value &cmd_params = command->params;

                HIVELOG_INFO(m_log, "got \"" << cmd_name << "\" command");

                if (false)
                    ;
                else if (cmd_name == "AllJoyn/WatchAnnounces")
                {
                    const json::Value &j_ifaces = cmd_params;

                    std::vector<String> ifaces;
                    for (size_t i = 0; i < j_ifaces.size(); ++i)
                        ifaces.push_back(j_ifaces[i].asString());
                    if (j_ifaces.isString())
                        ifaces.push_back(j_ifaces.asString());

                    watchAnnounces(ifaces);
                }
                else if (cmd_name == "AllJoyn/UnwatchAnnounces")
                {
                    const json::Value &j_ifaces = cmd_params;

                    std::vector<String> ifaces;
                    for (size_t i = 0; i < j_ifaces.size(); ++i)
                        ifaces.push_back(j_ifaces[i].asString());
                    if (j_ifaces.isString())
                        ifaces.push_back(j_ifaces.asString());

                    unwatchAnnounces(ifaces);
                }
                else if (cmd_name == "AllJoyn/FindAdvertisedName")
                {
                    const json::Value &j_prefix = cmd_params;
                    findAdvertisedName(j_prefix.asString());
                }
                else if (cmd_name == "AllJoyn/Ping")
                {
                    const json::Value &j_name = cmd_params;
                    ping(j_name.asString(), command);
                    processed = false; // will be updated in PingCB()
                }
                else if (cmd_name == "AllJoyn/GetObjectInfo")
                {
                    const json::Value &j_addr = cmd_params;

                    AJ_BusProxyPtr pBus = getBusProxy(j_addr["bus"].asString(),
                                                      j_addr["port"].asInt());
                    if (!pBus) throw std::runtime_error("not bus found");

                    AJ_ObjProxyPtr pObj = getObjProxy(pBus, j_addr["object"].asString());
                    if (!pObj) throw std::runtime_error("no object found");

                    command->result = pObj->getObjectInfo();
                }
                else if (cmd_name == "AllJoyn/GetInterfaceInfo")
                {
                    const json::Value &j_addr = cmd_params;

                    AJ_BusProxyPtr pBus = getBusProxy(j_addr["bus"].asString(),
                                                      j_addr["port"].asInt());
                    if (!pBus) throw std::runtime_error("not bus found");

                    AJ_ObjProxyPtr pObj = getObjProxy(pBus, j_addr["object"].asString());
                    if (!pObj) throw std::runtime_error("no object found");

                    command->result = pObj->getInterfaceInfo(cmd_params["interface"].asString());
                }
                else if (cmd_name == "AllJoyn/CallMethod")
                {
                    const json::Value &j_addr = cmd_params;

                    AJ_BusProxyPtr pBus = getBusProxy(j_addr["bus"].asString(),
                                                      j_addr["port"].asInt());
                    if (!pBus) throw std::runtime_error("not bus found");

                    AJ_ObjProxyPtr pObj = getObjProxy(pBus, j_addr["object"].asString());
                    if (!pObj) throw std::runtime_error("no object found");

                    String iface = cmd_params["interface"].asString();
                    String method = cmd_params["method"].asString();

                    json::Value args = cmd_params["arguments"];

                    command->status = pObj->callMethod(iface, method,
                                              args, &command->result);
                }
                else if (cmd_name == "AllJoyn/GetProperty")
                {
                    const json::Value &j_addr = cmd_params;

                    AJ_BusProxyPtr pBus = getBusProxy(j_addr["bus"].asString(),
                                                      j_addr["port"].asInt());
                    if (!pBus) throw std::runtime_error("not bus found");

                    AJ_ObjProxyPtr pObj = getObjProxy(pBus, j_addr["object"].asString());
                    if (!pObj) throw std::runtime_error("no object found");

                    String iface = cmd_params["interface"].asString();
                    String property = cmd_params["property"].asString();

                    command->status = pObj->getProperty(iface, property, &command->result);
                }
                else if (cmd_name == "AllJoyn/SetProperty")
                {
                    const json::Value &j_addr = cmd_params;

                    AJ_BusProxyPtr pBus = getBusProxy(j_addr["bus"].asString(),
                                                      j_addr["port"].asInt());
                    if (!pBus) throw std::runtime_error("not bus found");

                    AJ_ObjProxyPtr pObj = getObjProxy(pBus, j_addr["object"].asString());
                    if (!pObj) throw std::runtime_error("no object found");

                    String iface = cmd_params["interface"].asString();
                    String property = cmd_params["property"].asString();
                    json::Value val = cmd_params["value"];

                    command->status = pObj->setProperty(iface, property, val);
                    command->result = json::Value::null();
                }
                else if (cmd_name == "AllJoyn/WatchSignal")
                {
                    String obj = cmd_params["object"].asString();
                    String iface = cmd_params["interface"].asString();
                    String signal = cmd_params["signal"].asString();

                    const ajn::InterfaceDescription *i = m_AJ_bus->GetInterface(iface.c_str());
                    if (!i) throw std::runtime_error("no interface found");

                    const ajn::InterfaceDescription::Member *s = i->GetSignal(signal.c_str());
                    if (!s) throw std::runtime_error("no signal found");

                    QStatus status = m_AJ_bus->RegisterSignalHandler(this, (MessageReceiver::SignalHandler)&This::onSignalHandler, s, obj.empty() ? NULL : obj.c_str());
                    AJ_check(status, "failed to register signal handler");
                }
                else if (cmd_name == "AllJoyn/UnwatchSignal")
                {
                    String obj = cmd_params["object"].asString();
                    String iface = cmd_params["interface"].asString();
                    String signal = cmd_params["signal"].asString();

                    const ajn::InterfaceDescription *i = m_AJ_bus->GetInterface(iface.c_str());
                    if (!i) throw std::runtime_error("no interface found");

                    const ajn::InterfaceDescription::Member *s = i->GetSignal(signal.c_str());
                    if (!s) throw std::runtime_error("no signal found");

                    QStatus status = m_AJ_bus->UnregisterSignalHandler(this, (MessageReceiver::SignalHandler)&This::onSignalHandler, s, obj.empty() ? NULL : obj.c_str());
                    AJ_check(status, "failed to unregister signal handler");
                }
                else
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
            m_gw_dev_registered = false;

            HIVELOG_DEBUG_STR(m_log, "try to connect later...");
            m_delayed->callLater(SERVER_RECONNECT_TIMEOUT,
                boost::bind(&devicehive::IDeviceService::asyncConnect, m_service));
        }
    }

private:
    std::vector<devicehive::NotificationPtr> m_pendingNotifications;

    /**
     * @brief Send all pending notifications.
     */
    void sendPendingNotifications()
    {
        if (!m_service)
            return;

        for (size_t i = 0; i < m_pendingNotifications.size(); ++i)
            m_service->asyncInsertNotification(m_gw_dev, m_pendingNotifications[i]);
        m_pendingNotifications.clear();
    }

private: // ajn::BusListener interface

    void findAdvertisedName(const String &namePrefix)
    {
        QStatus status = m_AJ_bus->FindAdvertisedName(namePrefix.c_str());
        AJ_check(status, "failed to find advertised name");
    }


    virtual void FoundAdvertisedName(const char* name, ajn::TransportMask transport, const char* namePrefix)
    {
        HIVELOG_INFO(m_log_AJ, "found advertised name:\"" << name << "\", prefix:\"" << namePrefix << "\"");
        HIVE_UNUSED(transport);

        // do processing on main thread
        m_ios.post(boost::bind(&This::safeFoundAdvertisedName, shared_from_this(),
                               String(name), String(namePrefix)));
    }

    /**
     * @brief Send FoundAdvertisedName notification.
     */
    void safeFoundAdvertisedName(const String &name, const String &namePrefix)
    {
        json::Value params;
        params["name"] = name;
        params["prefix"] = namePrefix;
        devicehive::NotificationPtr p = devicehive::Notification::create("AllJoyn/FoundAdvertisedName", params);
        if (m_service && m_gw_dev && m_gw_dev_registered)
            m_service->asyncInsertNotification(m_gw_dev, p);
        else
            m_pendingNotifications.push_back(p);
    }


    virtual void LostAdvertisedName(const char* name, ajn::TransportMask transport, const char* namePrefix)
    {
        HIVELOG_INFO(m_log_AJ, "advertised name is lost:\"" << name << "\", prefix:\"" << namePrefix << "\"");
        HIVE_UNUSED(transport);

        // do processing on main thread
        m_ios.post(boost::bind(&This::safeLostAdvertisedName, shared_from_this(),
                               String(name), String(namePrefix)));
    }

    /**
     * @brief Send LostAdvertisedName notification.
     */
    void safeLostAdvertisedName(const String &name, const String &namePrefix)
    {
        json::Value params;
        params["name"] = name;
        params["prefix"] = namePrefix;
        devicehive::NotificationPtr p = devicehive::Notification::create("AllJoyn/LostAdvertisedName", params);
        if (m_service && m_gw_dev && m_gw_dev_registered)
            m_service->asyncInsertNotification(m_gw_dev, p);
        else
            m_pendingNotifications.push_back(p);
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

    /**
     * @brief Watch for Announce signals.
     * @param ifaces The list of supported interfaces.
     *      Empty for all interfaces.
     */
    void watchAnnounces(const std::vector<String> &ifaces)
    {
        std::vector<const char*> p(ifaces.size());
        for (size_t i = 0; i < ifaces.size(); ++i)
            p[i] = ifaces[i].c_str();

        // watch for announces
        QStatus status = ajn::services::AnnouncementRegistrar::RegisterAnnounceHandler(*m_AJ_bus,
                                *this, p.empty() ? NULL : &p[0], p.size());
        AJ_check(status, "failed to register announce handler");
    }


    /**
     * @brief Unwatch Announce signals.
     * @param ifaces The list of supported interfaces.
     *      Empty for all interfaces.
     */
    void unwatchAnnounces(const std::vector<String> &ifaces)
    {
        std::vector<const char*> p(ifaces.size());
        for (size_t i = 0; i < ifaces.size(); ++i)
            p[i] = ifaces[i].c_str();

        // unwatch announces
        QStatus status = ajn::services::AnnouncementRegistrar::UnRegisterAnnounceHandler(*m_AJ_bus,
                                *this, p.empty() ? NULL : &p[0], p.size());
        AJ_check(status, "failed to unregister announce handler");
    }


    /**
     * @brief Announce information.
     */
    struct AnnounceInfo
    {
        String busName;
        int    port;

        typedef std::vector<String> InterfaceList;
        std::map<String, InterfaceList> objects;


        /**
         * @brief Default constructor.
         */
        AnnounceInfo()
            : port(0)
        {}


        /**
         * @brief Convert to JSON.
         */
        json::Value toJson() const
        {
            json::Value params;
            params["bus"] = busName;
            params["port"] = port;

            typedef std::map<String,InterfaceList>::const_iterator Iterator;
            for (Iterator i = objects.begin(); i != objects.end(); ++i)
            {
                const String &name = i->first;
                const InterfaceList &ifaces = i->second;

                // copy interfaces
                json::Value j_ifaces;
                j_ifaces.resize(ifaces.size());
                for (size_t k = 0; k < ifaces.size(); ++k)
                    j_ifaces[k] = ifaces[k];

                params["objects"][name]/*["interfaces"]*/ = j_ifaces;
            }

            return params;
        }
    };


    /**
     * @brief Is called without AllJoyn context.
     */
    virtual void Announce(uint16_t version, uint16_t port, const char* busName,
                          const ObjectDescriptions& objectDescs,
                          const AboutData& aboutData)
    {
        HIVELOG_INFO(m_log_AJ, "Announce version:" << version << ", port:" << port
                  << ", bus:\"" << busName << "\"");

        AnnounceInfo info;
        info.busName = busName;
        info.port    = port;

        ObjectDescriptions::const_iterator od = objectDescs.begin();
        for (; od != objectDescs.end(); ++od)
        {
            // copy interface names
            AnnounceInfo::InterfaceList ifaces;
            ifaces.reserve(od->second.size());
            for (size_t i = 0; i < od->second.size(); ++i)
                ifaces.push_back(od->second[i].c_str());

            String objName = od->first.c_str();
            info.objects[objName] = ifaces;
        }
        HIVE_UNUSED(aboutData);

        // do processing on main thread!
        m_ios.post(boost::bind(&This::safeAnnounce, shared_from_this(), info));
    }


    /**
     * @brief Got Announce signal.
     */
    void safeAnnounce(const AnnounceInfo &info)
    {
        devicehive::NotificationPtr p = devicehive::Notification::create("AllJoyn/Announce", info.toJson());
        if (m_service && m_gw_dev && m_gw_dev_registered)
            m_service->asyncInsertNotification(m_gw_dev, p);
        else
            m_pendingNotifications.push_back(p);
    }

private: // Ping

    // ping context
    struct PingContext
    {
        String name;
        devicehive::CommandPtr command;

        PingContext(const String &n, devicehive::CommandPtr c)
            : name(n)
            , command(c)
        {}
    };


    /**
     * @brief Send ping request.
     */
    void ping(const String &name, devicehive::CommandPtr command)
    {
        std::auto_ptr<PingContext> context(new PingContext(name, command));
        QStatus status = m_AJ_bus->PingAsync(name.c_str(), PING_TIMEOUT, this, context.get());
        AJ_check(status, "failed to initiate Ping request");
        context.release(); // will be deleted in PingCB
    }

    virtual void PingCB(QStatus status, void* context_)
    {
        std::auto_ptr<PingContext> context(reinterpret_cast<PingContext*>(context_));

        // do processing on main thread
        m_ios.post(boost::bind(&This::safePingCB, shared_from_this(),
                                status, context->name, context->command));
    }

    void safePingCB(QStatus status, const String &name, devicehive::CommandPtr command)
    {
        command->status = (status == ER_OK) ? "Success" : "Failed";
        command->result = String(QCC_StatusText(status));
        HIVE_UNUSED(name);

        if (m_service && m_gw_dev && m_gw_dev_registered)
            m_service->asyncUpdateCommand(m_gw_dev, command);
    }

private: // signal handlers
    void onSignalHandler(const ajn::InterfaceDescription::Member *member, const char *srcPath, ajn::Message &message)
    {
        MsgArgInfo meta("", member->signature.c_str(), member->argNames.c_str());

        const ajn::MsgArg *p_args = 0;
        size_t n_args = 0;
        message->GetArgs(n_args, p_args);
        std::vector<ajn::MsgArg> aj_args(p_args, p_args+n_args);
        json::Value args = AJ_toJson(aj_args, meta);

        // do processing on main thread
        m_ios.post(boost::bind(&This::safeSignalHandler, shared_from_this(),
                        String(member->name.c_str()), String(srcPath), args));
    }

    void safeSignalHandler(const String &signalName, const String &objectPath, const json::Value &args)
    {
        json::Value params;
        params["object"] = objectPath;
        params["arguments"] = args;

        devicehive::NotificationPtr p = devicehive::Notification::create("AllJoyn/Signal/" + signalName, params);
        if (m_service && m_gw_dev && m_gw_dev_registered)
            m_service->asyncInsertNotification(m_gw_dev, p);
        else
            m_pendingNotifications.push_back(p);
    }

private: // AllJoyn bus structure

    class AJ_BusProxy;
    class AJ_ObjProxy;

    typedef boost::shared_ptr<AJ_BusProxy> AJ_BusProxyPtr;
    typedef boost::shared_ptr<AJ_ObjProxy> AJ_ObjProxyPtr;


    /**
     * @brief The remote bus.
     *
     * Represents remote bus.
     */
    class AJ_BusProxy
    {
    private:
        AJ_BusProxy(boost::shared_ptr<ajn::BusAttachment> bus, const String &name, int port, ajn::SessionListener *listener)
            : m_bus(bus)
            , m_name(name)
            , m_port(port)
            , m_sessionId(0)
        {
            ajn::SessionOpts opts(ajn::SessionOpts::TRAFFIC_MESSAGES, false/*multipoint*/,
                                  ajn::SessionOpts::PROXIMITY_ANY, ajn::TRANSPORT_ANY);

            // TODO: do it in asynchronous way
            QStatus status = m_bus->JoinSession(m_name.c_str(), m_port, listener, m_sessionId, opts);
            AJ_check(status, "cannot join session");
        }

    public:
        ~AJ_BusProxy()
        {
            m_bus->LeaveSession(m_sessionId); // ignore status
        }

    public:
        typedef boost::shared_ptr<AJ_BusProxy> SharedPtr;

        /**
         * @brief Create new remote bus.
         */
        static SharedPtr create(boost::shared_ptr<ajn::BusAttachment> bus, const String &name, int port, ajn::SessionListener *listener)
        {
            return SharedPtr(new AJ_BusProxy(bus, name, port, listener));
        }

    private:
    public:
        boost::shared_ptr<ajn::BusAttachment> m_bus;
        String m_name;
        int m_port;

        ajn::SessionId m_sessionId;

    public:
        std::vector<AJ_ObjProxyPtr> m_obj_proxies;
    };


    /**
     * @brief The remote object.
     */
    class AJ_ObjProxy
    {
    private:
        AJ_ObjProxy(AJ_BusProxyPtr pBusProxy, const String &name)
            : m_name(name)
            , m_proxy(*pBusProxy->m_bus,
                      pBusProxy->m_name.c_str(),
                      m_name.c_str(),
                      pBusProxy->m_sessionId,
                      false)
            , m_pBusProxy(pBusProxy)
        {
            if (m_proxy.IsValid())
            {
                QStatus status = m_proxy.IntrospectRemoteObject();
                AJ_check(status, "cannot introspect remote object");
            }
        }

    public:
        typedef boost::shared_ptr<AJ_ObjProxy> SharedPtr;

        /**
         * @brief Create new remote object.
         */
        static SharedPtr create(AJ_BusProxyPtr pBusProxy, const String &name)
        {
            return SharedPtr(new AJ_ObjProxy(pBusProxy, name));
        }

    public:

        /**
         * @brief Get object information.
         *
         *  List of interfaces.
         */
        json::Value getObjectInfo() const
        {
            const int N = 1024;
            json::Value iface_list;

            const ajn::InterfaceDescription* ifaces[N];
            int n = m_proxy.GetInterfaces(ifaces, N);
            iface_list.resize(n);
            for (int i = 0; i < n; ++i)
            {
                const ajn::InterfaceDescription *iface = ifaces[i];
                const String name = iface->GetName();
                iface_list[i] = name;
            }

            json::Value info;
            info["interfaces"] = iface_list;
            return info;
        }


        /**
         * @brief Get interface information.
         *
         * List of members/signals/properties.
         */
        json::Value getInterfaceInfo(const String &name) const
        {
            const int N = 1024;
            json::Value res;

            const ajn::InterfaceDescription *iface = m_proxy.GetInterface(name.c_str());
            if (!iface) throw std::runtime_error("no interface found");

            // methods and signals
            const ajn::InterfaceDescription::Member* members[N];
            int n = iface->GetMembers(members, N);
            for (int i = 0; i < n; ++i)
            {
                const ajn::InterfaceDescription::Member *mb = members[i];
                const String name = mb->name.c_str();

                json::Value info;
                info["signature"] = String(mb->signature.c_str());
                info["returnSignature"] = String(mb->returnSignature.c_str());
                info["argumentNames"] = String(mb->argNames.c_str());

                if (mb->memberType == ajn::MESSAGE_METHOD_CALL)
                    res["methods"][name] = info;
                else if (mb->memberType == ajn::MESSAGE_SIGNAL)
                    res["signals"][name] = info;
            }

            // properties
            const ajn::InterfaceDescription::Property* properties[N];
            int m = iface->GetProperties(properties, N);
            for (int i = 0; i < m; ++i)
            {
                const ajn::InterfaceDescription::Property *p = properties[i];
                const String name = p->name.c_str();

                json::Value info;
                info["signature"] = String(p->signature.c_str());
                if (p->access == ajn::PROP_ACCESS_READ)
                    info["access"] = "read-only";
                else if (p->access == ajn::PROP_ACCESS_WRITE)
                    info["access"] = "write-only";
                else if (p->access == ajn::PROP_ACCESS_RW)
                    info["access"] = "read-write";

                res["properties"][name] = info;
            }

            return res;
        }

    public:

        /**
         * @brief Call method on remote object.
         */
        String callMethod(const String &ifaceName, const String &methodName,
                          const json::Value &arg, json::Value *res)
        {
            const ajn::InterfaceDescription *iface = m_proxy.GetInterface(ifaceName.c_str());
            if (!iface) throw std::runtime_error("no interface found");

            const ajn::InterfaceDescription::Member *func = iface->GetMember(methodName.c_str());
            if (!func) throw std::runtime_error("no method found");

            MsgArgInfo meta(func->signature.c_str(),
                         func->returnSignature.c_str(),
                         func->argNames.c_str());

            std::cerr << "CALL: " << ifaceName << "." << methodName
                         << " with \"" << meta.argSign << "\"-\""
                            << meta.retSign << "\n";
            std::vector<ajn::MsgArg> args = AJ_fromJson(arg, meta);
            ajn::Message reply(*m_pBusProxy->m_bus);

            // TODO: do it in async way
            QStatus status = m_proxy.MethodCall(ifaceName.c_str(), methodName.c_str(),
                                                 &args[0], args.size(), reply);
            if (ER_OK == status)
            {
                const ajn::MsgArg* raw_args = 0;
                size_t N_args = 0;
                reply->GetArgs(N_args, raw_args);

                std::vector<ajn::MsgArg> ret_args(raw_args, raw_args + N_args);
                *res = AJ_toJson(ret_args, meta, args.size());
            }

            return String(QCC_StatusText(status));
        }


        /**
         * @brief Get property value.
         */
        String getProperty(const String &ifaceName, const String &propertyName, json::Value *val)
        {
            const ajn::InterfaceDescription *iface = m_proxy.GetInterface(ifaceName.c_str());
            if (!iface) throw std::runtime_error("no interface found");

            const ajn::InterfaceDescription::Property *prop = iface->GetProperty(propertyName.c_str());
            if (!prop) throw std::runtime_error("no property found");
            if ((prop->access&ajn::PROP_ACCESS_READ) == 0)
                throw std::runtime_error("property is not readable");

            MsgArgInfo meta("", prop->signature.c_str(), propertyName);

            std::cerr << "GET-PROP: " << ifaceName << "." << propertyName
                         << " with \"" << meta.retSign << "\n";
            std::vector<ajn::MsgArg> ret_args(1);

            // TODO: do it in async way
            QStatus status = m_proxy.GetProperty(ifaceName.c_str(), propertyName.c_str(), ret_args[0]);
            if (ER_OK == status)
            {
                *val = AJ_toJson(ret_args, meta, 0);
            }

            return String(QCC_StatusText(status));
        }


        /**
         * @brief Set property value.
         */
        String setProperty(const String &ifaceName, const String &propertyName, const json::Value &val)
        {
            const ajn::InterfaceDescription *iface = m_proxy.GetInterface(ifaceName.c_str());
            if (!iface) throw std::runtime_error("no interface found");

            const ajn::InterfaceDescription::Property *prop = iface->GetProperty(propertyName.c_str());
            if (!prop) throw std::runtime_error("no property found");
            if ((prop->access&ajn::PROP_ACCESS_WRITE) == 0)
                throw std::runtime_error("property is not writable");

            MsgArgInfo meta(prop->signature.c_str(), "", propertyName);

            std::cerr << "SET-PROP: " << ifaceName << "." << propertyName
                         << " with \"" << meta.argSign << "\"\n";
            std::vector<ajn::MsgArg> args = AJ_fromJson(val, meta);

            if (args.size() != 1)
                throw std::runtime_error("no value parsed");

            // TODO: do it in async way
            QStatus status = m_proxy.SetProperty(ifaceName.c_str(), propertyName.c_str(), args[0]);
            return String(QCC_StatusText(status));
        }

    public:
        String m_name; // object name
        ajn::ProxyBusObject m_proxy;
        AJ_BusProxyPtr m_pBusProxy;
    };

private:

    struct MsgArgInfo
    {
        String argSign;
        String retSign;

        std::vector<String> names;

        MsgArgInfo(const String &arg_s,
                const String &ret_s,
                const String &names)
            : argSign(arg_s)
            , retSign(ret_s)
        {
            boost::split(this->names, names,
                         boost::is_any_of(","));
        }

        String getArgName(size_t i) const
        {
            if (i < names.size())
                return names[i];

            hive::OStringStream oss;
            oss << "#" << i;
            return oss.str();
        }
    };

    /**
     * @brief Convert JSON to AllJoyn message.
     * @return List of MsgArg.
     *
     * Uses argSign.
     */
    static std::vector<ajn::MsgArg> AJ_fromJson(const json::Value &val, const MsgArgInfo &meta)
    {
        std::vector<ajn::MsgArg> res;

        const String &sign = meta.argSign;
        for (int i = 0; i < sign.size(); ++i)
        {
            String name = meta.getArgName(i);
            const char s = sign[i];
            ajn::MsgArg arg;

            switch (s)
            {
                case 'b': arg.Set("b", val[name].asBool()); break;
                case 'y': arg.Set("y", val[name].asUInt8()); break;
                case 'q': arg.Set("q", val[name].asUInt16()); break;
                case 'n': arg.Set("n", val[name].asInt16()); break;
                case 'u': arg.Set("u", val[name].asUInt32()); break;
                case 'i': arg.Set("i", val[name].asInt32()); break;
                case 't': arg.Set("t", val[name].asUInt64()); break;
                case 'x': arg.Set("x", val[name].asInt64()); break;
                case 'd': arg.Set("d", val[name].asDouble()); break;
                case 's':
                {
                    String str = val[name].asString();
                    arg.Set("s", str.c_str());
                    arg.Stabilize();
                } break;

                case 'a': case 'e':
                case 'r': case 'v':
                case '(': case ')':
                case '{': case '}':
                default:
                {
                    hive::OStringStream oss;
                    oss << "\"" << s << "\" is unsupported signature";
                    throw std::runtime_error(oss.str());
                } break;
            }

            res.push_back(arg);
        }

        return res;
    }


    /**
     * @brief Convert AllJoyn messages to JSON.
     * @return The JSON value.
     *
     * Uses retSign.
     */
    static json::Value AJ_toJson(const std::vector<ajn::MsgArg> &args, const MsgArgInfo &meta, size_t arg_offset = 0)
    {
        json::Value res;

        const String &sign = meta.retSign;
        for (int i = 0; i < sign.size(); ++i)
        {
            String name = meta.getArgName(i+arg_offset);
            ajn::MsgArg arg = (i < args.size()) ? args[i] : ajn::MsgArg();
            const char s = sign[i];

            switch (s)
            {
                case 'b':
                {
                    bool r = false;
                    arg.Get("b", &r);
                    res[name] = r;
                } break;

                case 'y':
                {
                    uint8_t r = 0;
                    arg.Get("y", &r);
                    res[name] = r;
                } break;

                case 'q':
                {
                    uint16_t r = 0;
                    arg.Get("q", &r);
                    res[name] = r;
                } break;

                case 'n':
                {
                    int16_t r = 0;
                    arg.Get("n", &r);
                    res[name] = r;
                } break;

                case 'u':
                {
                    uint32_t r = 0;
                    arg.Get("u", &r);
                    res[name] = r;
                } break;

                case 'i':
                {
                    int32_t r = 0;
                    arg.Get("i", &r);
                    res[name] = r;
                } break;

                case 't':
                {
                    uint64_t r = 0;
                    arg.Get("t", &r);
                    res[name] = r;
                } break;

                case 'x':
                {
                    int64_t r = 0;
                    arg.Get("x", &r);
                    res[name] = r;
                } break;

                case 'd':
                {
                    double r = 0.0;
                    arg.Get("d", &r);
                    res[name] = r;
                } break;

                case 's':
                {
                    char *str = 0;
                    arg.Get("s", &str);
                    res[name] = String(str);
                } break;

                case 'a': case 'e':
                case 'r': case 'v':
                case '(': case ')':
                case '{': case '}':
                default:
                {
                    hive::OStringStream oss;
                    oss << "\"" << s << "\" is unsupported signature";
                    throw std::runtime_error(oss.str());
                } break;
            }
        }

        return res;
    }

private: // remote bus list
    std::vector<AJ_BusProxyPtr> m_bus_proxies;

    /**
     * @brief Get remote bus by name and port.
     */
    AJ_BusProxyPtr getBusProxy(const String &busName, int port)
    {
        const size_t n = m_bus_proxies.size();
        for (size_t i = 0; i < n; ++i)
        {
            AJ_BusProxyPtr p = m_bus_proxies[i];
            if (p->m_name == busName
             && p->m_port == port)
                return p;
        }

        // insert new
        AJ_BusProxyPtr p = AJ_BusProxy::create(m_AJ_bus, busName, port, this);
        m_bus_proxies.push_back(p);
        return p;
    }


    /**
     * @brief Get remote object by name (object path).
     */
    AJ_ObjProxyPtr getObjProxy(AJ_BusProxyPtr pBusProxy, const String &objName)
    {
        const size_t n = pBusProxy->m_obj_proxies.size();
        for (size_t i = 0; i < n; ++i)
        {
            AJ_ObjProxyPtr p = pBusProxy->m_obj_proxies[i];
            if (p->m_name == objName)
                return p;
        }

        // insert new
        AJ_ObjProxyPtr p = AJ_ObjProxy::create(pBusProxy, objName);
        pBusProxy->m_obj_proxies.push_back(p);
        return p;
    }

private:
    bool m_disableWebsockets;       ///< @brief No automatic websocket switch.
    bool m_disableWebsocketPingPong; ///< @brief Disable websocket PING/PONG messages.
    http::ClientPtr m_http;

    devicehive::IDeviceServicePtr m_service; ///< @brief The cloud service.
    devicehive::NetworkPtr m_network;
    devicehive::DevicePtr m_gw_dev;  // gateway device
    bool m_gw_dev_registered;
    String m_lastCommandTimestamp; ///< @brief The timestamp of the last received command.

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



/*
    void sendAnnounceNotification(const AnnounceInfo &info)
    {
        json::Value params;
        params["bus"] = info.busName;
        params["port"] = info.port;

        AJ_BusProxyPtr pBusProxy = getBusProxy(info.busName, info.port);

        typedef std::map<String,AnnounceInfo::InterfaceList>::const_iterator Iterator;
        for (Iterator i = info.objects.begin(); i != info.objects.end(); ++i)
        {
            const String name = i->first;
            AJ_ObjProxyPtr pObjProxy = getObjProxy(pBusProxy, name);
            params["objects"][name]["interfaces"] = pObjProxy->getInterfaces();
        }

        m_service->asyncInsertNotification(m_gw_dev, devicehive::Notification::create("Announce", params));
    }
*/
