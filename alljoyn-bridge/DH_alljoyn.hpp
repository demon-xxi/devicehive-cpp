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
#include <alljoyn/AuthListener.h>

#include <alljoyn/about/AnnounceHandler.h>
#include <alljoyn/about/AnnouncementRegistrar.h>

#include "hexUtils.hpp"
#include <functional>
#include <set>

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
        public ajn::AuthListener,
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
        , m_authPassword("000000")
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

        String baseUrl = "http://alljoyn.pgcloud.devicehive.com/DeviceHive/rest";
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
            else if (boost::algorithm::iequals(argv[i], "--gatewayId") && i+1 < argc)
                gatewayId = argv[++i];
            else if (boost::algorithm::iequals(argv[i], "--gatewayKey") && i+1 < argc)
                gatewayKey = argv[++i];
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

        HIVELOG_TRACE(m_log_AJ, "enabling security");
        status = m_AJ_bus->EnablePeerSecurity("ALLJOYN_ECDHE_NULL ALLJOYN_ECDHE_PSK ALLJOYN_PIN_KEYX ALLJOYN_SRP_KEYX", this);
        AJ_check(status, "failed to enable security");
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
        if (m_gw_dev != device)     // if device's changed
            return;                 // just do nothing

        if (!err)
        {
            HIVELOG_INFO_STR(m_log, "registered, getting data...");
            m_service->asyncGetDeviceData(m_gw_dev);
            m_gw_dev_registered = true;

            sendPendingNotifications();
        }
        else
            handleError(err, "registering device");
    }


    /// @copydoc devicehive::IDeviceServiceEvents::onGetDeviceData()
    virtual void onGetDeviceData(boost::system::error_code err, devicehive::DevicePtr device)
    {
        if (m_gw_dev != device)     // if device's changed
            return;                 // just do nothing

        if (!err)
        {
            HIVELOG_INFO_STR(m_log, "got device data, subscribing for commands...");
            m_service->asyncSubscribeForCommands(m_gw_dev, m_lastCommandTimestamp);

            if (1) // watch announces
            {
                const json::Value j_ann = device->data["announces"];
                for (size_t i = 0; i < j_ann.size(); ++i)
                {
                    const json::Value j_ifaces = j_ann[i];
                    std::vector<String> ifaces;
                    for (size_t j = 0; j < j_ifaces.size(); ++j)
                        ifaces.push_back(j_ifaces[j].asString());
                    if (j_ifaces.isString())
                        ifaces.push_back(j_ifaces.asString());

                    watchAnnounces(ifaces, true);
                }
            }
        }
        else
            handleError(err, "getting device data");
    }

    std::set<const ajn::InterfaceDescription::Member*> m_watchSignals;

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
                else if (cmd_name == "AllJoyn/SetCredentials")
                {
                    m_authUserName = cmd_params["username"].asString();
                    m_authPassword = cmd_params["password"].asString();
                }
                else if (cmd_name == "AllJoyn/WatchAnnounces")
                {
                    const json::Value &j_ifaces = cmd_params;

                    std::vector<String> ifaces;
                    for (size_t i = 0; i < j_ifaces.size(); ++i)
                        ifaces.push_back(j_ifaces[i].asString());
                    if (j_ifaces.isString())
                        ifaces.push_back(j_ifaces.asString());

                    watchAnnounces(ifaces, false);
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
                else if (cmd_name == "AllJoyn/CallMethod"
                      || cmd_name == "AllJoyn/MethodCall")
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
                else if (cmd_name == "AllJoyn/GetProperties")
                {
                    const json::Value &j_addr = cmd_params;

                    AJ_BusProxyPtr pBus = getBusProxy(j_addr["bus"].asString(),
                                                      j_addr["port"].asInt());
                    if (!pBus) throw std::runtime_error("not bus found");

                    AJ_ObjProxyPtr pObj = getObjProxy(pBus, j_addr["object"].asString());
                    if (!pObj) throw std::runtime_error("no object found");

                    String iface = cmd_params["interface"].asString();
                    json::Value properties = cmd_params["properties"];

                    if (properties.isString())
                        command->status = pObj->getProperty(iface, properties.asString(), &command->result);
                    else if (properties.isArray())
                    {
                        String status;
                        json::Value result;
                        for (size_t i = 0; i < properties.size(); ++i)
                        {
                            String name = properties[i].asString();

                            if (i) status += ", ";
                            status += pObj->getProperty(iface, name, &result[name]);
                        }
                        command->status = status;
                        command->result = result;
                    }
                    else
                        throw std::runtime_error("unknown property to get");
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
                else if (cmd_name == "AllJoyn/SetProperties")
                {
                    const json::Value &j_addr = cmd_params;

                    AJ_BusProxyPtr pBus = getBusProxy(j_addr["bus"].asString(),
                                                      j_addr["port"].asInt());
                    if (!pBus) throw std::runtime_error("not bus found");

                    AJ_ObjProxyPtr pObj = getObjProxy(pBus, j_addr["object"].asString());
                    if (!pObj) throw std::runtime_error("no object found");

                    String iface = cmd_params["interface"].asString();
                    json::Value properties = cmd_params["properties"];

                    if (!properties.isObject())
                        throw std::runtime_error("invalid request, not an object");

                    json::Value::MemberIterator i, b = properties.membersBegin();
                    String status;
                    for (i = b; i != properties.membersEnd(); ++i)
                    {
                        String name = i->first;
                        json::Value val = i->second;

                        if (i != b) status += ", ";
                        status += pObj->setProperty(iface, name, val);
                    }

                    command->status = status;
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

                    if (m_watchSignals.insert(s).second)
                    {
                        QStatus status = m_AJ_bus->RegisterSignalHandler(this, (MessageReceiver::SignalHandler)&This::onSignalHandler, s, obj.empty() ? NULL : obj.c_str());
                        AJ_check(status, "failed to register signal handler");
                    }
                    else
                    {
                        command->result = "Already exists";
                    }
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

                    m_watchSignals.erase(s);
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

private: // AuthListener interface

    virtual bool RequestCredentials(const char* authMechanism, const char* peerName, uint16_t authCount, const char* userName, uint16_t credMask, Credentials& credentials)
    {
        HIVELOG_INFO(m_log_AJ, "RequestCredentials #" << authCount << " mechanism:\"" << authMechanism
                     << "\", peer:\"" << peerName << "\", user:\"" << userName << "\", mask:" << hive::dump::hex(credMask));
        bool res = false;

        if (credMask&CRED_PASSWORD /*&& !m_authPassword.empty()*/)
        {
            const char *psw = m_authPassword.c_str();
            credentials.SetPassword(psw);
            HIVELOG_INFO(m_log_AJ, "\tset password: \"" << psw << "\"");
            res = true;
        }
        if (credMask&CRED_USER_NAME /*&& !m_authUserName.empty()*/)
        {
            credentials.SetUserName(m_authUserName.c_str());
            HIVELOG_INFO(m_log_AJ, "\tset username: \"" << m_authUserName << "\"");
            res = true;
        }

        return res;
    }

    virtual void SecurityViolation(QStatus status, const ajn::Message& msg)
    {
        HIVELOG_WARN(m_log_AJ, "security violation:" << QCC_StatusText(status) << msg->ToString().c_str());
    }

    virtual void AuthenticationComplete(const char* authMechanism, const char* peerName, bool success)
    {
        HIVELOG_INFO(m_log_AJ, "AuthenticationComplete mechanism:\"" << authMechanism
                     << "\", peer:\"" << peerName << "\", "
                     << (success ? "SUCCESS":"FAILED"));
    }

private: // ajn::services::AnnounceHandler

    /**
     * @brief Compare string and JSON value.
     */
    static bool stringEqual(const String &a, const json::Value &b)
    {
        return b.isString() && a == b.asString();
    }


    /**
     * @brief Insert interface list.
     * @param ifaces Interface list to insert
     * @return `true` if need to update device data.
     */
    bool _insertWatchAnnounce(const std::vector<String> &ifaces)
    {
        json::Value& ann_list = m_gw_dev->data["announces"];

        // find existing
        for (size_t i = 0; i < ann_list.size(); ++i)
        {
            json::Value const& ann = ann_list[i]; // list of interfaces
            if ((ann.size() == ifaces.size()) && std::equal(ifaces.begin(), ifaces.end(), ann.elementsBegin(), &This::stringEqual))
                return false; // already exists
        }

        // not found, need to insert new
        json::Value ann(json::Value::TYPE_ARRAY);
        for (size_t i = 0; i < ifaces.size(); ++i)
            ann.append(ifaces[i]);
        ann_list.append(ann);

        return true; // created
    }


    /**
     * @brief Remove interface list.
     * @param ifaces Interface list to remove.
     * @return `true` if need to update device data.
     */
    bool _removeWatchAnnounce(const std::vector<String> &ifaces)
    {
        json::Value& ann_list = m_gw_dev->data["announces"];

        // find existing
        for (size_t i = 0; i < ann_list.size(); ++i)
        {
            json::Value const& ann = ann_list[i]; // list of interfaces
            if ((ann.size() == ifaces.size()) && std::equal(ifaces.begin(), ifaces.end(), ann.elementsBegin(), &This::stringEqual))
            {
                ann_list.remove(i);
                return true; // deleted
            }
        }

        return false; // nothing to remove
    }


    /**
     * @brief Watch for Announce signals.
     * @param ifaces The list of supported interfaces.
     *      Empty for all interfaces.
     */
    void watchAnnounces(const std::vector<String> &ifaces, bool forceToWatch)
    {
        if (!forceToWatch && !_insertWatchAnnounce(ifaces))
            return; // already watched

        if (m_service && m_gw_dev && !forceToWatch)
            m_service->asyncUpdateDeviceData(m_gw_dev);

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
        if (!_removeWatchAnnounce(ifaces))
            return; // doesn't exist

        if (m_service && m_gw_dev)
            m_service->asyncUpdateDeviceData(m_gw_dev);

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
        typedef std::map<String, InterfaceList> ObjectsInfo;
        ObjectsInfo objects;


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

        m_ios.post(boost::bind(&This::inspectRemoteBus, shared_from_this(), info));
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
                            << meta.retSign << "\"\n";
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

            std::cerr << "GET-PROP: " << ifaceName << "." << propertyName
                         << " with \"" << prop->signature.c_str() << "\"\n";
            ajn::MsgArg ret;

            // TODO: do it in async way
            QStatus status = m_proxy.GetProperty(ifaceName.c_str(), propertyName.c_str(), ret);
            if (ER_OK == status)
            {
                size_t sign_pos = 0;
                *val = AJ_toJson1(ret, prop->signature.c_str(), sign_pos);
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

            std::cerr << "SET-PROP: " << ifaceName << "." << propertyName
                         << " with \"" << prop->signature.c_str() << "\"\n";
            size_t sign_pos = 0;
            ajn::MsgArg arg = AJ_fromJson1(val, prop->signature.c_str(), sign_pos);

            // TODO: do it in async way
            QStatus status = m_proxy.SetProperty(ifaceName.c_str(), propertyName.c_str(), arg);
            return String(QCC_StatusText(status));
        }

    public:
        String m_name; // object name
        ajn::ProxyBusObject m_proxy;
        AJ_BusProxyPtr m_pBusProxy;
    };

private:
    std::map<String, AJ_BusProxyPtr> m_busByDevId; // [dev_id] = bus
    std::map<String, AJ_BusProxyPtr> m_busByDevName; // [dev_name] = bus

    /**
     * @brief Inspect remote bus for object identifiers.
     */
    void inspectRemoteBus(const AnnounceInfo &info)
    {
        typedef AnnounceInfo::ObjectsInfo::const_iterator Iterator;
        for (Iterator i = info.objects.begin(); i != info.objects.end(); ++i)
        {
            const String &objectName = i->first;
            const AnnounceInfo::InterfaceList &ifaces = i->second;

            if (std::find(ifaces.begin(), ifaces.end(), String("org.alljoyn.About")) != ifaces.end())
            {
                HIVELOG_DEBUG(m_log, "inspecting \"" << objectName << "\" object");

                ajn::services::AboutClient client(*m_AJ_bus);
                AJ_BusProxyPtr pbus = getBusProxy(info.busName, info.port);
                ajn::services::AboutClient::AboutData about_data;
                QStatus status = client.GetAboutData(info.busName.c_str(), "", about_data, pbus->m_sessionId);
                if (status == ER_OK)
                {
                    ajn::MsgArg dev_id_arg = about_data["DeviceId"];
                    ajn::MsgArg dev_name_arg = about_data["DeviceName"];

                    const char *dev_id = 0;
                    dev_id_arg.Get("s", &dev_id);
                    if (dev_id)
                    {
                        HIVELOG_INFO(m_log, "map deviceId:\"" << dev_id << "\" to bus:\"" << info.busName << "\", port:" << info.port);

                        // add device identifier to the cache
                        m_busByDevId[String(dev_id)] = pbus;
                    }

                    const char *dev_name = 0;
                    dev_name_arg.Get("s", &dev_name);
                    if (dev_name)
                    {
                        HIVELOG_INFO(m_log, "map deviceName:\"" << dev_name << "\" to bus:\"" << info.busName << "\", port:" << info.port);

                        // add device identifier to the cache
                        m_busByDevName[String(dev_name)] = pbus;
                    }
                }
            }
        }
    }


    /**
     * @brief Find bus proxy by device identifier.
     * @param devId The device identifier.
     * @return The bus proxy or NULL.
     */
    AJ_BusProxyPtr findBusProxyByDevId(const String &devId) const
    {
        std::map<String, AJ_BusProxyPtr>::const_iterator i = m_busByDevId.find(devId);
        return (i != m_busByDevId.end()) ? i->second : AJ_BusProxyPtr();
    }

    /**
     * @brief Find bus proxy by device name.
     * @param devName The device name.
     * @return The bus proxy or NULL.
     */
    AJ_BusProxyPtr findBusProxyByDevName(const String &devName) const
    {
        std::map<String, AJ_BusProxyPtr>::const_iterator i = m_busByDevName.find(devName);
        return (i != m_busByDevName.end()) ? i->second : AJ_BusProxyPtr();
    }

public:

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


    static ajn::MsgArg AJ_fromJson0(const json::Value &arg)
    {
        if (arg.isNull())
            return ajn::MsgArg();

        else if (arg.isBool())
            return ajn::MsgArg("b", arg.asBool());

        else if (arg.isInteger())
            return ajn::MsgArg("x", arg.asInt());

        else if (arg.isDouble())
            return ajn::MsgArg("d", arg.asDouble());

        else if (arg.isString())
        {
            String s = arg.asString();
            ajn::MsgArg msg("s", s.c_str());
            msg.Stabilize();
            return msg;
        }

        hive::OStringStream oss;
        oss << "\"" << arg << "\" cannot convert to MsgArg";
        throw std::runtime_error(oss.str());
    }


    /**
     */
    static ajn::MsgArg AJ_fromJson1(const json::Value &val, const String &signature, size_t &sign_pos)
    {
        if (signature.empty())
            throw std::runtime_error("not signature provided");

        ajn::MsgArg res;
        size_t pos_inc = 1;
        switch (signature[sign_pos])
        {
            case 'b': res.Set("b", val.asBool()); break;
            case 'y': res.Set("y", val.asUInt8()); break;
            case 'q': res.Set("q", val.asUInt16()); break;
            case 'n': res.Set("n", val.asInt16()); break;
            case 'u': res.Set("u", val.asUInt32()); break;
            case 'i': res.Set("i", val.asInt32()); break;
            case 't': res.Set("t", val.asUInt64()); break;
            case 'x': res.Set("x", val.asInt64()); break;
            case 'd': res.Set("d", val.asDouble()); break;

            case 's':
            {
                String str = val.asString();
                res.Set("s", str.c_str());
                res.Stabilize();
            } break;

            case 'o':
            {
                String str = val.asString();
                res.Set("o", str.c_str());
                res.Stabilize();
            } break;

            case 'g':
            {
                String str = val.asString();
                res.Set("g", str.c_str());
                res.Stabilize();
            } break;

            case 'v': // variant
            {
                ajn::MsgArg v = AJ_fromJson0(val);
                res.Set("v", &v);
                res.Stabilize();
            } break;

            case 'a':
            {
                String elem = AJ_elementSignature(signature, sign_pos+1);
                if (elem.empty())
                    throw std::runtime_error("unknown element signature");
                sign_pos += elem.size();

                const size_t n = val.size();
                if (elem == "b")
                {
                    bool *v = new bool[n];
                    for (size_t i = 0; i < n; ++i)
                        v[i] = val[i].asBool();
                    res.Set("ab", n, v);
                    res.Stabilize();
                    delete[] v;
                }
                else if (elem == "y")
                {
                    std::vector<u_int8_t> v(n);
                    for (size_t i = 0; i < n; ++i)
                        v[i] = val[i].asUInt8();
                    res.Set("ay", n, n ? &v[0] : 0);
                    res.Stabilize();
                }
                else if  (elem == "q")
                {
                    std::vector<u_int16_t> v(n);
                    for (size_t i = 0; i < n; ++i)
                        v[i] = val[i].asUInt16();
                    res.Set("aq", n, n ? &v[0] : 0);
                    res.Stabilize();
                }
                else if (elem == "n")
                {
                    std::vector<int16_t> v(n);
                    for (size_t i = 0; i < n; ++i)
                        v[i] = val[i].asInt16();
                    res.Set("an", n, n ? &v[0] : 0);
                    res.Stabilize();
                }
                else if (elem == "u")
                {
                    std::vector<u_int32_t> v(n);
                    for (size_t i = 0; i < n; ++i)
                        v[i] = val[i].asUInt32();
                    res.Set("au", n, n ? &v[0] : 0);
                    res.Stabilize();
                }
                else if (elem == "i")
                {
                    std::vector<int32_t> v(n);
                    for (size_t i = 0; i < n; ++i)
                        v[i] = val[i].asInt32();
                    res.Set("ai", n, n ? &v[0] : 0);
                    res.Stabilize();
                }
                else if (elem == "t")
                {
                    std::vector<u_int64_t> v(n);
                    for (size_t i = 0; i < n; ++i)
                        v[i] = val[i].asUInt64();
                    res.Set("at", n, n ? &v[0] : 0);
                    res.Stabilize();
                }
                else if (elem == "x")
                {
                    std::vector<int64_t> v(n);
                    for (size_t i = 0; i < n; ++i)
                        v[i] = val[i].asInt64();
                    res.Set("ax", n, n ? &v[0] : 0);
                    res.Stabilize();
                }
                else if (elem == "d")
                {
                    std::vector<double> v(n);
                    for (size_t i = 0; i < n; ++i)
                        v[i] = val[i].asDouble();
                    res.Set("ad", n, n ? &v[0] : 0);
                    res.Stabilize();
                }
                else if (elem == "s")
                {
                    std::vector<String> vv(n);
                    std::vector<const char*> v(n);
                    for (size_t i = 0; i < n; ++i)
                    {
                        vv[i] = val[i].asString();
                        v[i] = vv[i].c_str();
                    }
                    res.Set("as", n, n ? &v[0] : 0);
                    res.Stabilize();
                }
                else if (elem == "o")
                {
                    std::vector<String> vv(n);
                    std::vector<const char*> v(n);
                    for (size_t i = 0; i < n; ++i)
                    {
                        vv[i] = val[i].asString();
                        v[i] = vv[i].c_str();
                    }
                    res.Set("ao", n, n ? &v[0] : 0);
                    res.Stabilize();
                }
                else if (elem == "g")
                {
                    std::vector<String> vv(n);
                    std::vector<const char*> v(n);
                    for (size_t i = 0; i < n; ++i)
                    {
                        vv[i] = val[i].asString();
                        v[i] = vv[i].c_str();
                    }
                    res.Set("ag", n, n ? &v[0] : 0);
                    res.Stabilize();
                }
                else if (elem[0] == '{' && elem[elem.size()-1] == '}')
                {
                    std::vector<ajn::MsgArg> v(n);
                    json::Value::MemberIterator i = val.membersBegin();
                    for (size_t k = 0; i != val.membersEnd(); ++i, ++k)
                    {
                        String      d_key = i->first;
                        json::Value d_val = i->second;

                        size_t p = 1;
                        ajn::MsgArg a_key = AJ_fromJson1(d_key, elem, p); p += 1;
                        ajn::MsgArg a_val = AJ_fromJson1(d_val, elem, p);

                        v[k].Set("{**}", &a_key, &a_val);
                    }

                    res.Set(("a" + elem).c_str(), n, n ? &v[0] : 0);
                    res.Stabilize();
                }
                else
                {
                    hive::OStringStream oss;
                    oss << "\"" << elem << "\" is unsupported element signature";
                    throw std::runtime_error(oss.str());
                }
            } break;

            case 'e':
            case 'r':
            case '(': case ')':
            case '{': case '}':
            default:
            {
                hive::OStringStream oss;
                oss << "\"" << signature << "\" is unsupported signature";
                throw std::runtime_error(oss.str());
            } break;
        }

        //sign_pos += pos_inc;
        return res;
    }


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
        for (size_t i = 0, k = 0; i < sign.size(); ++i, ++k)
        {
            String name = meta.getArgName(k);
            ajn::MsgArg arg = AJ_fromJson1(val[name], sign, i);
            res.push_back(arg);
        }

        return res;
    }

    static json::Value AJ_toJson0(const ajn::MsgArg *arg)
    {
        if (!arg)
            return json::Value::null();

        switch (arg->typeId)
        {
            case ajn::ALLJOYN_INVALID:
                return json::Value::null();

            case ajn::ALLJOYN_BOOLEAN:
                return json::Value(arg->v_bool);

            case ajn::ALLJOYN_DOUBLE:
                return json::Value(arg->v_double);

            case ajn::ALLJOYN_SIGNATURE:
                return json::Value(String(arg->v_signature.sig, arg->v_signature.len));

            case ajn::ALLJOYN_INT32:
                return json::Value(arg->v_int32);

            case ajn::ALLJOYN_INT16:
                return json::Value(arg->v_int16);

            case ajn::ALLJOYN_OBJECT_PATH:
                return json::Value(String(arg->v_objPath.str, arg->v_objPath.len));

            case ajn::ALLJOYN_UINT16:
                return json::Value(arg->v_uint16);

            case ajn::ALLJOYN_STRING:
                return json::Value(String(arg->v_objPath.str, arg->v_objPath.len));

            case ajn::ALLJOYN_UINT64:
                return json::Value(arg->v_uint64);

            case ajn::ALLJOYN_UINT32:
                return json::Value(arg->v_uint32);

            case ajn::ALLJOYN_INT64:
                return json::Value(arg->v_int64);

            case ajn::ALLJOYN_BYTE:
                return json::Value(arg->v_byte);

//            case ajn::ALLJOYN_BOOLEAN_ARRAY:
//            case ajn::ALLJOYN_DOUBLE_ARRAY:
//            case ajn::ALLJOYN_INT32_ARRAY:
//            case ajn::ALLJOYN_INT16_ARRAY:
//            case ajn::ALLJOYN_UINT16_ARRAY:
//            case ajn::ALLJOYN_UINT64_ARRAY:
//            case ajn::ALLJOYN_UINT32_ARRAY:
//            case ajn::ALLJOYN_INT64_ARRAY:
//            case ajn::ALLJOYN_BYTE_ARRAY:

            default:
                hive::OStringStream oss;
                oss << "\"" << arg->ToString().c_str() << "\" cannot convert to JSON";
                throw std::runtime_error(oss.str());
        }
    }

    /**
     */
    static json::Value AJ_toJson1(const ajn::MsgArg &arg, const String &signature, size_t &sign_pos)
    {
        if (signature.empty())
            throw std::runtime_error("no signature provided");
        json::Value res;

        size_t pos_inc = 1;
        switch (signature[sign_pos])
        {
            case 'b':
            {
                bool r = false;
                arg.Get("b", &r);
                res = r;
            } break;

            case 'y':
            {
                uint8_t r = 0;
                arg.Get("y", &r);
                res = r;
            } break;

            case 'q':
            {
                uint16_t r = 0;
                arg.Get("q", &r);
                res = r;
            } break;

            case 'n':
            {
                int16_t r = 0;
                arg.Get("n", &r);
                res = r;
            } break;

            case 'u':
            {
                uint32_t r = 0;
                arg.Get("u", &r);
                res = r;
            } break;

            case 'i':
            {
                int32_t r = 0;
                arg.Get("i", &r);
                res = r;
            } break;

            case 't':
            {
                uint64_t r = 0;
                arg.Get("t", &r);
                res = r;
            } break;

            case 'x':
            {
                int64_t r = 0;
                arg.Get("x", &r);
                res = r;
            } break;

            case 'd':
            {
                double r = 0.0;
                arg.Get("d", &r);
                res = r;
            } break;

            case 's':
            {
                char *str = 0;
                arg.Get("s", &str);
                res = String(str);
            } break;

            case 'o':
            {
                char *str = 0;
                arg.Get("o", &str);
                res = String(str);
            } break;

            case 'g':
            {
                char *str = 0;
                arg.Get("g", &str);
                res = String(str);
            } break;

            case 'a':
            {
                String elem = AJ_elementSignature(signature, sign_pos+1);
                if (elem.empty())
                    throw std::runtime_error("unknown element signature");
                sign_pos += elem.size();

                if (elem == "b")
                {
                    size_t n = 0;
                    bool *v = 0;
                    arg.Get("ab", &n, &v);
                    res.resize(n);
                    for (size_t i = 0; i < n; ++i)
                        res[i] = v[i];
                }
                else if (elem == "y")
                {
                    size_t n = 0;
                    uint8_t *v = 0;
                    arg.Get("ay", &n, &v);
                    res.resize(n);
                    for (size_t i = 0; i < n; ++i)
                        res[i] = v[i];
                }
                else if  (elem == "q")
                {
                    size_t n = 0;
                    uint16_t *v = 0;
                    arg.Get("aq", &n, &v);
                    res.resize(n);
                    for (size_t i = 0; i < n; ++i)
                        res[i] = v[i];
                }
                else if (elem == "n")
                {
                    size_t n = 0;
                    int16_t *v = 0;
                    arg.Get("an", &n, &v);
                    res.resize(n);
                    for (size_t i = 0; i < n; ++i)
                        res[i] = v[i];
                }
                else if (elem == "u")
                {
                    size_t n = 0;
                    uint32_t *v = 0;
                    arg.Get("au", &n, &v);
                    res.resize(n);
                    for (size_t i = 0; i < n; ++i)
                        res[i] = v[i];
                }
                else if (elem == "i")
                {
                    size_t n = 0;
                    int32_t *v = 0;
                    arg.Get("ai", &n, &v);
                    res.resize(n);
                    for (size_t i = 0; i < n; ++i)
                        res[i] = v[i];
                }
                else if (elem == "t")
                {
                    size_t n = 0;
                    uint64_t *v = 0;
                    arg.Get("at", &n, &v);
                    res.resize(n);
                    for (size_t i = 0; i < n; ++i)
                        res[i] = v[i];
                }
                else if (elem == "x")
                {
                    size_t n = 0;
                    int64_t *v = 0;
                    arg.Get("ax", &n, &v);
                    res.resize(n);
                    for (size_t i = 0; i < n; ++i)
                        res[i] = v[i];
                }
                else if (elem == "d")
                {
                    size_t n = 0;
                    double *v = 0;
                    arg.Get("ad", &n, &v);
                    res.resize(n);
                    for (size_t i = 0; i < n; ++i)
                        res[i] = v[i];
                }
                else if (elem == "s")
                {
                    size_t n = 0;
                    ajn::MsgArg *v = 0;
                    arg.Get("as", &n, &v);
                    res.resize(n);
                    for (size_t i = 0; i < n; ++i)
                    {
                        char *s = 0;
                        v[i].Get("s", &s);
                        res[i] = String(s);
                    }
                }
                else if (elem == "o")
                {
                    size_t n = 0;
                    ajn::MsgArg *v = 0;
                    arg.Get("ao", &n, &v);
                    res.resize(n);
                    for (size_t i = 0; i < n; ++i)
                    {
                        char *s = 0;
                        v[i].Get("o", &s);
                        res[i] = String(s);
                    }
                }
                else if (elem == "g")
                {
                    size_t n = 0;
                    ajn::MsgArg *v = 0;
                    arg.Get("ag", &n, &v);
                    res.resize(n);
                    for (size_t i = 0; i < n; ++i)
                    {
                        char *s = 0;
                        v[i].Get("g", &s);
                        res[i] = String(s);
                    }
                }
                else if (elem[0] == '{' && elem[elem.size()-1] == '}') // dictionary entries
                {
                    size_t n = 0;
                    ajn::MsgArg *v = 0;
                    arg.Get(("a" + elem).c_str(), &n, &v);
                    for (size_t i = 0; i < n; ++i)
                    {
                        ajn::MsgArg *dk = 0;
                        ajn::MsgArg *dv = 0;
                        v[i].Get("{**}", &dk, &dv);

                        json::Value kk = AJ_toJson0(dk);
                        json::Value vv = AJ_toJson0(dv);

                        std::cerr << "AJ->json: " << dv->typeId << " " << dv->ToString().c_str() << " = " << vv << "\n";

                        res[kk.asString()] = vv;
                    }
                }
                else
                {
                    hive::OStringStream oss;
                    oss << "\"" << elem << "\" is unsupported element signature";
                    throw std::runtime_error(oss.str());
                }
            } break;

//            case '(': // structure
//            {
//                String elem = AJ_elementSignature(signature, sign_pos);
//                if (elem.empty())
//                    throw std::runtime_error("unknown element signature");
//                sign_pos += elem.size();

//                return AJ_toJson(std::vector<ajn::MsgArg>(1, arg), MsgArgInfo(elem.substr(1, elem.size()-1), "", "xxx"));
//            } break;

            case 'e':
            case 'r': case 'v':
            case '(': case ')':
            case '{': case '}':
            default:
            {
                hive::OStringStream oss;
                oss << "\"" << signature << "\" is unsupported signature";
                throw std::runtime_error(oss.str());
            } break;
        }

        //sign_pos += pos_inc;
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
        for (size_t i = 0, k = 0; i < sign.size(); ++i, ++k)
        {
            String name = meta.getArgName(k+arg_offset);
            ajn::MsgArg arg = (k < args.size()) ? args[k] : ajn::MsgArg();
            res[name] = AJ_toJson1(arg, sign, i);
        }

        return res;
    }


    static String AJ_elementSignature(const String &sign, size_t i)
    {
        size_t n = 1;

        if (sign[i] == '(')
        {
            int deep = 1;
            for (size_t k = i+1; k < sign.size(); ++k)
            {
                const int s = sign[k];
                if (s == 'a')
                {
                    String ss = AJ_elementSignature(sign, k+1);
                    k += ss.size();
                }
                else if (s == '(')
                    ++deep;
                else if (s == ')' && --deep == 0)
                {
                    n = k-i+1;
                    break;
                }
            }
        }
        else if (sign[i] == '{')
        {
            int deep = 1;
            for (size_t k = i+1; k < sign.size(); ++k)
            {
                const int s = sign[k];
                if (s == 'a')
                {
                    String ss = AJ_elementSignature(sign, k+1);
                    k += ss.size();
                }
                else if (s == '{')
                    ++deep;
                else if (s == '}' && --deep == 0)
                {
                    n = k-i+1;
                    break;
                }
            }
        }

        return sign.substr(i, n);
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
    String m_authPassword;
    String m_authUserName;

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

    if (0) // json <-> MsgArg conversion
    {
        std::cerr << Application::AJ_elementSignature("auxxx", 1) << "\n";
        std::cerr << Application::AJ_elementSignature("asxxx", 1) << "\n";
        std::cerr << Application::AJ_elementSignature("a(ss)xxx", 1) << "\n";
        std::cerr << Application::AJ_elementSignature("xa{sv}xxx", 2) << "\n";
        std::cerr << Application::AJ_elementSignature("(uasu)xxx", 0) << "\n";
        std::cerr << "\n\n";
    }

    if (0)
    {
        ajn::MsgArg msg;
        const char* v[] = {"Hello","World"};
        msg.Set("(uasu)", 111, 2, v, 222);
        msg.Stabilize();
        std::cerr << msg.ToString().c_str() << "\n\n";

        Application::MsgArgInfo meta("", "(uasu)", "v1,v2,v3");
        json::Value j_val = Application::AJ_toJson(std::vector<ajn::MsgArg>(1, msg), meta);
        std::cerr << j_val << "\n\n";

        Application::MsgArgInfo meta2 = meta;
        meta2.argSign = meta2.retSign;
        std::vector<ajn::MsgArg> msg2 = Application::AJ_fromJson(j_val, meta2);
        for (size_t i = 0; i < msg2.size(); ++i)
            std::cerr << msg2[i].ToString().c_str() << "\n\n";
    }

    if (0)
    {
        ajn::MsgArg dict[3];
        dict[0].Set("{is}", 1, "first");
        dict[1].Set("{is}", 2, "second");
        dict[2].Set("{is}", 3, "third");

        ajn::MsgArg msg;
        msg.Set("a{is}", 3, dict);
        msg.Stabilize();
        std::cerr << msg.ToString().c_str() << "\n\n";

        Application::MsgArgInfo meta("", "a{is}", "v1,v2,v3");
        json::Value j_val = Application::AJ_toJson(std::vector<ajn::MsgArg>(1, msg), meta);
        std::cerr << j_val << "\n\n";

        Application::MsgArgInfo meta2 = meta;
        meta2.argSign = meta2.retSign;
        std::vector<ajn::MsgArg> msg2 = Application::AJ_fromJson(j_val, meta2);
        for (size_t i = 0; i < msg2.size(); ++i)
            std::cerr << msg2[i].ToString().c_str() << "\n\n";
    }

    if (0)
    {
        Application::MsgArgInfo meta("", "a{sv}", "v1,v2,v3");
        json::Value j_val = json::fromStr("{\"v1\": { \"a\":1, \"b\":2, \"c\":3 }}");
        std::cerr << j_val << "\n\n";

        Application::MsgArgInfo meta2 = meta;
        meta2.argSign = meta2.retSign;
        std::vector<ajn::MsgArg> msg2 = Application::AJ_fromJson(j_val, meta2);
        for (size_t i = 0; i < msg2.size(); ++i)
            std::cerr << msg2[i].ToString().c_str() << "\n\n";

        return;
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
