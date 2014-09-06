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
#include <alljoyn/BusObject.h>
#include <alljoyn/InterfaceDescription.h>
#include <alljoyn/DBusStd.h>
#include <alljoyn/AllJoynStd.h>

#include "hexUtils.hpp"
#include <functional>

// constants
static const char* SERVICE_INTERFACE_NAME = "com.devicehive.samples.alljoyn.serial";
static const char* SERVICE_OBJECT_PATH = "/serialService";
static const char* FROM_GW_SIGNAL_NAME = "dataFromGw";
static const char* TO_GW_SIGNAL_NAME = "dataToGw";
static const char* BUS_NAME = "DH_AJ";
static const uint32_t LINK_TIMEOUT = 20;
static const ajn::SessionPort SERVICE_PORT = 27;

namespace DH_alljoyn
{
    using namespace hive;

/// @brief Various contants and timeouts.
enum Timeouts
{
    SERVER_RECONNECT_TIMEOUT    = 10000, ///< @brief Try to open server connection each X milliseconds.
    RETRY_TIMEOUT               = 5000,  ///< @brief Common retry timeout, milliseconds.
    IDLE_TIMEOUT_SEC            = 10, ///< @brief IDLE timeout to send registration request.
    DEVICE_OFFLINE_TIMEOUT      = 0
};


/**
 * @brief Check AllJoyn status code and throw an exception if it's not ER_OK.
 */
inline void checkAllJoynStatus(QStatus status, const char *text)
{
    if (ER_OK != status)
    {
        std::ostringstream ess;
        ess << text << ": " << QCC_StatusText(status);
        throw std::runtime_error(ess.str());
    }
}


class AJ_SessionEvents
{
public:
    virtual ~AJ_SessionEvents() {}

    virtual void onGotFrame(gateway::Frame::SharedPtr frame,
                            const hive::String &from,
                            ajn::SessionId sessionId) = 0;
};


/**
 * @brief The simple gateway application.
 *
 * This application controls multiple devices connected via AllJoyn!
 */
class Application:
    public basic_app::Application,
        public ajn::BusListener,
        public ajn::SessionPortListener,
        public ajn::SessionListener,
        public AJ_SessionEvents
{
    typedef basic_app::Application Base; ///< @brief The base type.
    typedef Application This; ///< @brief The type alias.

private:

    /**
     * @brief The alljoyn session object.
     */
    class AJ_Session:
        public ajn::BusObject
    {
    public:

        /**
         * @brief The main constuctor.
         */
        AJ_Session(AJ_SessionEvents *events, ajn::BusAttachment& bus, const hive::String &path)
            : ajn::BusObject(path.c_str())
            , m_fromGwSignal(0)
            , m_toGwSignal(0)
            , m_log("/AllJoyn/Session/" + path)
            , m_events(events)
        {
            // get interface to this object
            const ajn::InterfaceDescription *iface = bus.GetInterface(SERVICE_INTERFACE_NAME);
            if (!iface) throw std::runtime_error("no interface found");
            AddInterface(*iface);

            // store the signal members away so it can be quickly looked up when signals are sent
            m_fromGwSignal = iface->GetMember(FROM_GW_SIGNAL_NAME);
            if (!m_fromGwSignal) throw std::runtime_error("no FromGw signal found");
            m_toGwSignal = iface->GetMember(TO_GW_SIGNAL_NAME);
            if (!m_toGwSignal) throw std::runtime_error("no ToGw signal found");

            // register signal handler
            QStatus status =  bus.RegisterSignalHandler(this,
                    static_cast<ajn::MessageReceiver::SignalHandler>(&AJ_Session::gotData),
                    m_toGwSignal, NULL);
            checkAllJoynStatus(status, "failed to register AllJoyn signal handler");

            // register bus object
            status = bus.RegisterBusObject(*this);
            checkAllJoynStatus(status, "failed to register AllJoyn bus object");

            HIVELOG_TRACE(m_log, "created (path:" << path << ")");
        }


        ~AJ_Session()
        {
            HIVELOG_TRACE_STR(m_log, "deleted");
        }


        void stop(ajn::BusAttachment& bus)
        {
            HIVELOG_TRACE_STR(m_log, "unregistering bus object");
            bus.UnregisterBusObject(*this);
            //checkAllJoynStatus(status, "failed to unregister AllJoyn bus object");

            HIVELOG_TRACE_STR(m_log, "unregistering all handlers");
            QStatus status = bus.UnregisterAllHandlers(this);
            checkAllJoynStatus(status, "failed to unregister AllJoyn signal handlers");

            HIVELOG_TRACE_STR(m_log, "stopped");
        }


        /**
         * @brief Send frame to AllJoyn service.
         */
        QStatus sendFrame(gateway::Frame::SharedPtr frame, String const& dest, ajn::SessionId sessionId)
        {
            if (!sessionId)
            {
                HIVELOG_WARN(m_log, "no session id, ignore frame #" << frame->getIntent());
                return ER_FAIL;
            }

            String payload; frame->getPayload(payload);
            const String data_hex = utils::toHex(payload);

            ajn::MsgArg msg_arg[2];
            msg_arg[0] = ajn::MsgArg("i", frame->getIntent());
            msg_arg[1] = ajn::MsgArg("s", data_hex.c_str());
            uint8_t flags = 0;

            QStatus res = Signal(dest.c_str(), sessionId, *m_fromGwSignal, &msg_arg[0], 2, 0, flags);
            HIVELOG_DEBUG(m_log, "send frame: #" << frame->getIntent()
                << " \"" << utils::lim(data_hex, 32) << "\" to \""
                << dest << "\", session:" << sessionId << " (status: " << res << ")");

            return res;
        }


        /**
         * @brief Got signal from AllJoyn service
         */
        void gotData(const ajn::InterfaceDescription::Member*, const char* srcPath, ajn::Message& msg)
        {
            const int intent = msg->GetArg(0)->v_int32;
            const String data_hex = msg->GetArg(1)->v_string.str;
            HIVELOG_DEBUG(m_log, "recv frame: #" << intent
                        << " \"" << utils::lim(data_hex, 32) << "\" from "
                        << (srcPath ? srcPath : "<null>")
                        << " session_id:" << msg->GetSessionId()
                        << " sender:" << msg->GetSender());

            if (m_events)
            {
                m_events->onGotFrame(gateway::Frame::create(intent, utils::fromHex(data_hex)),
                                     hive::String(msg->GetSender()),
                                     msg->GetSessionId());
            }
        }

    private:
        const ajn::InterfaceDescription::Member *m_fromGwSignal;
        const ajn::InterfaceDescription::Member *m_toGwSignal;
        hive::log::Logger m_log;
        AJ_SessionEvents *m_events;
    };


    /**
     * A session object.
     */
    class Session:
        public boost::enable_shared_from_this<Session>,
        public devicehive::IDeviceServiceEvents
    {
    protected:
        Session(Application *app, String const& name)
            : m_name(name)
            , m_app(app)
            , m_webTout(0)
            , m_http_major(1)
            , m_http_minor(0)
            , m_pendingReconnect(false)
            , m_deviceRegistered(false)
            , m_idleTimer(app->m_ios)
            , m_sessionId(0)
            , m_log("/Session/" + name)
        {
            HIVELOG_TRACE_STR(m_log, "created");
        }

    public:

        ~Session()
        {
            HIVELOG_TRACE_STR(m_log, "deleted");
        }

        /**
         * @brief Get session identifier reference.
         */
        ajn::SessionId& sessionRef()
        {
            return m_sessionId;
        }

    public:
        typedef boost::shared_ptr<Session> SharedPtr;

        static SharedPtr create(Application *app, String const& baseUrl, int web_timeout,
                                ajn::SessionId sessionId, String const& joiner,
                                devicehive::NetworkPtr network)
        {
            HIVELOG_DEBUG(app->m_log, "binding to AllJoyn session: " << sessionId);

            SharedPtr pthis(new Session(app, joiner));
            pthis->sessionRef() = sessionId;

            uint32_t timeout = LINK_TIMEOUT;
            QStatus status = app->m_AJ_bus->SetLinkTimeout(pthis->sessionRef(), timeout);
            checkAllJoynStatus(status, "failed to set link timeout");

            if (sessionId != 0)
            {
                HIVELOG_TRACE_STR(pthis->m_log, "sending registration request");
                pthis->sendGatewayRegistrationRequest();
            }

            pthis->m_baseUrl = baseUrl;
            pthis->m_webTout = web_timeout;
            pthis->m_network = network;

            return pthis;
        }

        void setHttpVersion(int major, int minor)
        {
            HIVELOG_INFO(m_log, "set HTTP version to "
                         << major << "." << minor);

            m_http_major = major;
            m_http_minor = minor;
        }

        void start()
        {}

        void stop()
        {
            if (m_service)
                m_service->cancelAll();
            m_service.reset();

            m_idleTimer.cancel();
        }

    private: // IDeviceServiceEvents

        /// @copydoc devicehive::IDeviceServiceEvents::onConnected()
        virtual void onConnected(ErrorCode err)
        {
            HIVELOG_TRACE_BLOCK(m_log, "onConnected()");
            m_pendingReconnect = false;

            if (!err)
            {
                HIVELOG_INFO_STR(m_log, "connected to the server");
                m_service->asyncGetServerInfo();
            }
            else
                handleServiceError(err, "connection");
        }


        /// @copydoc devicehive::IDeviceServiceEvents::onServerInfo()
        virtual void onServerInfo(boost::system::error_code err, devicehive::ServerInfo info)
        {
            if (!err)
            {
                // update timestamp first time only!
                // otherwise it's possible to lost some commands between reconnection attempts
                if (m_lastCommandTimestamp.empty())
                    m_lastCommandTimestamp = info.timestamp;

                // try to switch to websocket protocol
                if (!m_app->m_disableWebsockets && !info.alternativeUrl.empty())
                    if (devicehive::RestfulService::SharedPtr rest = boost::dynamic_pointer_cast<devicehive::RestfulService>(m_service))
                {
                    HIVELOG_INFO(m_log, "switching to Websocket service: " << info.alternativeUrl);
                    rest->cancelAll();

                    devicehive::WebsocketService::SharedPtr service = devicehive::WebsocketService::create(
                        m_app->m_http, info.alternativeUrl, shared_from_this());
                    service->setTimeout(rest->getTimeout());
                    m_service = service;

                    // connect again as soon as possible
                    m_app->m_delayed->callLater(boost::bind(&devicehive::IDeviceService::asyncConnect, m_service));
                    return;
                }

                // register device
                if (m_device && !m_deviceRegistered)
                    m_service->asyncRegisterDevice(m_device);
            }
            else
                handleServiceError(err, "getting server info");
        }


        /// @copydoc devicehive::IDeviceServiceEvents::onRegisterDevice()
        virtual void onRegisterDevice(boost::system::error_code err, devicehive::DevicePtr device)
        {
            if (m_device != device)     // if device's changed
                return;                 // just do nothing

            if (!err)
            {
                m_deviceRegistered = true;

                m_service->asyncSubscribeForCommands(m_device, m_lastCommandTimestamp);
                sendPendingNotifications();
            }
            else
                handleServiceError(err, "registering device");
        }


        /// @copydoc devicehive::IDeviceServiceEvents::onInsertCommand()
        virtual void onInsertCommand(ErrorCode err, devicehive::DevicePtr device, devicehive::CommandPtr command)
        {
            if (m_device != device)     // if device's changed
                return;                 // just do nothing

            if (!err)
            {
                m_lastCommandTimestamp = command->timestamp;
                bool processed = true;

                try
                {
                    processed = sendGatewayCommand(command);
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
                handleServiceError(err, "polling command");
        }

    private:

        /// @brief Send all pending notifications.
        void sendPendingNotifications()
        {
            const size_t N = m_pendingNotifications.size();
            HIVELOG_INFO(m_log, "sending " << N
                << " pending notifications");

            for (size_t i = 0; i < N; ++i)
            {
                m_service->asyncInsertNotification(m_device,
                    m_pendingNotifications[i]);
            }
            m_pendingNotifications.clear();
        }

    private:

        /// @brief Handle the service error.
        /**
        @param[in] err The error code.
        @param[in] hint The custom hint.
        */
        void handleServiceError(boost::system::error_code err, const char *hint)
        {
            if (!m_app->terminated() && m_service)
            {
                HIVELOG_ERROR(m_log, (hint ? hint : "something")
                    << " failed: [" << err << "] " << err.message());

                if (!m_pendingReconnect) // do nothing if reconnect is already in progress
                {
                    m_service->cancelAll();
                    if (m_device) // just in case to be able to subscribe again later
                        m_service->asyncUnsubscribeFromCommands(m_device);
                    m_deviceRegistered = false; // force to register it back

                    HIVELOG_DEBUG_STR(m_log, "try to connect later...");
                    m_app->m_delayed->callLater(SERVER_RECONNECT_TIMEOUT,
                        boost::bind(&devicehive::IDeviceService::asyncConnect, m_service));
                    m_pendingReconnect = true; // disable all another reconnect attempts
                }
            }
        }

    private:

        /// @brief Send gateway registration request.
        void sendGatewayRegistrationRequest()
        {
            json::Value data;
            data["data"] = json::Value();

            sendGatewayMessage(gateway::INTENT_REGISTRATION_REQUEST, data);
        }


        /// @brief Send the command to the device.
        /**
        @param[in] command The command to send.
        @return `true` if command processed.
        */
        bool sendGatewayCommand(devicehive::CommandPtr command)
        {
            const int intent = m_gw.findCommandIntentByName(command->name);
            if (0 <= intent)
            {
                HIVELOG_INFO(m_log, "command: \"" << command->name
                    << "\" mapped to #" << intent << " intent");

                json::Value data;
                data["id"] = command->id;
                data["parameters"] = command->params;
                if (!sendGatewayMessage(intent, data))
                    throw std::runtime_error("invalid command format");
                return false; // device will report command result later!
            }
            else
                throw std::runtime_error("unknown command, ignored");

            return true;
        }


        /// @brief Send the custom gateway message.
        /**
        @param[in] intent The message intent.
        @param[in] data The custom message data.
        @return `false` for invalid command.
        */
        bool sendGatewayMessage(int intent, json::Value const& data)
        {
            if (gateway::Frame::SharedPtr frame = m_gw.jsonToFrame(intent, data))
            {
                restartIdleTimeout(IDLE_TIMEOUT_SEC);

                HIVELOG_TRACE(m_log, "frame sending #"
                    << frame->getIntent() << ", "
                    << frame->size() << " bytes");

                if (m_app->m_AJ_obj)
                {
                    m_app->m_AJ_obj->sendFrame(frame, m_name, m_sessionId);
                    return true;
                }
                else
                    HIVELOG_WARN_STR(m_log, "no AllJoyn client connected, no message sent");
            }
            else
                HIVELOG_WARN_STR(m_log, "cannot convert frame to binary format");

            return false;
        }

    private:

        /// @brief The "recv frame" callback.
        /**
        @param[in] err The error code.
        @param[in] frame The frame received.
        */
        void onRecvGatewayFrame(boost::system::error_code err, gateway::Frame::SharedPtr frame)
        {
            if (!err)
            {
                if (frame)
                {
                    restartIdleTimeout(IDLE_TIMEOUT_SEC);

                    HIVELOG_TRACE(m_log, "frame received #"
                        << frame->getIntent() << ", "
                        << frame->size() << " bytes, from \"" << m_name << "\"");

                    try
                    {
                        handleGatewayMessage(frame->getIntent(),
                            m_gw.frameToJson(frame));
                    }
                    catch (std::exception const& ex)
                    {
                        HIVELOG_ERROR(m_log, "failed to handle received frame: " << ex.what());
                    }
                }
                else
                    HIVELOG_DEBUG_STR(m_log, "no frame received");
            }
            else
            {
                HIVELOG_ERROR(m_log, "failed to receive frame: ["
                    << err << "] " << err.message());
            }
        }


        /// @brief Create device from the JSON data.
        /**
        @param[in] jdev The JSON device description.
        */
        devicehive::DevicePtr createDevice(json::Value const& jdev)
        {
            devicehive::DevicePtr device;

            { // create device
                const String id = jdev["id"].asString();
                const String key = jdev["key"].asString();
                const String name = jdev["name"].asString();

                devicehive::Device::ClassPtr deviceClass = devicehive::Device::Class::create("", "", false, DEVICE_OFFLINE_TIMEOUT);
                devicehive::Serializer::fromJson(jdev["deviceClass"], deviceClass);

                //devicehive::NetworkPtr network = devicehive::Network::create(m_network->name, m_network->key, m_network->desc);
                //devicehive::Serializer::fromJson(jdev["network"], network);

                device = devicehive::Device::create(id, name, key, deviceClass, m_network);
                device->status = "Online";
            }

            { // update equipment
                json::Value const& equipment = jdev["equipment"];
                const size_t N = equipment.size();
                for (size_t i = 0; i < N; ++i)
                {
                    json::Value const& eq = equipment[i];

                    const String code = eq["code"].asString();
                    const String name = eq["name"].asString();
                    const String type = eq["type"].asString();

                    device->equipment.push_back(devicehive::Equipment::create(code, name, type));
                }
            }

            return device;
        }


        void startService(json::Value const& data)
        {
            if (!m_service) // create service
            {
                String baseUrl = data["URL"].asString();
                if (baseUrl.empty())
                    baseUrl = m_baseUrl; // default one

                http::Url url(baseUrl);

                if (boost::iequals(url.getProtocol(), "ws")
                    || boost::iequals(url.getProtocol(), "wss"))
                {
                    if (m_app->m_disableWebsockets)
                        throw std::runtime_error("websockets are disabled by --no-ws switch");

                    devicehive::WebsocketService::SharedPtr service = devicehive::WebsocketService::create(m_app->m_http, baseUrl, shared_from_this());
                    if (0 < m_webTout)
                        service->setTimeout(m_webTout*1000); // seconds -> milliseconds

                    HIVELOG_INFO_STR(m_log, "WebSocket service is used");
                    m_service = service;
                }
                else
                {
                    devicehive::RestfulService::SharedPtr service = devicehive::RestfulService::create(m_app->m_http, baseUrl, shared_from_this());
                    if (0 < m_webTout)
                        service->setTimeout(m_webTout*1000); // seconds -> milliseconds
                    service->setHttpVersion(m_http_major, m_http_minor);

                    HIVELOG_INFO_STR(m_log, "RESTful service is used");
                    m_service = service;
                }

                m_service->asyncConnect();
            }
        }


        /// @brief Handle the incomming message.
        /**
        @param[in] intent The message intent.
        @param[in] data The custom message data.
        */
        void handleGatewayMessage(int intent, json::Value const& data)
        {
            HIVELOG_INFO(m_log, "process intent #" << intent << " data: " << data << "\n");

            if (intent == gateway::INTENT_REGISTRATION_RESPONSE)
            {
                devicehive::DevicePtr dev = m_device = createDevice(data);
                bool restart = !m_device || (m_device->id != dev->id || m_device->name != dev->name);

                if (restart && m_device && m_service) // stop old device
                    m_service->asyncUnsubscribeFromCommands(m_device);

                if (restart)
                {
                    m_pendingNotifications.clear();
                    m_deviceRegistered = false;
                    m_device = dev;
                }

                m_gw.handleRegisterResponse(data);
                startService(data);
            }

            else if (intent == gateway::INTENT_REGISTRATION2_RESPONSE)
            {
                json::Value jdev = json::fromStr(data["json"].asString());
                devicehive::DevicePtr dev = createDevice(jdev);
                bool restart = !m_device || (m_device->id != dev->id || m_device->name != dev->name);

                if (restart && m_device && m_service) // stop old device
                    m_service->asyncUnsubscribeFromCommands(m_device);

                if (restart)
                {
                    m_pendingNotifications.clear();
                    m_deviceRegistered = false;
                    m_device = dev;
                }

                m_gw.handleRegister2Response(jdev);
                startService(data);
            }

            else if (intent == gateway::INTENT_COMMAND_RESULT_RESPONSE)
            {
                if (m_device && m_service)
                {
                    HIVELOG_DEBUG(m_log, "got command result");

                    devicehive::CommandPtr command = devicehive::Command::create();
                    command->id = data["id"].asUInt();
                    command->status = data["status"].asString();
                    command->result = data["result"];

                    m_service->asyncUpdateCommand(m_device, command);
                }
                else
                    HIVELOG_WARN_STR(m_log, "got command result before registration, ignored");
            }

            else if (intent >= gateway::INTENT_USER)
            {
                String name = m_gw.findNotificationNameByIntent(intent);
                if (!name.empty())
                {
                    HIVELOG_DEBUG(m_log, "got notification");

                    devicehive::NotificationPtr notification = devicehive::Notification::create(name, data["parameters"]);

                    if (m_deviceRegistered && m_device && m_service)
                    {
                        m_service->asyncInsertNotification(m_device, notification);
                    }
                    else
                    {
                        HIVELOG_DEBUG_STR(m_log, "device is not registered, notification delayed");
                        m_pendingNotifications.push_back(notification);
                    }
                }
                else
                    HIVELOG_WARN(m_log, "unknown notification: " << intent << ", ignored");
            }

            else
                HIVELOG_WARN(m_log, "invalid intent: " << intent << ", ignored");
        }

    private:

        void restartIdleTimeout(int wait_sec)
        {
            //HIVELOG_INFO(m_log, "restart idle timeout " << wait_sec << " seconds");
            m_idleTimer.expires_from_now(boost::posix_time::seconds(wait_sec));
            m_idleTimer.async_wait(boost::bind(&Session::needToSendRegistrationRequest,
                    shared_from_this(), boost::asio::placeholders::error()));
        }

        void needToSendRegistrationRequest(boost::system::error_code err)
        {
            if (!err)
            {
                HIVELOG_INFO(m_log, "session IDLE, sending registration request...");
                sendGatewayRegistrationRequest();
            }
        }

    private:
        friend class Application;
        String const m_name;
        Application* const m_app;

        String m_baseUrl;
        int m_webTout;
        int m_http_major;
        int m_http_minor;

        bool m_pendingReconnect;
        devicehive::IDeviceServicePtr m_service; ///< @brief The cloud service.
        gateway::Engine m_gw; ///< @brief The gateway engine.

    private:
        devicehive::DevicePtr m_device;   ///< @brief The device.
        devicehive::NetworkPtr m_network; ///< @brief The network.
        String m_lastCommandTimestamp;    ///< @brief The timestamp of the last received command.
        bool m_deviceRegistered;          ///< @brief The DeviceHive cloud "registered" flag.

    private:
        std::vector<devicehive::NotificationPtr> m_pendingNotifications; ///< @brief The list of pending notification.
        boost::asio::deadline_timer m_idleTimer;

        ajn::SessionId m_sessionId;
        hive::log::Logger m_log;
    };

    typedef Session::SharedPtr SessionPtr;

protected:

    /// @brief The default constructor.
    Application()
        : m_disableWebsockets(false)
        , m_web_timeout(0)
        , m_http_major(1)
        , m_http_minor(0)
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

        String networkName = "C++ AllJoyn network";
        String networkKey = "";
        String networkDesc = "C++ device test network";

        String baseUrl = "http://ecloud.dataart.com/ecapi8";
        size_t web_timeout = 0; // zero - don't change

        bool http_keep_alive = true;

        String serviceName;

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
                std::cout << "\t--http-maj <major version> the HTTP major version\n";
                std::cout << "\t--http-min <minor version> the HTTP minor version\n";
                std::cout << "\t--http-no-keep-alive disable keep-alive connections\n";
                std::cout << "\t--service <AllJoyn service name>\n";
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
            else if (boost::iequals(argv[i], "--no-ws"))
                pthis->m_disableWebsockets = true;
            else if (boost::algorithm::iequals(argv[i], "--http-maj") && i+1 < argc)
                pthis->m_http_major = boost::lexical_cast<int>(argv[++i]);
            else if (boost::algorithm::iequals(argv[i], "--http-min") && i+1 < argc)
                pthis->m_http_minor = boost::lexical_cast<int>(argv[++i]);
            else if (boost::iequals(argv[i], "--http-no-keep-alive"))
                http_keep_alive = false;
            else if (boost::algorithm::iequals(argv[i], "--service") && i+1 < argc)
                serviceName = argv[++i];
        }

        if (serviceName.empty())
            throw std::runtime_error("no AllJoyn service name provided");

        pthis->m_network = devicehive::Network::create(networkName, networkKey, networkDesc);
        pthis->m_defaultBaseUrl =  baseUrl;
        pthis->m_web_timeout = web_timeout;

        pthis->m_http = http::Client::create(pthis->m_ios);
        pthis->m_http->enableKeepAliveConnections(http_keep_alive);

        pthis->m_serviceName = serviceName;
        pthis->initAllJoyn();
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
    void initAllJoyn()
    {
        HIVELOG_TRACE(m_log_AJ, "creating BusAttachment");
        m_AJ_bus.reset(new ajn::BusAttachment(BUS_NAME, true));

        /* Create org.alljoyn.bus.samples.chat interface */
        HIVELOG_TRACE(m_log_AJ, "creating interface");
        ajn::InterfaceDescription *iface = 0;
        QStatus status = m_AJ_bus->CreateInterface(SERVICE_INTERFACE_NAME, iface, ajn::AJ_IFC_SECURITY_OFF);
        checkAllJoynStatus(status, "failed to create AllJoyn interface");

        HIVELOG_TRACE(m_log_AJ, "adding signals and activate");
        iface->AddSignal(FROM_GW_SIGNAL_NAME, "is", "intent,payload");
        iface->AddSignal(TO_GW_SIGNAL_NAME, "is", "intent,payload");
        iface->Activate();

        HIVELOG_TRACE(m_log_AJ, "register bus listener");
        m_AJ_bus->RegisterBusListener(*this);
        status = m_AJ_bus->Start();
        checkAllJoynStatus(status, "failed to start AllJoyn bus");

        // connect
        HIVELOG_TRACE(m_log_AJ, "connecting");
        status = m_AJ_bus->Connect();
        checkAllJoynStatus(status, "failed to connect AllJoyn bus");
        HIVELOG_INFO(m_log_AJ, "connected to BUS: " << m_AJ_bus->GetUniqueName().c_str());

        /*
         * Advertise this service on the bus.
         * There are three steps to advertising this service on the bus.
         * 1) Request a well-known name that will be used by the client to discover
         *    this service.
         * 2) Create a session.
         * 3) Advertise the well-known name.
         */
        HIVELOG_TRACE(m_log_AJ, "requesting name");
        status = m_AJ_bus->RequestName(m_serviceName.c_str(), DBUS_NAME_FLAG_DO_NOT_QUEUE);
        checkAllJoynStatus(status, "failed to request AllJoyn name");

        HIVELOG_TRACE(m_log_AJ, "binding service port");
        ajn::SessionOpts opts(ajn::SessionOpts::TRAFFIC_MESSAGES, true, ajn::SessionOpts::PROXIMITY_ANY, ajn::TRANSPORT_ANY);
        ajn::SessionPort sp = SERVICE_PORT;
        status = m_AJ_bus->BindSessionPort(sp, opts, *this);
        checkAllJoynStatus(status, "failed to bind AllJoyn session port");

        HIVELOG_TRACE(m_log_AJ, "advertise name");
        status = m_AJ_bus->AdvertiseName(m_serviceName.c_str(), ajn::TRANSPORT_ANY);
        checkAllJoynStatus(status, "failed to advertise AllJoyn service");


        m_AJ_obj.reset(new AJ_Session(this, *m_AJ_bus, SERVICE_OBJECT_PATH));
    }

private: // AJ_SessionEvents

    virtual void onGotFrame(gateway::Frame::SharedPtr frame,
                            const hive::String &from,
                            ajn::SessionId sessionId)
    {
        // called from an another thread!
        // do processing in main thread!
        m_ios.post(boost::bind(&This::safeOnGotFrame,
            shared_from_this(), frame, from, sessionId));
    }

    void safeOnGotFrame(gateway::Frame::SharedPtr frame,
                        const hive::String &from,
                        ajn::SessionId sessionId)
    {
        if (SessionPtr session = findSessionByName(from))
        {
            if (session->sessionRef() == sessionId)
            {
                HIVELOG_DEBUG(m_log, "process frame #" << frame->getIntent()
                              << " by \"" << from << "\", id:"
                              << sessionId);

                boost::system::error_code err;
                session->onRecvGatewayFrame(err, frame);
            }
            else
            {
                HIVELOG_WARN(m_log, "session id mismatch \"" << from
                             << "\", id:" << sessionId << ", ignored");
            }
        }
        else
        {
            HIVELOG_WARN(m_log, "unknown session \"" << from
                         << "\", id:" << sessionId << ", ignored");
        }
    }

protected:

    /**
     * @brief Start the application.
     */
    virtual void start()
    {
        Base::start();
    }


    /**
     * @brief Stop the application.
     */
    virtual void stop()
    {
        { // stop all sessions
            std::map<String, SessionPtr>::iterator i = m_sessionsByName.begin();
            for (; i != m_sessionsByName.end(); ++i)
            {
                SessionPtr session = i->second;
                session->stop();
            }

            m_sessionsByName.clear();
            m_sessionsById.clear();
        }

        if (m_AJ_obj)
        {
            m_AJ_obj->stop(*m_AJ_bus);
            m_AJ_obj.reset();
        }

        HIVELOG_INFO(m_log_AJ, "disconnecting BUS: " << m_AJ_bus->GetUniqueName().c_str());
        QStatus status = m_AJ_bus->Disconnect();
        checkAllJoynStatus(status, "failed to disconnect AllJoyn bus");

        HIVELOG_INFO(m_log_AJ, "stopping bus...");
        status = m_AJ_bus->Stop();
        checkAllJoynStatus(status, "failed to stop bus attachment");

        Base::stop();
    }

private:

    void FoundAdvertisedName(const char* name, ajn::TransportMask transport, const char* namePrefix)
    {
        HIVELOG_INFO(m_log_AJ, "found advertised name: \"" << name << "\" prefix: \"" << namePrefix << "\"");
    }

    void LostAdvertisedName(const char* name, ajn::TransportMask transport, const char* namePrefix)
    {
        HIVELOG_INFO(m_log_AJ, "advertised name is lost:" << name << " prefix:" << namePrefix);
    }

    void NameOwnerChanged(const char* busName, const char* previousOwner, const char* newOwner)
    {
        HIVELOG_INFO(m_log_AJ, "bus:\"" << busName << "\" name owner changed from \""
                     << (previousOwner?previousOwner:"<null>") << "\" to \""
                     << (newOwner?newOwner:"<null>") << "\"");

        if (previousOwner && !newOwner)
        {
            // do it on main thread!
            m_ios.post(boost::bind(&Application::doSessionNameLost,
                shared_from_this(), hive::String(previousOwner)));
        }
    }

    virtual void SessionLost(ajn::SessionId sessionId, SessionLostReason)
    {
        // do it on main thread!
        m_ios.post(boost::bind(&Application::doSessionLost, shared_from_this(), sessionId));
    }

    virtual void SessionMemberAdded(ajn::SessionId sessionId, const char* uniqueName)
    {
        HIVELOG_INFO(m_log_AJ, "session id:" << sessionId << " member added: \"" << uniqueName << "\"");
    }

    virtual void SessionMemberRemoved(ajn::SessionId sessionId, const char* uniqueName)
    {
        HIVELOG_INFO(m_log_AJ, "session id:" << sessionId << " member removed: \"" << uniqueName << "\"");
    }

    bool AcceptSessionJoiner(ajn::SessionPort sessionPort, const char* joiner, const ajn::SessionOpts& opts)
    {
        if (sessionPort != SERVICE_PORT)
        {
            HIVELOG_WARN(m_log_AJ, "reject session from \"" << (joiner?joiner:"<null>") << "\", invalid port: " << sessionPort);
            return false;
        }

        HIVELOG_INFO(m_log_AJ, "accept session from \"" << (joiner?joiner:"<null>") << "\"");
        return true;
    }

    void SessionJoined(ajn::SessionPort sessionPort, ajn::SessionId id, const char* joiner)
    {
        // do it on main thread!
        String joiner_s = (joiner?joiner:"");
        m_ios.post(boost::bind(&Application::doSessionJoined, shared_from_this(), id, joiner_s));
    }

private:

    void doSessionJoined(ajn::SessionId id, String joiner)
    {
        HIVELOG_INFO(m_log_AJ, "session joined: \"" << (!joiner.empty()?joiner:"<null>") << "\" id:" << id);

        HIVELOG_TRACE(m_log_AJ, "creating session");
        SessionPtr session = Session::create(this, m_defaultBaseUrl, m_web_timeout, id, joiner, m_network);
        session->setHttpVersion(m_http_major, m_http_minor);
        m_sessionsByName[joiner] = session;
        m_sessionsById[id] = session;

        session->start();
    }

    void doSessionLost(ajn::SessionId id)
    {
        HIVELOG_INFO(m_log_AJ, "lost session id:" << id);
        if (SessionPtr session = findSessionById(id))
        {
            session->stop();

            m_sessionsById.erase(id);
            m_sessionsByName.erase(session->m_name);
        }
    }

    void doSessionNameLost(const hive::String &name)
    {
        HIVELOG_INFO(m_log_AJ, "lost session:" << name);
        if (SessionPtr session = findSessionByName(name))
        {
            session->stop();

            m_sessionsById.erase(session->sessionRef());
            m_sessionsByName.erase(session->m_name);
        }
    }

private:

    bool m_disableWebsockets;       ///< @brief No automatic websocket switch.
    http::ClientPtr m_http;

    devicehive::NetworkPtr m_network;
    String m_defaultBaseUrl;
    int m_web_timeout;

    int m_http_major;
    int m_http_minor;

private:
    std::map<String, SessionPtr> m_sessionsByName;
    std::map<ajn::SessionId, SessionPtr> m_sessionsById;

    SessionPtr findSessionById(ajn::SessionId id) const
    {
        std::map<ajn::SessionId, SessionPtr>::const_iterator i = m_sessionsById.find(id);
        return (i != m_sessionsById.end()) ? i->second : SessionPtr();
    }

    SessionPtr findSessionByName(String const& name) const
    {
        std::map<String, SessionPtr>::const_iterator i = m_sessionsByName.find(name);
        return (i != m_sessionsByName.end()) ? i->second : SessionPtr();
    }

private:
    String m_serviceName;  ///< @brief The AllJoyn service name.
    boost::shared_ptr<ajn::BusAttachment> m_AJ_bus;
    boost::shared_ptr<AJ_Session> m_AJ_obj;

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
