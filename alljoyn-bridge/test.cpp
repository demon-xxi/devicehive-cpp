#include <alljoyn/BusAttachment.h>
//#include <alljoyn/ProxyBusObject.h>
//#include <alljoyn/BusObject.h>
//#include <alljoyn/InterfaceDescription.h>
//#include <alljoyn/DBusStd.h>
//#include <alljoyn/AllJoynStd.h>

#include <alljoyn/about/AnnounceHandler.h>
#include <alljoyn/about/AnnouncementRegistrar.h>

#include <boost/smart_ptr.hpp>

#include <stdexcept>
#include <iostream>
#include <sstream>

static const char* BUS_NAME = "DH_AJ";

/**
 * @brief The Application class.
 */
class Application:
    public ajn::BusListener,
    public ajn::services::AnnounceHandler,
    public ajn::ProxyBusObject::Listener,
    public ajn::SessionListener
{
public:

    /**
     * @brief The main constructor.
     */
    Application(int argc, const char **argv)
    {

    }


    /**
     * @brief Run the application.
     * @return Exit code.
     */
    int run()
    {
        AJ_init();

        for (int i = 0; i < 100; ++i)
        {
            if (!m_announcedDevices.empty())
            {
                AnnounceInfo info = m_announcedDevices.back();
                m_announcedDevices.pop_back();

                try
                {
                    checkBus(info.busName, info.port, info.objName);
                }
                catch (const std::exception &e)
                {
                    std::cerr << "failed to inspect \"" << info.busName << "\"\n";
                }
            }

            usleep(100000);
        }

        return 0;
    }

private:

    void checkBus(const qcc::String &busName, int port, const qcc::String &objName)
    {
        std::cerr << "checking \"" << objName << "\" object at bus \"" << busName << "\" port:" << port << "\n";

        ajn::SessionOpts opts(ajn::SessionOpts::TRAFFIC_MESSAGES, false, ajn::SessionOpts::PROXIMITY_ANY, ajn::TRANSPORT_ANY);
        ajn::SessionId sessionId = 0;
        QStatus status = m_AJ_bus->JoinSession(busName.c_str(), port, this, sessionId, opts);
        AJ_check(status, "cannot join session");
        std::cerr << "joined session id: " << sessionId << "\n";

        checkRemoteObject(busName, objName, sessionId);

        std::cerr << "\n\n";
    }

    void checkRemoteObject(const qcc::String &busName, const qcc::String &objPath, ajn::SessionId sessionId)
    {
        ajn::ProxyBusObject proxy_obj(*m_AJ_bus, busName.c_str(), objPath.c_str(), sessionId, false);
        if (proxy_obj.IsValid())
        {
            //std::cerr << "introspecting remote object later...\n";
            QStatus status = proxy_obj.IntrospectRemoteObject();
            AJ_check(status, "cannot introspect remote object");

            const ajn::InterfaceDescription* ifaces[100];
            int n = proxy_obj.GetInterfaces(ifaces, 100);
            std::cerr << "got " << n << " interfaces:\n";
            for (int i = 0; i < n; ++i)
            {
                const ajn::InterfaceDescription *iface = ifaces[i];
                std::cerr << "\t#" << i;
                checkInterface(&proxy_obj, iface);
            }
        }
        else
            std::cerr << "proxy object is invalid\n";
    }

    void checkInterface(ajn::ProxyBusObject *proxy_obj, const ajn::InterfaceDescription *iface)
    {
        std::cerr << " name: \"" << iface->GetName() << "\"\n";

        const ajn::InterfaceDescription::Member* members[1024];
        int n = iface->GetMembers(members, 1024);
        for (int i = 0; i < n; ++i)
        {
            const ajn::InterfaceDescription::Member *mb = members[i];
            std::cerr << "\t\ttype:" << (int)mb->memberType << ", name:\"" << mb->name << "\", signature:\""
                         << mb->signature << "\", returnSignature:\"" << mb->returnSignature << "\", argNames:\""
                            << mb->argNames << "\", description:\"" << mb->description << "\"\n";
        }

        const ajn::InterfaceDescription::Property* properties[1024];
        int m = iface->GetProperties(properties, 1024);
        for (int i = 0; i < m; ++i)
        {
            const ajn::InterfaceDescription::Property *p = properties[i];
            std::cerr << "\t\tproperty name:\"" << p->name << "\", signature:\""
                         << p->signature << "\", description:\"" << p->description << "\"\n";
        }
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

        // watch announcements
        status = ajn::services::AnnouncementRegistrar::RegisterAnnounceHandler(*m_AJ_bus, *this,
                                                                               NULL, 0);
        AJ_check(status, "failed to register announce handler");


    }


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

private: // AnnounceHandler

    struct AnnounceInfo
    {
        qcc::String busName;
        int         port;

        qcc::String objName;
    };

    std::vector<AnnounceInfo> m_announcedDevices;

    virtual void Announce(uint16_t version, uint16_t port, const char* busName,
                          const ObjectDescriptions& objectDescs,
                          const AboutData& aboutData)
    {
        std::cerr << "announce: version: " << version << ", port: " << port
                  << ", bus: \"" << busName << "\"\n";

        //m_AJ_bus->EnableConcurrentCallbacks();

        ObjectDescriptions::const_iterator od = objectDescs.begin();
        for (; od != objectDescs.end(); ++od)
        {
            std::cerr << "  object \"" << od->first << "\":\n";
            for (size_t i = 0; i < od->second.size(); ++i)
                std::cerr << "    interface \"" << od->second[i] << "\"\n";

            AnnounceInfo info;
            info.busName = busName;
            info.port    = port;
            info.objName = od->first;
            m_announcedDevices.push_back(info);
        }
    }

private: // Introspect callback

    virtual void my_IntrospectCB(QStatus status, ajn::ProxyBusObject* obj, void* context)
    {
        std::cerr << "introspect callback: " << status << "\n";
    }

private:
    boost::shared_ptr<ajn::BusAttachment> m_AJ_bus;
    //boost::shared_ptr<ajn::> m_AJ_obj;
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
