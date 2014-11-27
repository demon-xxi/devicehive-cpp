/**
 * @brief Test object (AboutService example based).
 */
#include <alljoyn/BusAttachment.h>
//#include <alljoyn/ProxyBusObject.h>
//#include <alljoyn/BusObject.h>
//#include <alljoyn/InterfaceDescription.h>
//#include <alljoyn/DBusStd.h>
//#include <alljoyn/AllJoynStd.h>

#include <alljoyn/about/AnnounceHandler.h>
#include <alljoyn/about/AnnouncementRegistrar.h>
#include <alljoyn/about/AboutPropertyStoreImpl.h>
#include <alljoyn/about/AboutServiceApi.h>

#include <boost/smart_ptr.hpp>

#include <stdexcept>
#include <iostream>
#include <sstream>

#include <stdio.h>

static const char* BUS_NAME = "DH_AJ";
const int SERVICE_PORT = 666;

const char *TEST_OBJ_PATH = "/my/test/object";
const char *IFACE_NAME = "com.devicehive.examples.ITest";


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
 * @brief The TestObj class.
 */
class TestObj:
    public ajn::BusObject
{
public:
    TestObj(ajn::BusAttachment &bus)
        : ajn::BusObject(TEST_OBJ_PATH)
    {
        QStatus status;

        ajn::InterfaceDescription *iface = 0;
        status = bus.CreateInterface(IFACE_NAME, iface, ajn::AJ_IFC_SECURITY_INHERIT);
        AJ_check(status, "unable to create interface");

        status = iface->AddMethod("TestMethod1", "ss", "s", "a,b,res");
        AJ_check(status, "unable to register method");

        status = iface->AddMethod("TestMethod2", "uu", "u", "a,b,res");
        AJ_check(status, "unable to register method");

        status = AddInterface(*iface, ajn::BusObject::ANNOUNCED);
        AJ_check(status, "unable to add interface");
        iface->Activate();

        status = AddMethodHandler(iface->GetMethod("TestMethod1"),
                                  (ajn::MessageReceiver::MethodHandler)&TestObj::do_TestMethod1);
        AJ_check(status, "unable to register method handler");

        status = AddMethodHandler(iface->GetMethod("TestMethod2"),
                                  (ajn::MessageReceiver::MethodHandler)&TestObj::do_TestMethod2);
        AJ_check(status, "unable to register method handler");

        std::cerr << "test object created\n";
    }

    ~TestObj()
    {
        std::cerr << "test object deleted\n";
    }

private:

    void do_TestMethod1(const ajn::InterfaceDescription::Member*, ajn::Message& message)
    {
        /* Concatenate the two input strings and reply with the result. */
        qcc::String a = message->GetArg(0)->v_string.str;
        qcc::String b = message->GetArg(1)->v_string.str;
        qcc::String res = a + b;

        std::cerr << "do TestMethod1 call: \"" << a << "\"+\"" << b << "\"=\"" << res << "\"\n";

        ajn::MsgArg out("s", res.c_str());
        QStatus status = MethodReply(message, &out, 1);
        AJ_check(status, "cannot send reply");
    }

    void do_TestMethod2(const ajn::InterfaceDescription::Member*, ajn::Message& message)
    {
        /* add two integers. */
        int a = message->GetArg(0)->v_uint32;
        int b = message->GetArg(1)->v_uint32;
        int res = a + b;

        std::cerr << "do TestMethod2 call: " << a << "+" << b << "=" << res << "\n";

        ajn::MsgArg out("u", res);
        QStatus status = MethodReply(message, &out, 1);
        AJ_check(status, "cannot send reply");
    }
};


/**
 * @brief The Application class.
 */
class Application:
    public ajn::BusListener,
    public ajn::SessionPortListener
{
public:

    /**
     * @brief The main constructor.
     */
    Application(int argc, const char **argv)
    {}


    /**
     * @brief Run the application.
     * @return Exit code.
     */
    int run()
    {
        AJ_init();

        while (1)
        {
            char ch = std::cin.get();
            if (ch == 'q')
                break;
        }

        return 0;
    }

private:

    void AJ_init()
    {
        //HIVELOG_TRACE(m_log_AJ, "creating BusAttachment");
        m_AJ_bus.reset(new ajn::BusAttachment(BUS_NAME, true));

        //HIVELOG_TRACE(m_log_AJ, "register bus listener");
        m_AJ_bus->RegisterBusListener(*this);
        QStatus status = m_AJ_bus->Start();
        AJ_check(status, "failed to start AllJoyn bus");

        // connect
        //HIVELOG_TRACE(m_log_AJ, "connecting");
        status = m_AJ_bus->Connect();
        AJ_check(status, "failed to connect AllJoyn bus");
        std::cerr << "connected to BUS: \"" << m_AJ_bus->GetUniqueName().c_str() << "\"\n";

        m_AJ_bus->RegisterBusListener(*this);

        AJ_initObj();

        ajn::services::AboutPropertyStoreImpl* props = new ajn::services::AboutPropertyStoreImpl();
        AJ_fillAbout(props);

        ajn::services::AboutServiceApi::Init(*m_AJ_bus, *props);
        if (!ajn::services::AboutServiceApi::getInstance())
            throw std::runtime_error("cannot create about service");

        status = ajn::services::AboutServiceApi::getInstance()->Register(SERVICE_PORT);
        AJ_check(status, "failed to register about service");

        status = m_AJ_bus->RegisterBusObject(*ajn::services::AboutServiceApi::getInstance());
        AJ_check(status, "failed to register about bus object");

        std::vector<qcc::String> interfaces;
        interfaces.push_back("com.devicehive.example.TestObj");
        status = ajn::services::AboutServiceApi::getInstance()->AddObjectDescription(TEST_OBJ_PATH, interfaces);
        AJ_check(status, "failed to add object description");


        ajn::SessionOpts opts(ajn::SessionOpts::TRAFFIC_MESSAGES, false, ajn::SessionOpts::PROXIMITY_ANY, ajn::TRANSPORT_ANY);
        ajn::SessionPort sp = SERVICE_PORT;
        status = m_AJ_bus->BindSessionPort(sp, opts, *this);
        AJ_check(status, "unable to bin service port");

        status = m_AJ_bus->AdvertiseName(m_AJ_bus->GetUniqueName().c_str(), ajn::TRANSPORT_ANY);
        AJ_check(status, "unable to advertise name");

        status = ajn::services::AboutServiceApi::getInstance()->Announce();
        AJ_check(status, "unable to announce");
    }


    void AJ_initObj()
    {
        QStatus status;
        m_AJ_obj.reset(new TestObj(*m_AJ_bus));

        status = m_AJ_bus->RegisterBusObject(*m_AJ_obj);
        AJ_check(status, "unable to register bus object");
    }


    void AJ_fillAbout(ajn::services::AboutPropertyStoreImpl *props)
    {
        props->setDeviceId("a461cbc0-763e-11e4-82f8-0800200c9a66");
        props->setAppId("b3feaee0-763e-11e4-82f8-0800200c9a66");

        std::vector<qcc::String> languages(1);
        languages[0] = "en";
        props->setSupportedLangs(languages);
        props->setDefaultLang("en");

        props->setAppName("Test Obj", "en");
        props->setModelNumber("WTF123");
        props->setDateOfManufacture("1999-01-01");
        props->setSoftwareVersion("0.0.0 build 1");
        props->setAjSoftwareVersion(ajn::GetVersion());
        props->setHardwareVersion("1.0a");

        props->setDeviceName("Test device name", "en");
        props->setDescription("This is an Alljoyn Application", "en");
        props->setManufacturer("DataArt", "en");

        props->setSupportUrl("http://www.devicehive.com");
    }

private: // ajn::BusListener implementation

    virtual void ListenerRegistered(ajn::BusAttachment *bus)
    {
        std::cerr << "listener registered for: \"" << bus->GetUniqueName().c_str() << "\"\n";
    }

    virtual void ListenerUnregistered()
    {
        std::cerr << "listener unregistered\n";
    }

    virtual void FoundAdvertisedName(const char* name, ajn::TransportMask transport, const char* namePrefix)
    {
        std::cerr << "found advertized name: \"" << name << "\", prefix: \"" << namePrefix << "\"\n";
    }

    virtual void LostAdvertisedName(const char* name, ajn::TransportMask transport, const char* namePrefix)
    {
        std::cerr << "lost advertized name: \"" << name << "\", prefix: \"" << namePrefix << "\"\n";
    }

    virtual void NameOwnerChanged(const char* busName, const char* previousOwner, const char* newOwner)
    {
        std::cerr << "name owner changed, bus name: \"" << (busName?busName:"<null>")
                  << "\", from: \"" << (previousOwner?previousOwner:"<null>")
                  << "\", to: \"" << (newOwner?newOwner:"<null>")
                  << "\"\n";
    }

    virtual void PropertyChanged(const char* propName, const ajn::MsgArg* propValue)
    {
        std::cerr << "property changed, name: \"" << propName << "\"\n";
    }

    virtual void BusStopping()
    {
        std::cerr << "bus stopping\n";
    }

    virtual void BusDisconnected()
    {
        std::cerr << "bus disconnected\n";
    }

private: // SessionPortListener

    virtual bool AcceptSessionJoiner(ajn::SessionPort sessionPort, const char* joiner, const ajn::SessionOpts& opts)
    {
        if (sessionPort != SERVICE_PORT)
        {
            std::cerr << "rejecting join attempt on unexpected session port " << sessionPort << "\n";
            return false;
        }

        std::cout << "accepting join attempt from \"" << joiner << "\"\n";
        return true;
    }

    virtual void SessionJoined(ajn::SessionPort sessionPort, ajn::SessionId id, const char* joiner)
    {
        std::cerr << "session #" << id << " joined on "
                  << sessionPort << " port (joiner: \""
                    << joiner << "\")\n";
    }


private:
    boost::shared_ptr<ajn::BusAttachment> m_AJ_bus;
    boost::shared_ptr<TestObj> m_AJ_obj;
};


/**
 * @brief The application entry point.
 */
int main(int argc, const char **argv)
{
    try
    {
        Application app(argc, argv);
        return app.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << "ERROR: " << e.what() << "\n";
        return -1;
    }
}
