/** @file
@brief The simple gateway example.
@author Sergey Polichnoy <sergey.polichnoy@dataart.com>
@see @ref page_simple_gw
*/
#ifndef __AJ_SERIAL_HPP__
#define __AJ_SERIAL_HPP__

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

// constants
static const char* SERVICE_INTERFACE_NAME = "com.devicehive.samples.alljoyn.serial";
static const char* SERVICE_OBJECT_PATH = "/serialService";
static const char* FROM_GW_SIGNAL_NAME = "dataFromGw";
static const char* TO_GW_SIGNAL_NAME = "dataToGw";
static const char* BUS_NAME = "DH_AJ";
static const uint32_t LINK_TIMEOUT = 20;
static const ajn::SessionPort SERVICE_PORT = 27;


namespace AJ_serial
{
    using namespace hive;


/// @brief Various contants and timeouts.
enum Timeouts
{
    SERIAL_RECONNECT_TIMEOUT    = 10000, ///< @brief Try to open serial port each X milliseconds.
    SERVER_RECONNECT_TIMEOUT    = 10000, ///< @brief Try to open server connection each X milliseconds.
    RETRY_TIMEOUT               = 5000   ///< @brief Common retry timeout, milliseconds.
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


/**
 * @brief The AllJoyn-Serial bridge application.
 *
 * This application controls only one device connected via serial port!
 */
class Application:
    public basic_app::Application,
    public ajn::BusListener,
    public ajn::SessionListener
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
        AJ_Session(Application *app, ajn::BusAttachment& bus, const char* path)
            : ajn::BusObject(path)
            , m_fromGwSignal(0)
            , m_toGwSignal(0)
            , m_sessionId(0)
            , m_log("/AllJoyn/Session")
            , m_app(app)
        {
            // get interface to this object
            const ajn::InterfaceDescription *iface = bus.GetInterface(SERVICE_INTERFACE_NAME);
            if (!iface) throw std::runtime_error("no interface found");
            AddInterface(*iface);

            // store the signal member away so it can be quickly looked up when signals are sent
            m_fromGwSignal = iface->GetMember(FROM_GW_SIGNAL_NAME);
            if (!m_fromGwSignal) throw std::runtime_error("no FromGw signal found");
            m_toGwSignal = iface->GetMember(TO_GW_SIGNAL_NAME);
            if (!m_toGwSignal) throw std::runtime_error("no ToGw signal found");

            // register signal handler
            QStatus status =  bus.RegisterSignalHandler(this,
                    static_cast<ajn::MessageReceiver::SignalHandler>(&AJ_Session::gotData),
                    m_fromGwSignal, NULL);
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
            HIVELOG_TRACE_STR(m_log, "unregistering object...");
            bus.UnregisterBusObject(*this);
            //checkAllJoynStatus(status, "failed to unregister AllJoyn bus object");

            HIVELOG_TRACE_STR(m_log, "unregister all handlers...");
            QStatus status = bus.UnregisterAllHandlers(this);
            checkAllJoynStatus(status, "failed to unregister AllJoyn signal handlers");

            HIVELOG_TRACE_STR(m_log, "stopped");
        }


        /**
         * @brief Send frame to AllJoyn service.
         */
        QStatus sendFrame(gateway::Frame::SharedPtr frame)
        {
            if (!m_sessionId)
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

            QStatus res = Signal(m_destination.c_str(), m_sessionId, *m_toGwSignal, &msg_arg[0], 2, 0, flags);
            HIVELOG_DEBUG(m_log, "send frame: #" << frame->getIntent()
                << " \"" << utils::lim(data_hex, 32) << "\" to \""
                         << m_destination << "\" (status: " << res << ")");

            return res;
        }


        /**
         * @brief Got signal from AllJoyn service
         */
        void gotData(const ajn::InterfaceDescription::Member*, const char* src_path, ajn::Message& msg)
        {
            const int intent = msg->GetArg(0)->v_int32;
            const String data_hex = msg->GetArg(1)->v_string.str;
            HIVELOG_DEBUG(m_log, "recv frame: #" << intent
                        << " \"" << utils::lim(data_hex, 32) << "\" from "
                        << (src_path ? src_path : "<null>")
                        << " session_id:" << msg->GetSessionId()
                        << " sender:" << msg->GetSender());

            // do it on main thread
            m_app->m_ios.post(boost::bind(&Application::sendFrameToSerial, m_app,
                gateway::Frame::create(intent, utils::fromHex(data_hex))));
        }


        /**
         * @brief Get session identifier reference.
         */
        ajn::SessionId& sessionRef()
        {
            return m_sessionId;
        }

        void setDestination(const hive::String &dest)
        {
            HIVELOG_INFO(m_log, "assume gateway name is \"" << dest << "\n");
            m_destination = dest;
        }

    private:
        const ajn::InterfaceDescription::Member *m_fromGwSignal;
        const ajn::InterfaceDescription::Member *m_toGwSignal;
        ajn::SessionId m_sessionId;
        hive::String m_destination;
        hive::log::Logger m_log;
        Application *m_app;
    };


    /**
     * @brief Bridge engine.
     *
     * Contains bridge-related messages.
     */
    class BridgeEngine:
        public gateway::Engine
    {
    public:

        enum Intents
        {
            AJ_INFO_REQUEST     = 30001,
            AJ_INFO_RESPONSE    = 30002,
            AJ_SESSION_STATUS   = 30003,
            AJ_SYSTEM_EXEC      = 30004
        };

        BridgeEngine()
        {
            m_layouts.registerIntent(AJ_INFO_REQUEST, createAllJoynInfoRequest());
            m_layouts.registerIntent(AJ_INFO_RESPONSE, createAllJoynInfoResponse());
            m_layouts.registerIntent(AJ_SESSION_STATUS, createAllJoynSessionStatus());
            m_layouts.registerIntent(AJ_SYSTEM_EXEC, createAllJoynSystemExec());
        }

    private:

        /// @brief Create "AllJoyn Info Request" layout.
        /**
        @return The new "AllJoyn Info Request" layout.
        */
        static gateway::Layout::SharedPtr createAllJoynInfoRequest()
        {
            gateway::Layout::SharedPtr layout = gateway::Layout::create();
            layout->add("data", gateway::DT_NULL);
            return layout;
        }


        /// @brief Create "AllJoyn Info Response" layout.
        /**
        @return The new "AllJoyn Info Response" layout.
        */
        static gateway::Layout::SharedPtr createAllJoynInfoResponse()
        {
            gateway::Layout::SharedPtr layout = gateway::Layout::create();
            layout->add("channel", gateway::DT_STRING);
            return layout;
        }


        /// @brief Create "AllJoyn Session Status" layout.
        /**
        @return The new "AllJoyn Session Status" layout.
        */
        static gateway::Layout::SharedPtr createAllJoynSessionStatus()
        {
            gateway::Layout::SharedPtr layout = gateway::Layout::create();
            layout->add("connected", gateway::DT_UINT8);
            return layout;
        }


        /// @brief Create "AllJoyn System Exec" layout.
        /**
        @return The new "AllJoyn System Exec" layout.
        */
        static gateway::Layout::SharedPtr createAllJoynSystemExec()
        {
            gateway::Layout::SharedPtr layout = gateway::Layout::create();
            layout->add("cmd", gateway::DT_STRING);
            return layout;
        }
    };

protected:

    /// @brief The default constructor.
    Application()
        : m_serial(m_ios)
        , m_echoMode(false)
        , m_log_AJ("/AllJoyn")
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

        String serialPortName = "";
        UInt32 serialBaudrate = 9600;

        String joinName = "";

        // custom device properties
        for (int i = 1; i < argc; ++i) // skip executable name
        {
            if (boost::algorithm::iequals(argv[i], "--help"))
            {
                std::cout << argv[0] << " [options]\n";
#if !defined(ARDUINO_BRIDGE_ENABLED)
                std::cout << "\t--serial <serial device>\n";
                std::cout << "\t--baudrate <serial baudrate>\n";
#else
                std::cout << "\t--host <telnet host>\n";
                std::cout << "\t--port <telnet port>\n";
#endif
                std::cout << "\t--join <service name>\n";
                std::cout << "\t--log <log file name>\n"; // see below in main()

                exit(1);
            }
#if !defined(ARDUINO_BRIDGE_ENABLED)
            else if (boost::algorithm::iequals(argv[i], "--serial") && i+1 < argc)
                serialPortName = argv[++i];
            else if (boost::algorithm::iequals(argv[i], "--baudrate") && i+1 < argc)
                serialBaudrate = boost::lexical_cast<UInt32>(argv[++i]);
#else
            else if (boost::algorithm::iequals(argv[i], "--host") && i+1 < argc)
                serialPortName = argv[++i];
            else if (boost::algorithm::iequals(argv[i], "--port") && i+1 < argc)
                serialBaudrate = boost::lexical_cast<UInt32>(argv[++i]);
#endif
            else if (boost::algorithm::iequals(argv[i], "--join") && i+1 < argc)
                joinName = argv[++i];
            else if (boost::algorithm::iequals(argv[i],  "--echo"))
            {
                serialPortName = "<no>";
                pthis->m_echoMode = true;
            }
        }

        if (serialPortName.empty())
            throw std::runtime_error("no stream device name provided");
        if (joinName.empty())
            throw std::runtime_error("no AllJoyn service name provided");

        pthis->m_serialPortName = serialPortName;
        pthis->m_serialBaudrate = serialBaudrate;
        pthis->m_joinName = joinName;

        pthis->m_serial_api = SerialAPI::create(pthis->m_serial);
        pthis->initAllJoyn();

        return pthis;
    }


    /**
     * @brief Get the shared pointer.
     * @return The shared pointer to this instance.
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

        HIVELOG_TRACE(m_log_AJ, "creating interface");
        ajn::InterfaceDescription *iface = 0;
        QStatus status = m_AJ_bus->CreateInterface(SERVICE_INTERFACE_NAME, iface, ajn::AJ_IFC_SECURITY_OFF);
        checkAllJoynStatus(status, "failed to create AllJoyn interface");

        HIVELOG_TRACE(m_log_AJ, "adding signal and activate");
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

        if (!m_joinName.empty())
            findAdName(m_joinName);
    }

    void createNewAllJoynSession()
    {
        HIVELOG_TRACE(m_log_AJ, "creating new session");
        m_AJ_obj.reset(new AJ_Session(this, *m_AJ_bus, SERVICE_OBJECT_PATH));
        m_AJ_obj->setDestination(m_peerName);
    }

    void findAdName(const String &channel)
    {
        // begin discovery on the well-known name of the service to be called
        HIVELOG_TRACE(m_log_AJ, "finding advertised name \"" << channel << "\"");
        QStatus status = m_AJ_bus->FindAdvertisedName(channel.c_str());
        checkAllJoynStatus(status, "failed to find service AllJoyn bus");
    }

    void cancelFindAdName(const String &channel)
    {
        // begin discovery on the well-known name of the service to be called
        HIVELOG_TRACE(m_log_AJ, "cancel finding advertised name \"" << channel << "\"");
        /*QStatus status = */m_AJ_bus->CancelFindAdvertisedName(channel.c_str());
//        checkAllJoynStatus(status, "failed to find service AllJoyn bus");
    }

protected:

    /**
     * @brief Start the application.
     */
    virtual void start()
    {
        Base::start();
        m_delayed->callLater( // ASAP
            boost::bind(&This::tryToOpenSerial,
                shared_from_this()));
    }


    /**
     * @brief Stop the application.
     */
    virtual void stop()
    {
        resetSerial(false);
        asyncListenForSerialFrames(false); // stop listening to release shared pointer

        if (m_AJ_obj && m_AJ_obj->sessionRef() != 0)
            doSessionLost(m_AJ_obj->sessionRef());

        HIVELOG_INFO(m_log_AJ, "disconnecting BUS: " << m_AJ_bus->GetUniqueName().c_str());
        QStatus status = m_AJ_bus->Disconnect();
        checkAllJoynStatus(status, "failed to disconnect AllJoyn bus");

        HIVELOG_INFO(m_log_AJ, "stopping bus...");
        status = m_AJ_bus->Stop();
        checkAllJoynStatus(status, "failed to stop bus attachment");

        Base::stop();
    }

private:

    void sendFakeFrame()
    {
        if (m_AJ_obj)
        {
            static int connected = 0;
            ++connected;

            HIVELOG_DEBUG(m_log, "sending fake SESSION_STATUS:"<< connected << " to AllJoyn");
            json::Value params;
            params["connected"] = connected;
            gateway::Frame::SharedPtr frame = m_bridge.jsonToFrame(BridgeEngine::AJ_SESSION_STATUS, params);
            m_AJ_obj->sendFrame(frame);
        }

        if (!terminated()) // do it again later
        {
            m_delayed->callLater(10000,
                boost::bind(&This::sendFakeFrame,
                shared_from_this()));
        }
    }


    /**
     * @brief Try to open serial port device.
     */
    void tryToOpenSerial()
    {
        if (m_echoMode)
        {
            m_delayed->callLater(1000,
                boost::bind(&This::sendFakeFrame,
                shared_from_this()));
            return;
        }

        boost::system::error_code err = openSerial();

        if (!err)
        {
            HIVELOG_DEBUG(m_log,
                "got stream device \"" << m_serialPortName
                << "\" at baudrate/port: " << m_serialBaudrate);

            asyncListenForSerialFrames(true);
            sendAllJoynSessionStatus(m_AJ_obj && m_AJ_obj->sessionRef() != 0);
            sendAllJoynInfoRequest();
            sendAllJoynToSerialPendingFrames();
        }
        else
        {
            HIVELOG_DEBUG(m_log, "cannot open stream device \""
                << m_serialPortName << "\": ["
                << err << "] " << err.message());

            m_delayed->callLater(SERIAL_RECONNECT_TIMEOUT,
                boost::bind(&This::tryToOpenSerial,
                    shared_from_this()));
        }
    }


    /**
     * @brief Try to open serial device.
     * @return The error code.
     */
    virtual boost::system::error_code openSerial()
    {
        boost::system::error_code err;

#if !defined(ARDUINO_BRIDGE_ENABLED)
        boost::asio::serial_port & port = m_serial;

        port.close(err); // (!) ignore error
        port.open(m_serialPortName, err);
        if (err) return err;

        // set baud rate
        port.set_option(boost::asio::serial_port::baud_rate(m_serialBaudrate), err);
        if (err) return err;

        // set character size
        port.set_option(boost::asio::serial_port::character_size(), err);
        if (err) return err;

        // set flow control
        port.set_option(boost::asio::serial_port::flow_control(), err);
        if (err) return err;

        // set stop bits
        port.set_option(boost::asio::serial_port::stop_bits(), err);
        if (err) return err;

        // set parity
        port.set_option(boost::asio::serial_port::parity(), err);
        if (err) return err;
#else
        boost::asio::ip::tcp::socket & port = m_serial;

        typedef boost::asio::ip::tcp::resolver Resolver;
        Resolver r(m_ios);
        Resolver::iterator it = r.resolve(Resolver::query(m_serialPortName,
                        boost::lexical_cast<String>(m_serialBaudrate)), err);
        if (err) return err;

        if (it != Resolver::iterator())
        {
            port.connect(*it, err);
            if (err) return err;
        }
        else
            err = boost::asio::error::fault;
#endif

        return err; // OK
    }


    /**
     * @brief Reset the serial device.
     * @brief tryToReopen if `true` then try to reopen serial as soon as possible.
     */
    virtual void resetSerial(bool tryToReopen)
    {
        HIVELOG_WARN(m_log, "stream device reset");
        m_serial.close();

        if (tryToReopen && !terminated())
        {
            m_delayed->callLater(SERIAL_RECONNECT_TIMEOUT,
                boost::bind(&This::tryToOpenSerial,
                    shared_from_this()));
        }
    }

private:

    bool sendAllJoynInfoRequest()
    {
        HIVELOG_DEBUG(m_log, "sending AllJoyn INFO_REQEUST to serial");
        return sendFrameToSerial(m_bridge.jsonToFrame(BridgeEngine::AJ_INFO_REQUEST, json::Value()));
    }


    bool sendAllJoynSessionStatus(int connected)
    {
        HIVELOG_DEBUG(m_log, "sending AllJoyn SESSION_STATUS:"<< connected << " to serial");
        json::Value params;
        params["connected"] = connected;
        return sendFrameToSerial(m_bridge.jsonToFrame(BridgeEngine::AJ_SESSION_STATUS, params));
    }


    void sendAllJoynToSerialPendingFrames()
    {
        std::vector<gateway::Frame::SharedPtr> frames;
        std::swap(frames, m_alljyonToSerialPendingFrames);
        for (size_t i = 0; i < frames.size(); ++i)
            sendFrameToSerial(frames[i]);
    }


    void sendSerialToAllJoynPendingFrames()
    {
        if (!m_AJ_obj)
            return;

        std::vector<gateway::Frame::SharedPtr> frames;
        std::swap(frames, m_serialToAlljyonPendingFrames);
        for (size_t i = 0; i < frames.size(); ++i)
            m_AJ_obj->sendFrame(frames[i]);
    }


    /// @brief Send the custom frame to serial.
    /**
    @param[in] frame The custom frame to send.
    @return `false` for invalid command.
    */
    bool sendFrameToSerial(gateway::Frame::SharedPtr frame)
    {
        if (m_echoMode && m_AJ_obj)
        {
            HIVELOG_WARN(m_log, "ECHO MODE: send frame #" << frame->getIntent() << " back");
            m_AJ_obj->sendFrame(frame);
            return false;
        }

        if (!m_serial.is_open())
        {
            if (m_alljyonToSerialPendingFrames.size() > 100)
            {
                HIVELOG_WARN(m_log, "too many pending frames, clear all");
                m_alljyonToSerialPendingFrames.clear();
            }

            HIVELOG_WARN(m_log, "frame #" << frame->getIntent() << " is delayed, no serial yet");
            m_alljyonToSerialPendingFrames.push_back(frame);
            return false;
        }

        m_serial_api->send(frame,
            boost::bind(&This::onFrameSentToSerial,
                shared_from_this(), _1, _2));
        return true;
    }


    /// @brief The "send frame" callback.
    /**
    @param[in] err The error code.
    @param[in] frame The frame sent.
    */
    void onFrameSentToSerial(boost::system::error_code err, gateway::Frame::SharedPtr frame)
    {
        if (!err && frame)
        {
            HIVELOG_DEBUG(m_log, "frame #" << frame->getIntent() << " successfully sent ["
                << utils::lim(m_serial_api->hexdump(frame),32) << "], "
                << frame->size() << " bytes");
        }
        else
        {
            HIVELOG_ERROR(m_log, "failed to send frame: ["
                << err << "] " << err.message());
            resetSerial(true);
        }
    }

private:

    /// @brief Start/stop listen for RX frames.
    void asyncListenForSerialFrames(bool enable)
    {
        m_serial_api->recv(enable
            ? boost::bind(&This::onFrameReceivedFromSerial, shared_from_this(), _1, _2)
            : SerialAPI::RecvFrameCallback());
    }


    /// @brief The "recv frame" callback.
    /**
    @param[in] err The error code.
    @param[in] frame The frame received.
    */
    void onFrameReceivedFromSerial(boost::system::error_code err, gateway::Frame::SharedPtr frame)
    {
        if (!err)
        {
            if (frame)
            {
                HIVELOG_DEBUG(m_log, "frame #" << frame->getIntent() << " received ["
                    << utils::lim(m_serial_api->hexdump(frame), 32) << "], "
                    << frame->size() << " bytes");

                try
                {
                    handleFrameFromSerial(frame);
                }
                catch (std::exception const& ex)
                {
                    HIVELOG_ERROR(m_log, "failed to handle received frame: " << ex.what());
                    resetSerial(true);
                }
            }
            else
                HIVELOG_DEBUG_STR(m_log, "no frame received");
        }
        else
        {
            HIVELOG_ERROR(m_log, "failed to receive frame: ["
                << err << "] " << err.message());
            resetSerial(true);
        }
    }


    /// @brief Handle the incomming serial frame.
    /**
    @param[in] frame The frame received.
    */
    void handleFrameFromSerial(gateway::Frame::SharedPtr frame)
    {
        switch (frame->getIntent())
        {
            case BridgeEngine::AJ_INFO_REQUEST:
            case BridgeEngine::AJ_SESSION_STATUS:
                // just ignore
                return;

            case BridgeEngine::AJ_INFO_RESPONSE:
            {
                json::Value params = m_bridge.frameToJson(frame);
                String channel = params["channel"].asString();
                if (!channel.empty() && channel != m_joinName)
                {
                    if (m_AJ_obj)
                    {
                        HIVELOG_INFO(m_log, "delete previous session");
                        m_AJ_obj->stop(*m_AJ_bus);
                        m_AJ_obj.reset();
                    }

                    HIVELOG_INFO(m_log, "changing channel to \"" << channel << "\"");
                    m_peerName = getPeerName(channel);
                    cancelFindAdName(m_joinName);
                    m_joinName = channel;
                    findAdName(channel);
                }
                else if (m_AJ_obj)
                    sendAllJoynSessionStatus(1); // already connected
            } return;

            case BridgeEngine::AJ_SYSTEM_EXEC:
            {
                json::Value params = m_bridge.frameToJson(frame);
                const String cmd = params["cmd"].asString();
                if (!cmd.empty())
                {
                    HIVELOG_INFO(m_log, "executing \"" << cmd << "\" command...");
                    const int result = system(cmd.c_str());
                    HIVELOG_DEBUG(m_log, "execute \"" << cmd << "\" command:" << result);
                }
            } return;
        }

        if (m_AJ_obj)
            m_AJ_obj->sendFrame(frame);
        else
        {
            if (m_serialToAlljyonPendingFrames.size() > 100)
            {
                HIVELOG_WARN(m_log, "too many pending frames, clear all");
                m_serialToAlljyonPendingFrames.clear();
            }

            HIVELOG_WARN(m_log, "no AllJoyn session, frame is delayed");
            m_serialToAlljyonPendingFrames.push_back(frame);
        }
    }

private: // AllJoyn interfaces

    void FoundAdvertisedName(const char* name, ajn::TransportMask transport, const char* namePrefix)
    {
        // do it on main thread!
        m_ios.post(boost::bind(&Application::doFoundAdvertisedName, shared_from_this(), String(name)));
    }

    void LostAdvertisedName(const char* name, ajn::TransportMask transport, const char* namePrefix)
    {
        HIVELOG_INFO(m_log_AJ, "advertised name is lost:\"" << name << "\" prefix:\"" << namePrefix << "\"");
    }

    void NameOwnerChanged(const char* busName, const char* previousOwner, const char* newOwner)
    {
        HIVELOG_INFO(m_log_AJ, "bus:\"" << busName << "\" name owner changed from \""
                     << (previousOwner?previousOwner:"<null>") << "\" to \""
                     << (newOwner?newOwner:"<null>") << "\"");

        // do it on main thread
        m_ios.post(boost::bind(&Application::doNameOwnerChanged, shared_from_this(),
                               hive::String(busName ? busName : ""),
                               hive::String(previousOwner ? previousOwner : ""),
                               hive::String(newOwner ? newOwner : "")));
    }

    virtual void SessionLost(ajn::SessionId sessionId, SessionLostReason reason)
    {
        // do it on main thread
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

private:
    void doFoundAdvertisedName(String name)
    {
        if (name != m_joinName)
        {
            HIVELOG_DEBUG(m_log_AJ, "found unexpected advertised name: \"" << name << "\", ignored");
            return;
        }

        HIVELOG_INFO(m_log_AJ, "found advertised name: \"" << name << "\"");

        if (m_AJ_obj) // cleanup
        {
            HIVELOG_INFO(m_log_AJ, "delete previous session");
            m_AJ_obj->stop(*m_AJ_bus);
            m_AJ_obj.reset();
        }

        createNewAllJoynSession();

        // Join the conversation
        HIVELOG_TRACE(m_log_AJ, "joining session...");
        ajn::SessionOpts opts(ajn::SessionOpts::TRAFFIC_MESSAGES, true,
                              ajn::SessionOpts::PROXIMITY_ANY,
                              ajn::TRANSPORT_ANY);
        QStatus status = m_AJ_bus->JoinSession(name.c_str(), SERVICE_PORT, this, m_AJ_obj->sessionRef(), opts);
        checkAllJoynStatus(status, "failed to join session");
        HIVELOG_INFO(m_log_AJ, "join session id:" << m_AJ_obj->sessionRef());

        uint32_t timeout = LINK_TIMEOUT;
        status = m_AJ_bus->SetLinkTimeout(m_AJ_obj->sessionRef(), timeout);
        checkAllJoynStatus(status, "failed to set link timeout");

        sendAllJoynSessionStatus(1);
        sendSerialToAllJoynPendingFrames();
    }

    void doNameOwnerChanged(hive::String busName, hive::String previousOwner, hive::String newOwner)
    {
        if (!busName.empty())
        {
            if (!newOwner.empty()) // create
            {
                if (busName != newOwner)
                    m_peerNames[busName] = newOwner;
            }
            else    // remove
            {
                m_peerNames.erase(busName);
            }
        }

        // update destination
        if (busName == m_joinName && !newOwner.empty())
        {
            m_peerName = newOwner;

            if (m_AJ_obj)
                m_AJ_obj->setDestination(m_peerName);
        }
    }

    hive::String getPeerName(const hive::String &name) const
    {
        std::map<hive::String, hive::String>::const_iterator i = m_peerNames.find(name);
        return (i != m_peerNames.end()) ? i->second : hive::String();
    }

    void doSessionLost(ajn::SessionId sessionId)
    {
        HIVELOG_INFO(m_log_AJ, "lost session id:" << sessionId);
        if (m_AJ_obj && m_AJ_obj->sessionRef() == sessionId)
        {
            HIVELOG_INFO(m_log_AJ, "delete session id:" << sessionId);
            m_AJ_obj->stop(*m_AJ_bus);
            m_AJ_obj.reset();

            sendAllJoynSessionStatus(0);
        }
    }

private:
#if !defined(ARDUINO_BRIDGE_ENABLED)
    typedef gateway::API<boost::asio::serial_port> SerialAPI; ///< @brief The serial %API type.
    boost::asio::serial_port m_serial; ///< @brief The serial port device.
#else
    typedef gateway::API<boost::asio::ip::tcp::socket> SerialAPI; ///< @brief The socket %API type.
    boost::asio::ip::tcp::socket m_serial; ///< @brief The socket.
#endif

    SerialAPI::SharedPtr m_serial_api; ///< @brief The serial %API.
    BridgeEngine m_bridge;

    String m_serialPortName; ///< @brief The serial port name.
    UInt32 m_serialBaudrate; ///< @brief The serial baudrate.

private:
    std::vector<gateway::Frame::SharedPtr> m_alljyonToSerialPendingFrames;
    std::vector<gateway::Frame::SharedPtr> m_serialToAlljyonPendingFrames;

private:
    String m_joinName;  ///< @brief The AllJoyn service name.
    std::map<String, String> m_peerNames; ///< @brief All known names.
    String m_peerName;  ///< @brief The service destination.
    bool m_echoMode;

    boost::shared_ptr<ajn::BusAttachment> m_AJ_bus;
    boost::shared_ptr<AJ_Session>         m_AJ_obj;

private:
    hive::log::Logger m_log_AJ;
};


/// @brief The application entry point.
/**
Creates the Application instance and calls its Application::run() method.

@param[in] argc The number of command line arguments.
@param[in] argv The command line arguments.
*/
inline void main(int argc, const char* argv[])
{
    { // configure logging
        using namespace hive::log;

        hive::String log_file_name = "/tmp/AJ_serial.log";

        for (int i = 1; i < argc; ++i)
        {
            if (boost::iequals(argv[i], "--log") && (i+1 < argc))
                log_file_name = argv[i+1];
        }

        Target::File::SharedPtr log_file = Target::File::create(log_file_name);
        Target::SharedPtr log_console = Logger::root().getTarget();
        Logger::root().setTarget(Target::Tie::create(log_file, log_console));
        Logger::root().setLevel(LEVEL_TRACE);
        Logger("/gateway/API").setTarget(log_file); // disable annoying messages
        log_console->setFormat(Format::create("%N %L %M\n"));
        log_console->setMinimumLevel(LEVEL_DEBUG);
        log_file->setMaxFileSize(5*1024*1024);
        log_file->setNumberOfBackups(1);
        log_file->setFormat(Format::create("%T %N %L [%I] %M\n"));
        log_file->startNew();
    }

    Application::create(argc, argv)->run();
}

} // AJ_serial namespace

#endif // __AJ_SERIAL_HPP__
