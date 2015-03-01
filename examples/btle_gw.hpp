/** @file
@brief The BTLE gateway example.
@author Sergey Polichnoy <sergey.polichnoy@dataart.com>
@see @ref page_simple_gw
*/
#ifndef __EXAMPLES_BTLE_GW_HPP_
#define __EXAMPLES_BTLE_GW_HPP_

#include "bluepy.hpp"

#include <DeviceHive/restful.hpp>
#include <DeviceHive/websocket.hpp>
#include "basic_app.hpp"

// 'libbluetooth-dev' should be installed
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include <alljoyn/BusAttachment.h>
#include <alljoyn/about/AnnounceHandler.h>
#include <alljoyn/about/AnnouncementRegistrar.h>
#include <alljoyn/about/AboutPropertyStoreImpl.h>
#include <alljoyn/about/AboutServiceApi.h>

#include <alljoyn/controlpanel/ControlPanelService.h>
#include <alljoyn/controlpanel/ControlPanelControllee.h>
#include <alljoyn/controlpanel/LanguageSets.h>
#include <alljoyn/controlpanel/Label.h>
#include <alljoyn/controlpanel/Property.h>
#include <alljoyn/controlpanel/Action.h>

// callback with context wrappers: http://www.p-nand-q.com/programming/cplusplus/using_member_functions_with_c_function_pointers.html
namespace ctx_cb
{

typedef const char* (*LPFN_GetCharCallback0)();
typedef const char* (*LPFN_GetCharCallback1)(void *context);


class CallbackBase
{
public:

    /**
     * @param cb pointer to a unique C callback.
     */
    CallbackBase(LPFN_GetCharCallback0 cb)
        : m_ctx(0)
        , m_cb0(cb)
        , m_cb1(0)
    {}

    // when free, allocate this callback
    LPFN_GetCharCallback0 aquire(void* ctx, LPFN_GetCharCallback1 cb)
    {
        if (m_ctx)
            return 0; // already used

        m_ctx = ctx;
        m_cb1 = cb;
        return m_cb0;
    }

    // when done, remove allocation of the callback
    void release()
    {
        m_ctx = 0;
        m_cb1 = 0;
    }

protected:
    static const char* invoke(int slot);

private:
    void *m_ctx;
    LPFN_GetCharCallback0 m_cb0;
    LPFN_GetCharCallback1 m_cb1;
};


template <int slot>
class DynamicCallback:
    public CallbackBase
{
public:
    DynamicCallback()
        : CallbackBase(&DynamicCallback<slot>::invoke)
    {}

private:
    static const char* invoke()
    {
        return CallbackBase::invoke(slot);
    }
};


class FunctionCallback
{
public:
    FunctionCallback(void *context, LPFN_GetCharCallback1 cb)
        : m_cb0(0)
    {
        CallbackBase **ss = get_slots();

        for (m_alloc_index = 0; ss && *ss; ++ss, ++m_alloc_index)
        {
            m_cb0 = (*ss)->aquire(context, cb);
            if (m_cb0)
                break;
        }

        if (!m_cb0) throw std::runtime_error("no more free slots for callback");
    }

    ~FunctionCallback()
    {
        if (isValid())
        {
            get_slots()[m_alloc_index]->release();
        }
    }

public:
    operator LPFN_GetCharCallback0() const
    {
        return m_cb0;
    }

    bool isValid() const
    {
        return m_cb0 != 0;
    }

public:

    /**
     * @brief get array of available slots (NULL-terminated).
     */
    static CallbackBase** get_slots()
    {
        static CallbackBase* g_slots[] =
        {
            new DynamicCallback<0x00>(),
            new DynamicCallback<0x01>(),
            new DynamicCallback<0x02>(),
            new DynamicCallback<0x03>(),
            new DynamicCallback<0x04>(),
            new DynamicCallback<0x05>(),
            new DynamicCallback<0x06>(),
            new DynamicCallback<0x07>(),
            new DynamicCallback<0x08>(),
            new DynamicCallback<0x09>(),
            new DynamicCallback<0x0A>(),
            new DynamicCallback<0x0B>(),
            new DynamicCallback<0x0C>(),
            new DynamicCallback<0x0D>(),
            new DynamicCallback<0x0E>(),
            new DynamicCallback<0x0F>(),
            new DynamicCallback<0x10>(),
            new DynamicCallback<0x11>(),
            new DynamicCallback<0x12>(),
            new DynamicCallback<0x13>(),
            new DynamicCallback<0x14>(),
            new DynamicCallback<0x15>(),
            new DynamicCallback<0x16>(),
            new DynamicCallback<0x17>(),
            new DynamicCallback<0x18>(),
            new DynamicCallback<0x19>(),
            new DynamicCallback<0x1A>(),
            new DynamicCallback<0x1B>(),
            new DynamicCallback<0x1C>(),
            new DynamicCallback<0x1D>(),
            new DynamicCallback<0x1E>(),
            new DynamicCallback<0x1F>(),
            new DynamicCallback<0x20>(),
            new DynamicCallback<0x21>(),
            new DynamicCallback<0x22>(),
            new DynamicCallback<0x23>(),
            new DynamicCallback<0x24>(),
            new DynamicCallback<0x25>(),
            new DynamicCallback<0x26>(),
            new DynamicCallback<0x27>(),
            new DynamicCallback<0x28>(),
            new DynamicCallback<0x29>(),
            new DynamicCallback<0x2A>(),
            new DynamicCallback<0x2B>(),
            new DynamicCallback<0x2C>(),
            new DynamicCallback<0x2D>(),
            new DynamicCallback<0x2E>(),
            new DynamicCallback<0x2F>(),
            new DynamicCallback<0x30>(),
            new DynamicCallback<0x31>(),
            new DynamicCallback<0x32>(),
            new DynamicCallback<0x33>(),
            new DynamicCallback<0x34>(),
            new DynamicCallback<0x35>(),
            new DynamicCallback<0x36>(),
            new DynamicCallback<0x37>(),
            new DynamicCallback<0x38>(),
            new DynamicCallback<0x39>(),
            new DynamicCallback<0x3A>(),
            new DynamicCallback<0x3B>(),
            new DynamicCallback<0x3C>(),
            new DynamicCallback<0x3D>(),
            new DynamicCallback<0x3E>(),
            new DynamicCallback<0x3F>(),
            // TODO: more slots!!!
            0 // NULL-terminated
        };

        return g_slots;
    }

private:
    LPFN_GetCharCallback0 m_cb0;
    int m_alloc_index;

private: // NonCopyable
    FunctionCallback(const FunctionCallback&);
    FunctionCallback& operator=(const FunctionCallback&);
};


/**
 * @brief Invoke slot's callback.
 */
const char* CallbackBase::invoke(int slot)
{
    CallbackBase *s = FunctionCallback::get_slots()[slot];
    return s->m_cb1(s->m_ctx);
}

} // ctx_cb


namespace bluetooth
{
    using namespace hive;

/**
 * @brief The bluetooth Device.
 *
 * Supports scan method.
 */
class Device:
    public boost::enable_shared_from_this<Device>,
    private NonCopyable
{
protected:

    /// @brief Main constructor.
    Device(boost::asio::io_service &ios, const String &name)
        : scan_filter_dup(0x01)
        , scan_filter_type(0)
        , scan_filter_old_valid(false)
        , m_scan_active(false)
        , m_read_active(false)
        , m_ios(ios)
        , m_name(name)
        , m_dev_id(-1)
        , m_dd(-1)
        , m_stream(ios)
    {
        memset(&m_dev_addr, 0, sizeof(m_dev_addr));
    }

public:
    typedef boost::shared_ptr<Device> SharedPtr;

    /// @brief The factory method.
    static SharedPtr create(boost::asio::io_service &ios, const String &name)
    {
        return SharedPtr(new Device(ios, name));
    }

public:

    /// @brief Get IO service.
    boost::asio::io_service& get_io_service()
    {
        return m_ios;
    }


    /// @brief Get bluetooth device name.
    const String& getDeviceName() const
    {
        return m_name;
    }


    /// @brief Get device identifier.
    int getDeviceId() const
    {
        return m_dev_id;
    }


    /// @brief Get bluetooth address as string.
    String getDeviceAddressStr() const
    {
        char str[64];
        ba2str(&m_dev_addr, str);
        return String(str);
    }


    /// @brief Get device info.
    json::Value getDeviceInfo() const
    {
        hci_dev_info info;
        memset(&info, 0, sizeof(info));

        if (hci_devinfo(m_dev_id, &info) < 0)
            throw std::runtime_error("cannot get device info");

        return info2json(info);
    }


    /// @brief Convert device info to JSON value.
    static json::Value info2json(const hci_dev_info &info)
    {
        json::Value res;
        res["id"] = (int)info.dev_id;
        res["name"] = String(info.name);

        char *flags = hci_dflagstostr(info.flags);
        res["flags"] = boost::trim_copy(String(flags));
        bt_free(flags);

        char addr[64];
        ba2str(&info.bdaddr, addr);
        res["addr"] = String(addr);

        return res;
    }

    /// @brief Get info for all devices.
    static json::Value getDevicesInfo()
    {
        struct Aux
        {
            static int collect(int, int dev_id, long arg)
            {
                hci_dev_info info;
                memset(&info, 0, sizeof(info));

                if (hci_devinfo(dev_id, &info) < 0)
                    throw std::runtime_error("cannot get device info");

                json::Value *res = reinterpret_cast<json::Value*>(arg);
                res->append(info2json(info));

                return 0;
            }
        };

        json::Value res(json::Value::TYPE_ARRAY);
        hci_for_each_dev(0, Aux::collect,
                         reinterpret_cast<long>(&res));
        return res;
    }

public:
    uint8_t scan_filter_dup;
    uint8_t scan_filter_type;
    hci_filter scan_filter_old;
    bool scan_filter_old_valid;

    typedef boost::function2<void,String,String> ScanCallback;

    /**
     * @brief Start scan operation.
     */
    void scanStart(const json::Value &opts, ScanCallback cb)
    {
        uint8_t own_type = LE_PUBLIC_ADDRESS;
        uint8_t scan_type = 0x01;
        uint8_t filter_policy = 0x00;
        uint16_t interval = htobs(0x0010);
        uint16_t window = htobs(0x0010);

        const json::Value &j_dup = opts["duplicates"];
        if (!j_dup.isNull())
        {
            if (j_dup.isConvertibleToInteger())
                scan_filter_dup = (j_dup.asInt() != 0);
            else
            {
                String s = j_dup.asString();
                if (boost::iequals(s, "yes"))
                    scan_filter_dup = 0x00;     // don't filter - has duplicates
                else if (boost::iequals(s, "no"))
                    scan_filter_dup = 0x01;     // filter - no duplicates
                else
                    throw std::runtime_error("unknown duplicates value");
            }
        }
        else
            scan_filter_dup = 0x01; // filter by default

        const json::Value &j_priv = opts["privacy"];
        if (!j_priv.isNull())
        {
            if (j_priv.isConvertibleToInteger())
            {
                if (j_priv.asInt())
                    own_type = LE_RANDOM_ADDRESS;
            }
            else
            {
                String s = j_priv.asString();

                if (boost::iequals(s, "enable") || boost::iequals(s, "enabled"))
                    own_type = LE_RANDOM_ADDRESS;
                else if (boost::iequals(s, "disable") || boost::iequals(s, "disabled"))
                    ;
                else
                    throw std::runtime_error("unknown privacy value");
            }
        }

        const json::Value &j_type = opts["type"];
        if (!j_type.isNull())
        {
            if (j_type.isConvertibleToInteger())
                scan_type = j_type.asUInt8();
            else
            {
                String s = j_type.asString();

                if (boost::iequals(s, "active"))
                    scan_type = 0x01;
                else if (boost::iequals(s, "passive"))
                    scan_type = 0x00;
                else
                    throw std::runtime_error("unknown scan type value");
            }
        }

        const json::Value &j_pol = opts["policy"];
        if (!j_pol.isNull())
        {
            if (j_pol.isConvertibleToInteger())
                filter_policy = j_pol.asUInt8();
            else
            {
                String s = j_pol.asString();

                if (boost::iequals(s, "whitelist"))
                    filter_policy = 0x01;
                else if (boost::iequals(s, "none"))
                    filter_policy = 0x00;
                else
                    throw std::runtime_error("unknown filter policy value");
            }
        }

//switch (opt) {
//case 'd':
//    filter_type = optarg[0];
//    if (filter_type != 'g' && filter_type != 'l') {
//        fprintf(stderr, "Unknown discovery procedure\n");
//        exit(1);
//    }

//    interval = htobs(0x0012);
//    window = htobs(0x0012);
//    break;
//}

        int err = hci_le_set_scan_parameters(m_dd, scan_type, interval, window,
                                             own_type, filter_policy, 10000);
        if (err < 0) throw std::runtime_error("failed to set scan parameters");

        err = hci_le_set_scan_enable(m_dd, 0x01, scan_filter_dup, 10000);
        if (err < 0) throw std::runtime_error("failed to enable scan");
        m_scan_active = true;

        socklen_t len = sizeof(scan_filter_old);
        err = getsockopt(m_dd, SOL_HCI, HCI_FILTER, &scan_filter_old, &len);
        if (err < 0) throw std::runtime_error("failed to get filter option");
        scan_filter_old_valid = true;

        hci_filter nf;
        hci_filter_clear(&nf);
        hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
        hci_filter_set_event(EVT_LE_META_EVENT, &nf);

        err = setsockopt(m_dd, SOL_HCI, HCI_FILTER, &nf, sizeof(nf));
        if (err < 0) throw std::runtime_error("failed to set filter option");

        m_scan_devices.clear(); // new search...
        m_scan_cb = cb;
    }


    /**
     * @brief Stop scan operation.
     */
    void scanStop()
    {
        if (scan_filter_old_valid) // revert filter back
        {
            setsockopt(m_dd, SOL_HCI, HCI_FILTER, &scan_filter_old, sizeof(scan_filter_old));
            scan_filter_old_valid = false;
        }

        if (m_scan_active)
        {
            m_scan_active = false;
            int err = hci_le_set_scan_enable(m_dd, 0x00, scan_filter_dup, 10000);
            if (err < 0) throw std::runtime_error("failed to disable scan");
        }

        m_scan_cb = ScanCallback();
    }

public:
    void asyncReadSome()
    {
        if (!m_read_active && m_stream.is_open())
        {
            m_read_active = true;
            boost::asio::async_read(m_stream, m_read_buf,
                boost::asio::transfer_at_least(1),
                boost::bind(&Device::onReadSome, this->shared_from_this(),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
        }
    }

    void readStop()
    {
        if (m_read_active && m_stream.is_open())
        {
            m_read_active = false;
            m_stream.cancel();
        }
    }

    json::Value getFoundDevices() const
    {
        json::Value res(json::Value::TYPE_OBJECT);

        std::map<String, String>::const_iterator i = m_scan_devices.begin();
        for (; i != m_scan_devices.end(); ++i)
        {
            const String &MAC = i->first;
            const String &name = i->second;

            res[MAC] = name;
        }

        return res;
    }

private:
    bool m_scan_active;
    bool m_read_active;
    ScanCallback m_scan_cb;
    boost::asio::streambuf m_read_buf;
    std::map<String, String> m_scan_devices;

    void onReadSome(boost::system::error_code err, size_t len)
    {
        m_read_active = false;
        HIVE_UNUSED(len);

//            std::cerr << "read " << len << " bytes, err:" << err << "\n";
//            std::cerr << "dump:" << dumphex(m_read_buf.data()) << "\n";

        if (!err)
        {
            const uint8_t *buf = boost::asio::buffer_cast<const uint8_t*>(m_read_buf.data());
            size_t buf_len = m_read_buf.size();
            size_t len = buf_len;

            const uint8_t *ptr = buf + (1 + HCI_EVENT_HDR_SIZE);
            len -= (1 + HCI_EVENT_HDR_SIZE);

            evt_le_meta_event *meta = (evt_le_meta_event*)ptr;
            if (meta->subevent == 0x02)
            {
                le_advertising_info *info = (le_advertising_info *)(meta->data + 1);
                // TODO: if (check_report_filter(filter_type, info))
                {
                    char addr[64];
                    ba2str(&info->bdaddr, addr);
                    String name = parse_name(info->data, info->length);

                    if (!name.empty())
                        m_scan_devices[addr] = name;
                    else if (m_scan_devices.find(addr) == m_scan_devices.end())
                        m_scan_devices[addr] = "(unknown)"; // save as unknown if not exists

                    if (!name.empty() && m_scan_cb)
                        m_ios.post(boost::bind(m_scan_cb, String(addr), name));
                }
            }

            m_read_buf.consume(buf_len);
            asyncReadSome(); // continue
        }
    }


    template<typename Buf>
    static String dumphex(const Buf &buf)
    {
        return dump::hex(
            boost::asio::buffers_begin(buf),
            boost::asio::buffers_end(buf));
    }

    static String parse_name(uint8_t *eir, size_t eir_len)
    {
        #define EIR_NAME_SHORT              0x08  /* shortened local name */
        #define EIR_NAME_COMPLETE           0x09  /* complete local name */

        size_t offset;

        offset = 0;
        while (offset < eir_len)
        {
            uint8_t field_len = eir[0];
            size_t name_len;

            // Check for the end of EIR
            if (field_len == 0)
                break;

            if (offset + field_len > eir_len)
                break;

            switch (eir[1])
            {
            case EIR_NAME_SHORT:
            case EIR_NAME_COMPLETE:
                name_len = field_len - 1;
                return String((const char*)&eir[2], name_len);
            }

            offset += field_len + 1;
            eir += field_len + 1;
        }

        return String();
    }

public:

    /// @brief The "open" operation callback type.
    typedef boost::function1<void, boost::system::error_code> OpenCallback;


    /// @brief Is device open?
    bool is_open() const
    {
        return m_dd >= 0 && m_stream.is_open();
    }


    /// @brief Open device asynchronously.
    void async_open(OpenCallback callback)
    {
        boost::system::error_code err;

        if (!m_name.empty())
            m_dev_id = hci_devid(m_name.c_str());
        else
            m_dev_id = hci_get_route(NULL);

        if (m_dev_id >= 0)
        {
            if (hci_devba(m_dev_id, &m_dev_addr) >= 0)
            {
                m_dd = hci_open_dev(m_dev_id);
                if (m_dd >= 0)
                    m_stream.assign(m_dd);
                else
                    err = boost::system::error_code(errno, boost::system::system_category());
            }
            else
                err = boost::system::error_code(errno, boost::system::system_category());
        }
        else
            err = boost::system::error_code(errno, boost::system::system_category());

        m_ios.post(boost::bind(callback, err));
    }


    /// @brief Close device.
    void close()
    {
        m_stream.release();

        if (m_dd >= 0)
            hci_close_dev(m_dd);

        m_dd = -1;
        m_dev_id = -1;
        memset(&m_dev_addr, 0,
            sizeof(m_dev_addr));
    }

private:
    boost::asio::io_service &m_ios;
    String m_name;

    bdaddr_t m_dev_addr; ///< @brief The BT address.
    int m_dev_id;        ///< @brief The device identifier.
    int m_dd;            ///< @brief The device descriptor.

    boost::asio::posix::stream_descriptor m_stream;
};

/// @brief The shared pointer type.
typedef Device::SharedPtr DevicePtr;

} // bluetooth namespace


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


namespace alljoyn
{
    using namespace hive;


static const char* BUS_NAME = "AllJoyn-GATT";
const int SERVICE_PORT = 777;

const char *MANAGER_OBJ_PATH = "/Manager";
const char *MANAGER_IFACE_NAME = "com.devicehive.gatt.Manager";
const char *RAW_IFACE_NAME = "com.devicehive.gatt.RAW";


/**
 * @brief The manager class.
 */
class ManagerObj: public ajn::BusObject,
        public boost::enable_shared_from_this<ManagerObj>
{
public:
    ManagerObj(boost::asio::io_service &ios,
               ajn::BusAttachment &bus, bluepy::IPeripheralList *plist,
               basic_app::DelayedTaskList::SharedPtr delayed,
               bluetooth::DevicePtr bt_dev)
        : ajn::BusObject(MANAGER_OBJ_PATH)
        , m_ios(ios)
        , m_plist(plist)
        , m_delayed(delayed)
        , m_bt_dev(bt_dev)
        , m_controllee(0)
        , m_log("/alljoyn/gatt/Manager")
    {
        QStatus status;

        if (1) // manager
        {
            ajn::InterfaceDescription *iface = Manager_createInterface(bus);
            status = AddInterface(*iface/*, ajn::BusObject::ANNOUNCED*/);
            AJ_check(status, "unable to add interface");
            iface->Activate();

            Manager_attachToInterface(iface);
        }

        if (1) // RAW
        {
            ajn::InterfaceDescription *iface = RAW_createInterface(bus);
            status = AddInterface(*iface/*, ajn::BusObject::ANNOUNCED*/);
            AJ_check(status, "unable to add interface");
            iface->Activate();

            RAW_attachToInterface(iface);
        }

        HIVELOG_TRACE(m_log, "created");
    }

    ~ManagerObj()
    {
        HIVELOG_TRACE(m_log, "deleted");
    }

private:

    /**
     * @brief Create Manager interface.
     */
    ajn::InterfaceDescription* Manager_createInterface(ajn::BusAttachment &bus)
    {
        QStatus status = ER_OK;
        ajn::InterfaceDescription *iface = 0;

        status = bus.CreateInterface(MANAGER_IFACE_NAME, iface, ajn::AJ_IFC_SECURITY_INHERIT);
        AJ_check(status, "unable to create interface");

        status = iface->AddMethod("createDevice", "ss", "u", "MAC,meta,result");
        AJ_check(status, "unable to register method");

        status = iface->AddMethod("deleteDevice", "s", "u", "MAC,result");
        AJ_check(status, "unable to register method");

        status = iface->AddMethod("getDeviceList", "", "as", "result");
        AJ_check(status, "unable to register method");

        return iface;
    }

    void Manager_attachToInterface(ajn::InterfaceDescription *iface)
    {
        QStatus status = ER_OK;

        status = AddMethodHandler(iface->GetMethod("createDevice"),
                                  (ajn::MessageReceiver::MethodHandler)&ManagerObj::do_createDevice);
        AJ_check(status, "unable to register method handler");

        status = AddMethodHandler(iface->GetMethod("deleteDevice"),
                                  (ajn::MessageReceiver::MethodHandler)&ManagerObj::do_deleteDevice);
        AJ_check(status, "unable to register method handler");

        status = AddMethodHandler(iface->GetMethod("getDeviceList"),
                                  (ajn::MessageReceiver::MethodHandler)&ManagerObj::do_getDeviceList);
        AJ_check(status, "unable to register method handler");
    }

    /**
     * @brief Create RAW interface.
     */
    ajn::InterfaceDescription* RAW_createInterface(ajn::BusAttachment &bus)
    {
        QStatus status = ER_OK;
        ajn::InterfaceDescription *iface = 0;

        status = bus.CreateInterface(RAW_IFACE_NAME, iface, ajn::AJ_IFC_SECURITY_INHERIT);
        AJ_check(status, "unable to create interface");

        status = iface->AddMethod("scanDevices", "u", "a{ss}", "timeout_ms,result");
        AJ_check(status, "unable to register method");

        status = iface->AddMethod("getServices", "s", "a(suu)", "MAC,result");
        AJ_check(status, "unable to register method");

        status = iface->AddMethod("getCharacteristics", "s", "a(suuu)", "MAC,result");
        AJ_check(status, "unable to register method");

        status = iface->AddMethod("read", "su", "s", "MAC,handle,result");
        AJ_check(status, "unable to register method");

        status = iface->AddMethod("write", "subs", "u", "MAC,handle,withResponse,value,result");
        AJ_check(status, "unable to register method");

        status = iface->AddMethod("connect", "s", "u", "MAC,result");
        AJ_check(status, "unable to register method");

        status = iface->AddMethod("disconnect", "s", "u", "MAC,result");
        AJ_check(status, "unable to register method");

        status = iface->AddMethod("status", "s", "u", "MAC,result");
        AJ_check(status, "unable to register method");

        return iface;
    }

    void RAW_attachToInterface(ajn::InterfaceDescription *iface)
    {
        QStatus status = ER_OK;

        status = AddMethodHandler(iface->GetMethod("scanDevices"),
                                  (ajn::MessageReceiver::MethodHandler)&ManagerObj::do_scanDevices);
        AJ_check(status, "unable to register method handler");

        status = AddMethodHandler(iface->GetMethod("getServices"),
                                  (ajn::MessageReceiver::MethodHandler)&ManagerObj::do_getServices);
        AJ_check(status, "unable to register method handler");

        status = AddMethodHandler(iface->GetMethod("getCharacteristics"),
                                  (ajn::MessageReceiver::MethodHandler)&ManagerObj::do_getCharacteristics);
        AJ_check(status, "unable to register method handler");

        status = AddMethodHandler(iface->GetMethod("read"),
                                  (ajn::MessageReceiver::MethodHandler)&ManagerObj::do_read);
        AJ_check(status, "unable to register method handler");

        status = AddMethodHandler(iface->GetMethod("write"),
                                  (ajn::MessageReceiver::MethodHandler)&ManagerObj::do_write);
        AJ_check(status, "unable to register method handler");

        // TODO: connect/disconnect/status
    }

public:

    /**
     * @brief Build ControlPanel controllee related to manager object.
    */
    ajn::services::ControlPanelControllee* getControllee()
    {
        using namespace ajn::services;
        LanguageSet lang_set("btle_gw_lang_set");
        lang_set.addLanguage("en");

        if (!m_controllee)
        {
            m_controllee = new ControlPanelControllee();
            QStatus status;

            LanguageSets::add(lang_set.getLanguageSetName(), lang_set);

            ControlPanelControlleeUnit *unit = new ControlPanelControlleeUnit("Device_Manager");
            status = m_controllee->addControlPanelUnit(unit);
            AJ_check(status, "cannot add controlpanel unit");

            ControlPanel *root_cp = ControlPanel::createControlPanel(&lang_set);
            if (!root_cp) throw std::runtime_error("cannot create controlpanel");
            status = unit->addControlPanel(root_cp);
            AJ_check(status, "cannot add root controlpanel");

            Container *root = new Container("root", NULL);
            status = root_cp->setRootWidget(root);
            AJ_check(status, "cannot set root widget");
            root->setEnabled(true);
            root->setIsSecured(false);
            root->setBgColor(0x200);
            if (1)
            {
                std::vector<qcc::String> v;
                v.push_back("Device management");
                root->setLabels(v);
            }
            if (1)
            {
                std::vector<uint16_t> v;
                v.push_back(VERTICAL_LINEAR);
                v.push_back(HORIZONTAL_LINEAR);
                root->setHints(v);
            }

            class AddressProperty: public Property
            {
            public:
                AddressProperty(const qcc::String &name, Widget *root)
                    : Property(name, root, STRING_PROPERTY)
                {
                    m_MAC_addr = "B4:99:4C:64:B0:AC"; // "AA:BB:CC:DD:EE:FF";
                    m_cb = new ctx_cb::FunctionCallback(this, &AddressProperty::getAddr);
                    if (!m_cb->isValid()) throw std::runtime_error("no free callback slot");
                    setGetValue(*m_cb);
                }

                ~AddressProperty()
                {
                    delete m_cb;
                }

                QStatus setValue(const char *value)
                {
                    std::cerr << "changing MAC address to: "
                              << value << "\n";

                    if (m_MAC_addr != value)
                    {
                        m_MAC_addr = value;
                        std::cerr << "change MAC address to: "
                                  << m_MAC_addr.c_str() << "\n";
//                        QStatus s = Property::setValue(value);
//                        if (s != ER_OK)
//                            return s;
                        return Property::SendValueChangedSignal();
                    }
                    return ER_OK;
                }

                const String& getAddrRef() const
                {
                    return m_MAC_addr;
                }

            private:

                static const char* getAddr(void *ctx)
                {
                    AddressProperty *pthis = reinterpret_cast<AddressProperty*>(ctx);
                    std::cerr << "getting MAC address: " << pthis->m_MAC_addr << "\n";
                    return pthis->m_MAC_addr.c_str();
                }

            private:
                ctx_cb::FunctionCallback *m_cb;
                String m_MAC_addr;
            };

            AddressProperty *MAC_prop = new AddressProperty("MAC_prop", root);
            status = root->addChildWidget(MAC_prop);
            AJ_check(status, "cannot add MAC property");
            MAC_prop->setEnabled(true);
            MAC_prop->setIsSecured(false);
            MAC_prop->setWritable(true);
            MAC_prop->setBgColor(0x500);
            if (1)
            {
                std::vector<qcc::String> v;
                v.push_back("MAC address:");
                MAC_prop->setLabels(v);
            }
            if (1)
            {
                std::vector<uint16_t> v;
                v.push_back(EDITTEXT);
                MAC_prop->setHints(v);
            }

            Container *line = new Container("line", root);
            status = root->addChildWidget(line);
            AJ_check(status, "cannot add line");
            line->setEnabled(true);
            line->setIsSecured(false);
            if (1)
            {
                std::vector<uint16_t> v;
                //v.push_back(VERTICAL_LINEAR);
                v.push_back(HORIZONTAL_LINEAR);
                line->setHints(v);
            }


            class CreateAction: public Action
            {
            public:
                CreateAction(const qcc::String &name, Widget* root,
                             const String &MAC_addr, ManagerObj *owner,
                             boost::asio::io_service &ios)
                    : Action(name, root)
                    , m_MAC_addr(MAC_addr)
                    , m_owner(owner)
                    , m_ios(ios)
                {}

                virtual ~CreateAction()
                {}

                bool executeCallBack()
                {
                    std::cerr << "creating device \"" << m_MAC_addr << "\"\n";
//                    m_ios.post(boost::bind(&ManagerObj::impl_createDevice, m_owner,
//                                m_MAC_addr, json::Value())); // no meta info
                    return true;
                }

            private:
                const String &m_MAC_addr;
                ManagerObj *m_owner;
                boost::asio::io_service &m_ios;
            };

            Action *NEW_action = new CreateAction("NEW_action", root,
                                MAC_prop->getAddrRef(), this, m_ios);
            status = line->addChildWidget(NEW_action);
            AJ_check(status, "cannot add NEW action");
            NEW_action->setEnabled(true);
            NEW_action->setIsSecured(false);
            NEW_action->setBgColor(0x400);
            if (1)
            {
                std::vector<qcc::String> v;
                v.push_back("Create");
                NEW_action->setLabels(v);
            }
            if (1)
            {
                std::vector<uint16_t> v;
                v.push_back(ACTIONBUTTON);
                NEW_action->setHints(v);
            }

            class DeleteAction: public Action
            {
            public:
                DeleteAction(const qcc::String &name, Widget* root,
                             const String &MAC_addr, ManagerObj *owner,
                             boost::asio::io_service &ios)
                    : Action(name, root)
                    , m_MAC_addr(MAC_addr)
                    , m_owner(owner)
                    , m_ios(ios)
                {}

                virtual ~DeleteAction()
                {}

                bool executeCallBack()
                {
                    std::cerr << "deleting device \"" << m_MAC_addr << "\"\n";
//                    m_ios.post(boost::bind(&ManagerObj::impl_deleteDevice,
//                                m_owner, m_MAC_addr));
                    return true;
                }

            private:
                const String &m_MAC_addr;
                ManagerObj *m_owner;
                boost::asio::io_service &m_ios;
            };

            Action *DEL_action = new DeleteAction("DEL_action", root,
                                MAC_prop->getAddrRef(), this, m_ios);
            status = line->addChildWidget(DEL_action);
            AJ_check(status, "cannot add DEL action");
            DEL_action->setEnabled(true);
            DEL_action->setIsSecured(false);
            DEL_action->setBgColor(0x400);
            if (1)
            {
                std::vector<qcc::String> v;
                v.push_back("Delete");
                DEL_action->setLabels(v);
            }
            if (1)
            {
                std::vector<uint16_t> v;
                v.push_back(ACTIONBUTTON);
                DEL_action->setHints(v);
            }



            class TestAction: public Action
            {
            public:
                TestAction(const qcc::String &name, Widget* root,
                           AddressProperty *edit)
                    : Action(name, root)
                    , m_edit(edit)
                {}

                virtual ~TestAction()
                {}

                bool executeCallBack()
                {
                    std::cerr << "resetting MAC\n";
                    String value = "00:00:00:00:00:00";
                    m_edit->setValue(value.c_str());
//                    m_ios.post(boost::bind(&ManagerObj::impl_createDevice, m_owner,
//                                m_MAC_addr, json::Value())); // no meta info
                    return true;
                }

            private:
                AddressProperty *m_edit;
            };

            Action *TEST_action = new TestAction("TEST_action", root, MAC_prop);
            status = line->addChildWidget(TEST_action);
            AJ_check(status, "cannot add TEST action");
            TEST_action->setEnabled(true);
            TEST_action->setIsSecured(false);
            TEST_action->setBgColor(0x400);
            if (1)
            {
                std::vector<qcc::String> v;
                v.push_back("Test");
                TEST_action->setLabels(v);
            }
            if (1)
            {
                std::vector<uint16_t> v;
                v.push_back(ACTIONBUTTON);
                TEST_action->setHints(v);
            }
        }

        return m_controllee;
    }

private:

    class HexProperty: public ajn::services::Property
    {
    public:
        HexProperty(const qcc::String &name, ajn::services::Widget *root, UInt32 handle)
            : ajn::services::Property(name, root, ajn::services::STRING_PROPERTY)
            , m_handle(handle)
        {
            m_value = "";
            m_cb = new ctx_cb::FunctionCallback(this, &HexProperty::getVal);
            if (!m_cb->isValid()) throw std::runtime_error("no free callback slot");
            setGetValue(*m_cb);
        }

        ~HexProperty()
        {
            delete m_cb;
        }

        QStatus setValue(const char *value)
        {
            if (m_value != value)
            {
                m_value = value;
                std::cerr << "setting value to: "
                          << m_value.c_str() << "\n";

                return Property::SendValueChangedSignal();
            }
            return ER_OK;
        }

        const String& getValRef() const
        {
            return m_value;
        }

        void onChanged(UInt32 handle, const String &value)
        {
            if (handle == m_handle)
            {
                std::cerr << "got notification: " << value << "\n";
                setValue(value.c_str());
            }
        }

    private:

        static const char* getVal(void *ctx)
        {
            HexProperty *pthis = static_cast<HexProperty*>(ctx);
            return pthis->m_value.c_str();
        }

    private:
        ctx_cb::FunctionCallback *m_cb;
        String m_value;
        UInt32 m_handle;
    };


    class ReadAction: public ajn::services::Action
    {
    public:
        ReadAction(const qcc::String &name, ajn::services::Widget* root,
                   HexProperty *edit,
                   bluepy::PeripheralPtr helper,
                   bluepy::CharacteristicPtr ch)
            : ajn::services::Action(name, root)
            , m_edit(edit)
            , m_helper(helper)
            , m_ch(ch)
        {}

        virtual ~ReadAction()
        {}

        bool executeCallBack()
        {
            std::cerr << "start reading \"" << m_ch->getValueHandle() << "\"\n";

            m_helper->readChar(m_ch->getValueHandle(),
                boost::bind(&ReadAction::onRead, this, _1, _2));

            return true;
        }

        void onRead(const String &status, const String &value)
        {
            std::cerr << "READ: status:'" << status << "', value:" << value << "\n";
            m_edit->setValue(value.c_str());
        }

    private:
        HexProperty *m_edit;
        bluepy::PeripheralPtr m_helper;
        bluepy::CharacteristicPtr m_ch;
    };


    class WriteAction: public ajn::services::Action
    {
    public:
        WriteAction(const qcc::String &name, ajn::services::Widget* root,
                    const String &val, bluepy::PeripheralPtr helper,
                    bluepy::CharacteristicPtr ch)
            : ajn::services::Action(name, root)
            , m_val(val)
            , m_helper(helper)
            , m_ch(ch)
        {}

        virtual ~WriteAction()
        {}

        bool executeCallBack()
        {
            if (m_val.empty())
                return false;

            std::cerr << "start writing \"" << m_val << "\" to \"" << m_ch->getValueHandle() << "\"\n";
            m_helper->writeChar(m_ch->getValueHandle(), m_val, false, boost::bind(&WriteAction::onWrite, this, _1));

            return true;
        }

        void onWrite(const String &status)
        {
            std::cerr << "WRITE: status:'" << status << "'\n";
        }

    private:
        const String &m_val;
        bluepy::PeripheralPtr m_helper;
        bluepy::CharacteristicPtr m_ch;
    };


    class WatchAction: public ajn::services::Action
    {
    public:
        WatchAction(const qcc::String &name, ajn::services::Widget* root,
                    const String &val0, const String &val1,
                    bluepy::PeripheralPtr helper,
                    bluepy::CharacteristicPtr ch)
            : ajn::services::Action(name, root)
            , m_val0(val0)
            , m_val1(val1)
            , m_current(1)
            , m_helper(helper)
            , m_ch(ch)
        {}

        virtual ~WatchAction()
        {}

        bool executeCallBack()
        {
            const String &val = m_current ? m_val1 : m_val0;
            if (val.empty())
                return false;

            std::cerr << "start writing \"" << val << "\" to \"" << m_ch->clientConfig << "\"\n";
            m_helper->writeChar(m_ch->clientConfig, val, false, boost::bind(&WatchAction::onWrite, this, _1));
            m_current = !m_current; // invert

            return true;
        }

        void onWrite(const String &status)
        {
            std::cerr << "WATCH: status:'" << status << "'\n";
        }

    private:
        const String m_val0;
        const String m_val1;
        int m_current;
        bluepy::PeripheralPtr m_helper;
        bluepy::CharacteristicPtr m_ch;
    };



    class BTDevice: public ajn::BusObject,
            public boost::enable_shared_from_this<BTDevice>
    {
    protected:
        BTDevice(const String &MAC, const String &objPath, bluepy::PeripheralPtr helper, const json::Value &meta)
            : ajn::BusObject(objPath.c_str())
            , m_ios(helper->getIoService())
            , m_controllee(0)
            , m_MAC(MAC)
            , m_meta(meta)
            , m_helper(helper)
            , m_active_req(0)
            , m_AJ_bus(0)
            , m_log("/bluetooth/device/" + MAC)
        {
            HIVELOG_TRACE(m_log, "created");
        }

    public:

        ~BTDevice()
        {
            HIVELOG_TRACE(m_log, "deleted");
        }


        static boost::shared_ptr<BTDevice> create(const String &MAC, const String &objPath,
                                                  bluepy::PeripheralPtr helper,
                                                  const json::Value &meta = json::Value())
        {
            return boost::shared_ptr<BTDevice>(new BTDevice(MAC, objPath, helper, meta));
        }

    public:

        void inspect()
        {
            HIVELOG_INFO(m_log, "inspecting...");

            m_helper->services(boost::bind(&BTDevice::on_services,
                        shared_from_this(), _1, _2));
        }

        void registerWhenInsected(ajn::BusAttachment &bus, ajn::services::ControlPanelControllee *controllee)
        {
            m_AJ_bus = &bus;
            m_controllee = controllee;
        }

    private:

        void on_services(const String &status, const std::vector<bluepy::ServicePtr> &services)
        {
            if (status.empty()) // OK
            {
                HIVELOG_INFO(m_log, "got " << services.size() << " services");

                m_services = services;
                m_helper->characteristics(boost::bind(&BTDevice::on_chars,
                            shared_from_this(), _1, _2));
            }
            else
                HIVELOG_ERROR(m_log, "failed to get services: " << status);
        }

        void on_chars(const String &status, const std::vector<bluepy::CharacteristicPtr> &chars)
        {
            if (status.empty()) // OK
            {
                HIVELOG_INFO(m_log, "got " << chars.size() << " characteristics");

                m_chars = chars;
                do_check_meta();
            }
            else
                HIVELOG_ERROR(m_log, "failed to get characteristics: " << status);
        }

        void on_desc(const String &status, const std::vector<bluepy::DescriptorPtr> &desc)
        {
            m_active_req -= 1;
            if (status.empty()) // OK
            {
                HIVELOG_DEBUG(m_log, "got " << desc.size() << " descriptors");

                for (size_t i = 0; i < desc.size(); ++i)
                {
                    bluepy::DescriptorPtr d = desc[i];

                    if (d->getUUID() == bluepy::UUID(0x2901)) // CharacteristicUserDescription
                    {
                        // get characteristic name
                        if (bluepy::CharacteristicPtr c = findNearestChar(d->getHandle()))
                        {
                            m_helper->readChar(d->getHandle(), boost::bind(&BTDevice::on_read_user_desc,
                                        shared_from_this(), _1, _2, c));
                            m_active_req += 1;
                        }
                    }
                    else if (d->getUUID() == bluepy::UUID(0x2902)) // ClientCharacteristicConfiguration
                    {
                        if (bluepy::CharacteristicPtr c = findNearestChar(d->getHandle()))
                            c->clientConfig = d->getHandle();
                    }
                }
            }
            else
                HIVELOG_ERROR(m_log, "failed to get descriptor: " << status);

            if (m_active_req == 0)
                do_build_meta();
        }


        void on_read_user_desc(const String &status, const String &value, bluepy::CharacteristicPtr ch)
        {
            m_active_req -= 1;
            if (status.empty()) // OK
            {
                ch->userDesc = hex2bytes(value);
            }
            else
                HIVELOG_ERROR(m_log, "failed to get user descriptor: " << status);

            if (m_active_req == 0)
                do_build_meta();
        }


        void do_check_meta()
        {
            for (size_t i = 0; i < m_services.size(); ++i)
            {
                bluepy::ServicePtr s = m_services[i];

                std::set<int> att_map;

                for (size_t i = 0; i < m_chars.size(); ++i)
                {
                    bluepy::CharacteristicPtr c = m_chars[i];
                    if (s->getStart() <= c->getHandle()
                     && c->getHandle() <= s->getEnd())
                    {
                        att_map.insert(c->getHandle());
                        att_map.insert(c->getValueHandle());
                    }
                }


                HIVELOG_INFO(m_log, "checking [" << s->getStart() << ", " << s->getEnd() << "] attribute range");
                size_t s_end = s->getEnd();
                if (s_end == 0xFFFF)
                {
                    int n = m_meta.get("maximumAttribute", 0).asInt();
                    if (0 < n && n < 0xFFFF)
                        s_end = n;
                    else if (!m_chars.empty())
                        s_end = m_chars.back()->getValueHandle() + 2; // TODO: fix this!!!
                }
                for (size_t i = s->getStart(); i <= s_end; ++i)
                {
                    // request information about missing attributes
                    if (att_map.find(i) == att_map.end())
                    {
                        //HIVELOG_INFO(m_log, "getting " << i << " attribute description");
                        m_helper->descriptors(boost::bind(&BTDevice::on_desc,
                                    shared_from_this(), _1, _2), i, i);
                        m_active_req += 1;
                    }
                }
            }
        }

        void do_build_meta()
        {
            String prefix = m_meta["objectPrefix"].asString();
            if (prefix.empty()) prefix = simplify(m_MAC);

            std::vector<qcc::String> interfaces;

            for (size_t i = 0; i < m_services.size(); ++i)
            {
                bluepy::ServicePtr s = m_services[i];

                String service_name = ifaceNameFromUUID(s->getUUID());
                String iface_name = "com.devicehive.gatt.device." + prefix + "." + service_name;

                ajn::InterfaceDescription *iface = 0;
                if (m_AJ_bus)
                {
                    m_AJ_bus->CreateInterface(iface_name.c_str(), iface, ajn::AJ_IFC_SECURITY_INHERIT);
                    // TODO: check error

                    interfaces.push_back(iface_name.c_str());
                }

                json::Value js;
                js["name"] = service_name;

                for (size_t i = 0; i < m_chars.size(); ++i)
                {
                    bluepy::CharacteristicPtr c = m_chars[i];
                    if (s->getStart() <= c->getHandle()
                     && c->getHandle() <= s->getEnd())
                    {
                        String char_name = charNameFromUUID(c->getUUID(), c->userDesc);
                        String char_type = charTypeFromHandle(c->getValueHandle());

                        if (iface)
                        {
                            uint8_t aj_prop = 0;
                            if (c->getProperties()&PROP_READ)
                                aj_prop |= ajn::PROP_ACCESS_READ;
                            if (c->getProperties()&(PROP_WRITE|PROP_WRITE_woR))
                                aj_prop |= ajn::PROP_ACCESS_WRITE;

                            iface->AddProperty(char_name.c_str(),
                                               AJ_type(char_type).c_str(),
                                               aj_prop);

                            //iface->AddPropertyAnnotation()
                            // TODO: add notification
                        }

                        m_prop_info[iface_name][char_name] = c;

                        json::Value jc;
                        jc["name"] = char_name;
                        if (c->clientConfig != 0)
                            jc["_config"] = c->clientConfig;
                        jc["access"] = accessFromProperties(c->getProperties());
                        jc["_value"] = c->getValueHandle();

                        js["properties"].append(jc);
                    }
                }

                if (iface) // activate interface
                {
                    AddInterface(*iface/*, ajn::BusObject::ANNOUNCED*/);
                    iface->Activate();
                }

                std::cerr << json::toStrH(js);
            }

            if (m_AJ_bus)
            {
                m_AJ_bus->RegisterBusObject(*this);
                createControlPanel();

                if (m_controllee) // restart
                {
                    using namespace ajn::services;
                    QStatus status;

                    ControlPanelService* service = ControlPanelService::getInstance();
//                    status = service->shutdownControllee();
//                    std::cerr << "shutdown: " << status << "\n";

                    status = service->initControllee(m_AJ_bus, m_controllee);
                    std::cerr << "init again: " << status << "\n";
                }
            }

            if (ajn::services::AboutServiceApi* about = ajn::services::AboutServiceApi::getInstance())
            {
                about->AddObjectDescription(GetPath(), interfaces);
                about->Announce();
            }

            m_interfaces = interfaces;
        }

public:


        /**
         * @brief Build ControlPanel controllee related to manager object.
        */
        void createControlPanel()
        {
            using namespace ajn::services;

            if (m_controllee)
            {
                QStatus status;
                std::cerr << "create CP for device: " << m_MAC << "\n";

                ControlPanelControlleeUnit *unit = new ControlPanelControlleeUnit(("Device_" + simplify(m_MAC)).c_str());
                status = m_controllee->addControlPanelUnit(unit);
                AJ_check(status, "cannot add controlpanel unit");

                for (size_t i = 0; i < m_services.size(); ++i)
                {
                    bluepy::ServicePtr s = m_services[i];

                    ControlPanel *root_cp = ControlPanel::createControlPanel(LanguageSets::get("btle_gw_lang_set"));
                    if (!root_cp) throw std::runtime_error("cannot create controlpanel");
                    status = unit->addControlPanel(root_cp);
                    AJ_check(status, "cannot add root controlpanel");

                    String root_name = "root_" + simplify(ifaceNameFromUUID(s->getUUID()));
                    std::cerr << "root name:" << root_name << "\n";
                    Container *root = new Container(root_name.c_str(), NULL);
                    status = root_cp->setRootWidget(root);
                    AJ_check(status, "cannot set root widget");
                    root->setEnabled(true);
                    root->setIsSecured(false);
                    root->setBgColor(0x200);
                    if (1)
                    {
                        std::vector<qcc::String> v;
                        v.push_back("Characteristics");
                        root->setLabels(v);
                    }
                    if (1)
                    {
                        std::vector<uint16_t> v;
                        v.push_back(VERTICAL_LINEAR);
                        v.push_back(HORIZONTAL_LINEAR);
                        root->setHints(v);
                    }

                    for (size_t i = 0; i < m_chars.size(); ++i)
                    {
                        bluepy::CharacteristicPtr c = m_chars[i];
                        if (s->getStart() <= c->getHandle()
                         && c->getHandle() <= s->getEnd())
                        {
                            //String char_type = charTypeFromHandle(c->getValueHandle());
                            controlleeForCharacteristic(root, c);


//                            if (iface)
//                            {
//                                uint8_t aj_prop = 0;
//                                if (c->getProperties()&PROP_READ)
//                                    aj_prop |= ajn::PROP_ACCESS_READ;
//                                if (c->getProperties()&(PROP_WRITE|PROP_WRITE_woR))
//                                    aj_prop |= ajn::PROP_ACCESS_WRITE;

//                                iface->AddProperty(char_name.c_str(),
//                                                   AJ_type(char_type).c_str(),
//                                                   aj_prop);

//                                //iface->AddPropertyAnnotation()
//                                // TODO: add notification
//                            }

//                            m_prop_info[iface_name][char_name] = c;

//                            json::Value jc;
//                            jc["name"] = char_name;
//                            if (c->clientConfig != 0)
//                                jc["_config"] = c->clientConfig;
//                            jc["access"] = accessFromProperties(c->getProperties());
//                            jc["_value"] = c->getValueHandle();

//                            js["properties"].append(jc);
                        }
                    }
                }
            }
        }

    private:

        void controlleeForCharacteristic(ajn::services::Container *root, bluepy::CharacteristicPtr ch)
        {
            using namespace ajn::services;

            String name = charNameFromUUID(ch->getUUID(), ch->userDesc);
            QStatus status;

            Container *line = new Container(("line_" + simplify(name)).c_str(), root);
            status = root->addChildWidget(line);
            AJ_check(status, "cannot add line");
            line->setEnabled(true);
            line->setIsSecured(false);
            if (1)
            {
                std::vector<uint16_t> v;
                //v.push_back(VERTICAL_LINEAR);
                v.push_back(HORIZONTAL_LINEAR);
                line->setHints(v);
            }

            HexProperty *hex_prop = new HexProperty(("edit_" + simplify(name)).c_str(), line, ch->getValueHandle());
            status = line->addChildWidget(hex_prop);
            AJ_check(status, "cannot add edit property");
            hex_prop->setEnabled(true);
            hex_prop->setIsSecured(false);
            if (ch->getProperties()&(PROP_WRITE|PROP_WRITE_woR))
                hex_prop->setWritable(true);
            hex_prop->setBgColor(0x500);
            if (1)
            {
                std::vector<qcc::String> v;
                v.push_back(name.c_str());
                hex_prop->setLabels(v);
            }
            if (1)
            {
                std::vector<uint16_t> v;
                v.push_back(EDITTEXT);
                hex_prop->setHints(v);
            }


            Action *RD_action = new ReadAction(("read_" + simplify(name)).c_str(),
                                               line, hex_prop, m_helper, ch);
            status = line->addChildWidget(RD_action);
            AJ_check(status, "cannot add READ action");
            RD_action->setEnabled(true);
            RD_action->setIsSecured(false);
            RD_action->setBgColor(0x400);
            if (1)
            {
                std::vector<qcc::String> v;
                v.push_back("Read");
                RD_action->setLabels(v);
            }
            if (1)
            {
                std::vector<uint16_t> v;
                v.push_back(ACTIONBUTTON);
                RD_action->setHints(v);
            }

            if (ch->getProperties()&(PROP_WRITE|PROP_WRITE_woR))
            {
                Action *WR_action = new WriteAction("WRITE_action", line,
                                    hex_prop->getValRef(), m_helper, ch);
                status = line->addChildWidget(WR_action);
                AJ_check(status, "cannot add WRITE action");
                WR_action->setEnabled(true);
                WR_action->setIsSecured(false);
                WR_action->setBgColor(0x400);
                if (1)
                {
                    std::vector<qcc::String> v;
                    v.push_back("Write");
                    WR_action->setLabels(v);
                }
                if (1)
                {
                    std::vector<uint16_t> v;
                    v.push_back(ACTIONBUTTON);
                    WR_action->setHints(v);
                }
            }

            if ((ch->getProperties()&PROP_NOTIFY) && ch->clientConfig)
            {
                m_helper->callOnNewNotification(boost::bind(&HexProperty::onChanged, hex_prop, _1, _2));

                Action *watch_action = new WatchAction("WATCH_action", line, "0000", "0100", m_helper, ch);
                status = line->addChildWidget(watch_action);
                AJ_check(status, "cannot add WATCH action");
                watch_action->setEnabled(true);
                watch_action->setIsSecured(false);
                watch_action->setBgColor(0x400);
                if (1)
                {
                    std::vector<qcc::String> v;
                    v.push_back("Watch");
                    watch_action->setLabels(v);
                }
                if (1)
                {
                    std::vector<uint16_t> v;
                    v.push_back(ACTIONBUTTON);
                    watch_action->setHints(v);
                }

//                Action *unwatch_action = new WatchAction("WATCH_action", line, "0000", m_helper, ch);
//                status = line->addChildWidget(unwatch_action);
//                AJ_check(status, "cannot add UNWATCH action");
//                unwatch_action->setEnabled(true);
//                unwatch_action->setIsSecured(false);
//                unwatch_action->setBgColor(0x400);
//                if (1)
//                {
//                    std::vector<qcc::String> v;
//                    v.push_back("Unwatch");
//                    unwatch_action->setLabels(v);
//                }
//                if (1)
//                {
//                    std::vector<uint16_t> v;
//                    v.push_back(ACTIONBUTTON);
//                    unwatch_action->setHints(v);
//                }
            }
        }

    public:
        std::vector<qcc::String> getAllInterfaces() const
        {
            return m_interfaces;
        }

    private:
        std::vector<qcc::String> m_interfaces;
        ajn::services::ControlPanelControllee *m_controllee;

        // [interface name][property name] => characteristic
        std::map<String, std::map<String, bluepy::CharacteristicPtr> > m_prop_info;

        bluepy::CharacteristicPtr findProp(const String &ifaceName, const String &propName) const
        {
            std::map<String, std::map<String, bluepy::CharacteristicPtr> >::const_iterator i = m_prop_info.find(ifaceName);
            if (i != m_prop_info.end())
            {
                std::map<String, bluepy::CharacteristicPtr>::const_iterator p = i->second.find(propName);
                if (p != i->second.end())
                    return p->second;
            }

            return bluepy::CharacteristicPtr();
        }


        String AJ_type(const String &type)
        {
            if (boost::iequals(type, "hex"))
                return "s";

            else if (boost::iequals(type, "u8"))
                return "y";
            else if (boost::iequals(type, "u16"))
                return "q";
            else if (boost::iequals(type, "i16"))
                return "n";
            else if (boost::iequals(type, "u32"))
                return "u";
            else if (boost::iequals(type, "i32"))
                return "i";
            else if (boost::iequals(type, "u64"))
                return "t";
            else if (boost::iequals(type, "i64"))
                return "x";
            else if (boost::iequals(type, "d"))
                return "d";
            else if (boost::iequals(type, "s"))
                return "s";
            else if (boost::iequals(type, "b"))
                return "b";

            else if (boost::iequals(type, "au8"))
                return "ay";
            else if (boost::iequals(type, "au16"))
                return "aq";
            else if (boost::iequals(type, "ai16"))
                return "an";
            else if (boost::iequals(type, "au32"))
                return "au";
            else if (boost::iequals(type, "ai32"))
                return "ai";
            else if (boost::iequals(type, "au64"))
                return "at";
            else if (boost::iequals(type, "ai64"))
                return "ax";
            else if (boost::iequals(type, "ad"))
                return "ad";
            else if (boost::iequals(type, "as"))
                return "as";
            else if (boost::iequals(type, "ab"))
                return "ab";

            throw std::runtime_error((type + " is unknown type").c_str());
        }

        template<typename T>
        std::vector<T> hex2arr(const String &hexVal, size_t w = sizeof(T))
        {
            std::vector<T> res;
            String b = hex2bytes(hexVal);

            std::cerr << hexVal << " to bytes: ";
            for (size_t i = 0 ; i < b.size(); ++i)
                std::cerr << (int)b[i] << " ";
            std::cerr << "\n";

            for (size_t i = 0; (i+w) <= b.size(); i += w)
                res.push_back(*reinterpret_cast<T*>(&b[i]));

            for (size_t i = 0; i < res.size(); ++i)
                std::cerr << res[i] << " ";
            std::cerr << "\n";

            return res;
        }

        ajn::MsgArg hex2aj(const String &hexVal, const String &userType)
        {
            ajn::MsgArg val;

            if (!userType.empty())
            {
                if (boost::iequals(userType, "hex"))
                {
                    val.Set("s", hexVal.c_str());
                    val.Stabilize();
                }

                else if (boost::iequals(userType, "u8"))
                    val.Set("y", strtoul(hexVal.c_str(), 0, 16));
                else if (boost::iequals(userType, "u16"))
                    val.Set("q", strtoul(hexVal.c_str(), 0, 16));
                else if (boost::iequals(userType, "i16"))
                    val.Set("n", strtoul(hexVal.c_str(), 0, 16));
                else if (boost::iequals(userType, "u32"))
                    val.Set("u", strtoul(hexVal.c_str(), 0, 16));
                else if (boost::iequals(userType, "i32"))
                    val.Set("i", strtoul(hexVal.c_str(), 0, 16));
                else if (boost::iequals(userType, "u64"))
                    val.Set("t", strtoull(hexVal.c_str(), 0, 16));
                else if (boost::iequals(userType, "i64"))
                    val.Set("x", strtoull(hexVal.c_str(), 0, 16));
//                else if (boost::iequals(userType, "d"))
//                    val.Set("d";
                else if (boost::iequals(userType, "s"))
                {
                    String d = hex2bytes(hexVal);
                    val.Set("s", d.c_str());
                    val.Stabilize();
                }
                else if (boost::iequals(userType, "b"))
                    val.Set("b", strtoul(hexVal.c_str(), 0, 16) != 0);

                else if (boost::iequals(userType, "au8"))
                {
                    std::vector<UInt8> d = hex2arr<UInt8>(hexVal);
                    val.Set("ay", d.size(), d.empty() ? 0 : &d[0]);
                    val.Stabilize();
                }
                else if (boost::iequals(userType, "au16"))
                {
                    std::vector<UInt16> d = hex2arr<UInt16>(hexVal);
                    val.Set("aq", d.size(), d.empty() ? 0 : &d[0]);
                    val.Stabilize();
                }
                else if (boost::iequals(userType, "ai16"))
                {
                    std::vector<Int16> d = hex2arr<Int16>(hexVal);
                    val.Set("an", d.size(), d.empty() ? 0 : &d[0]);
                    val.Stabilize();
                }
                else if (boost::iequals(userType, "au32"))
                {
                    std::vector<UInt32> d = hex2arr<UInt32>(hexVal);
                    val.Set("au", d.size(), d.empty() ? 0 : &d[0]);
                    val.Stabilize();
                }
                else if (boost::iequals(userType, "ai32"))
                {
                    std::vector<Int32> d = hex2arr<Int32>(hexVal);
                    val.Set("ai", d.size(), d.empty() ? 0 : &d[0]);
                    val.Stabilize();
                }
                else if (boost::iequals(userType, "au64"))
                {
                    std::vector<UInt64> d = hex2arr<UInt64>(hexVal);
                    val.Set("at", d.size(), d.empty() ? 0 : &d[0]);
                    val.Stabilize();
                }
                else if (boost::iequals(userType, "ai64"))
                {
                    std::vector<Int64> d = hex2arr<Int64>(hexVal);
                    val.Set("ax", d.size(), d.empty() ? 0 : &d[0]);
                    val.Stabilize();
                }
//                else if (boost::iequals(userType, "ad"))
//                    return "ad";
//                else if (boost::iequals(userType, "as"))
//                    return "as";
//                else if (boost::iequals(userType, "ab"))
//                    return "ab";
            }

            if (val.typeId == ajn::ALLJOYN_INVALID)
            {
                val.Set("s", hexVal.c_str());
                val.Stabilize();
            }

            HIVELOG_INFO(m_log, "hex: " << hexVal << " to AJ: "
                         << val.ToString().c_str()
                         << " (user type:" << userType << ")");
            return val;
        }

        String aj2hex(const ajn::MsgArg &val, const String &userType)
        {
            String hexVal;

            if (!userType.empty())
            {
                if (boost::iequals(userType, "hex"))
                {
                    char *s = 0;
                    val.Get("s", &s);
                    hexVal = s;
                }

                else if (boost::iequals(userType, "u8"))
                {
                    UInt8 d = 0;
                    val.Get("y", &d);
                    hexVal = dump::hex(d);
                }
                else if (boost::iequals(userType, "u16"))
                {
                    UInt16 d = 0;
                    val.Get("q", &d);
                    hexVal = dump::hex(d);
                }
                else if (boost::iequals(userType, "i16"))
                {
                    Int16 d = 0;
                    val.Get("n", &d);
                    hexVal = dump::hex(d);
                }
                else if (boost::iequals(userType, "u32"))
                {
                    UInt32 d = 0;
                    val.Get("u", &d);
                    hexVal = dump::hex(d);
                }
                else if (boost::iequals(userType, "i32"))
                {
                    Int32 d = 0;
                    val.Get("i", &d);
                    hexVal = dump::hex(d);
                }
                else if (boost::iequals(userType, "u64"))
                {
                    UInt64 d = 0;
                    val.Get("t", &d);
                    hexVal = dump::hex(d);
                }
                else if (boost::iequals(userType, "i64"))
                {
                    Int64 d = 0;
                    val.Get("x", &d);
                    hexVal = dump::hex(d);
                }
//                else if (boost::iequals(userType, "d"))
//                    val.Get("d";
                else if (boost::iequals(userType, "s"))
                {
                    char *d = 0;
                    val.Get("s", &d);
                    hexVal = dump::hex(String(d));
                }
                else if (boost::iequals(userType, "b"))
                {
                    bool d = 0;
                    val.Get("b", &d);
                    hexVal = dump::hex(UInt8(d));
                }

//                else if (boost::iequals(userType, "au8"))
//                {
//                    std::vector<UInt8> d = hex2arr<UInt8>(hexVal);
//                    val.Set("ay", d.size(), d.empty() ? 0 : &d[0]);
//                    val.Stabilize();
//                }
//                else if (boost::iequals(userType, "au16"))
//                {
//                    std::vector<UInt16> d = hex2arr<UInt16>(hexVal);
//                    val.Set("aq", d.size(), d.empty() ? 0 : &d[0]);
//                    val.Stabilize();
//                }
//                else if (boost::iequals(userType, "ai16"))
//                {
//                    std::vector<Int16> d = hex2arr<Int16>(hexVal);
//                    val.Set("an", d.size(), d.empty() ? 0 : &d[0]);
//                    val.Stabilize();
//                }
//                else if (boost::iequals(userType, "au32"))
//                {
//                    std::vector<UInt32> d = hex2arr<UInt32>(hexVal);
//                    val.Set("au", d.size(), d.empty() ? 0 : &d[0]);
//                    val.Stabilize();
//                }
//                else if (boost::iequals(userType, "ai32"))
//                {
//                    std::vector<Int32> d = hex2arr<Int32>(hexVal);
//                    val.Set("ai", d.size(), d.empty() ? 0 : &d[0]);
//                    val.Stabilize();
//                }
//                else if (boost::iequals(userType, "au64"))
//                {
//                    std::vector<UInt64> d = hex2arr<UInt64>(hexVal);
//                    val.Set("at", d.size(), d.empty() ? 0 : &d[0]);
//                    val.Stabilize();
//                }
//                else if (boost::iequals(userType, "ai64"))
//                {
//                    std::vector<Int64> d = hex2arr<Int64>(hexVal);
//                    val.Set("ax", d.size(), d.empty() ? 0 : &d[0]);
//                    val.Stabilize();
//                }
//                else if (boost::iequals(userType, "ad"))
//                    return "ad";
//                else if (boost::iequals(userType, "as"))
//                    return "as";
//                else if (boost::iequals(userType, "ab"))
//                    return "ab";

            }

            if (hexVal.empty())
            {
                char *d = 0;
                val.Get("s", &d);
                if (d) hexVal = d;
            }

            HIVELOG_INFO(m_log, "AJ: "
                         << val.ToString().c_str()
                         << " to hex: " << hexVal
                         << " (user type:" << userType << ")");
            return hexVal;
        }

    private:

        void GetProp(const ajn::InterfaceDescription::Member*, ajn::Message& msg)
        {
            const char* iface = msg->GetArg(0)->v_string.str;
            const char* property = msg->GetArg(1)->v_string.str;

            m_ios.post(boost::bind(&BTDevice::safe_GetProp, shared_from_this(),
                String(iface), String(property), ajn::Message(msg)));
        }

        void safe_GetProp(const String &ifaceName, const String &propName, ajn::Message msg)
        {
            if (bluepy::CharacteristicPtr ch = findProp(ifaceName, propName))
            {
                if (ch->getProperties()&PROP_READ)
                {
                    String userType = charTypeFromHandle(ch->getValueHandle());
                    m_helper->readChar(ch->getValueHandle(),
                        boost::bind(&BTDevice::done_GetProp,
                        shared_from_this(), _1, _2, userType, msg));
                }
                else
                    MethodReply(msg, ER_BUS_PROPERTY_ACCESS_DENIED);
            }
            else
                MethodReply(msg, ER_BUS_UNKNOWN_INTERFACE);
        }

        void done_GetProp(const String &status, const String &value, const String &userType, ajn::Message msg)
        {
            if (status.empty()) // OK
            {
                ajn::MsgArg val = hex2aj(value, userType);

                // Properties are returned as variants
                ajn::MsgArg arg = ajn::MsgArg(ajn::ALLJOYN_VARIANT);
                arg.v_variant.val = &val;
                arg.Stabilize();
                MethodReply(msg, &arg, 1);
            }
            else
                MethodReply(msg, "failed to read characteristic from BLE device", status.c_str());
        }

    private:

        void SetProp(const ajn::InterfaceDescription::Member*, ajn::Message& msg)
        {
            const char* iface = msg->GetArg(0)->v_string.str;
            const char* property = msg->GetArg(1)->v_string.str;
            const ajn::MsgArg* val = msg->GetArg(2);

            m_ios.post(boost::bind(&BTDevice::safe_SetProp, shared_from_this(),
                String(iface), String(property), val, ajn::Message(msg)));
        }

        void safe_SetProp(const String &ifaceName, const String &propName, const ajn::MsgArg *val, ajn::Message msg)
        {
            if (bluepy::CharacteristicPtr ch = findProp(ifaceName, propName))
            {
                if (ch->getProperties()&(PROP_WRITE|PROP_WRITE_woR))
                {
                    String userType = charTypeFromHandle(ch->getValueHandle());
                    String hexVal = aj2hex(*val, userType);
                    bool with_resp = false;
                    m_helper->writeChar(ch->getValueHandle(), hexVal, with_resp,
                        boost::bind(&BTDevice::done_SetProp,
                        shared_from_this(), _1, msg));
                }
                else
                    MethodReply(msg, ER_BUS_PROPERTY_ACCESS_DENIED);
            }
            else
                MethodReply(msg, ER_BUS_UNKNOWN_INTERFACE);
        }

        void done_SetProp(const String &status, ajn::Message msg)
        {
            if (status.empty()) // OK
            {
                //            // notify all session members that this property has changed
                //            const SessionId id = msg->hdrFields.field[ALLJOYN_HDR_FIELD_SESSION_ID].v_uint32;
                //            EmitPropChanged(iface->v_string.str, property->v_string.str, *(val->v_variant.val), id);

                MethodReply(msg, ER_OK);
            }
            else
                MethodReply(msg, "failed to write characteristic to BLE device", status.c_str());
        }

    public:

        static String simplify(const String &s)
        {
            // remove spaces and dots
            String res;
            res.reserve(s.size());
            for (size_t i = 0; i < s.size(); ++i)
                if (isalnum(s[i]) || s[i]=='_')
                    res.push_back(s[i]);
            return res;
        }

    private:
        String ifaceNameFromUUID(const bluepy::UUID &uuid) const
        {
            using bluepy::UUID;

            String meta_name = m_meta["interfaceNames"][uuid.toStr()].asString();
            if (!meta_name.empty()) return meta_name;

            // Service UUIDs
            if (uuid == UUID(0x1811)) return "AlertNotificationService";
            if (uuid == UUID(0x180F)) return "BatteryService";
            if (uuid == UUID(0x1810)) return "BloodPressure";
            if (uuid == UUID(0x1805)) return "CurrentTimeService";
            if (uuid == UUID(0x1818)) return "CyclingPower";
            if (uuid == UUID(0x1816)) return "CyclingSpeedAndCadence";
            if (uuid == UUID(0x180A)) return "DeviceInformation";
            if (uuid == UUID(0x1800)) return "GenericAccess";
            if (uuid == UUID(0x1801)) return "GenericAttribute";
            if (uuid == UUID(0x1808)) return "Glucose";
            if (uuid == UUID(0x1809)) return "HealthThermometer";
            if (uuid == UUID(0x180D)) return "HeartRate";
            if (uuid == UUID(0x1812)) return "HumanInterfaceDevice";
            if (uuid == UUID(0x1802)) return "ImmediateAlert";
            if (uuid == UUID(0x1803)) return "LinkLoss";
            if (uuid == UUID(0x1819)) return "LocationAndNavigation";
            if (uuid == UUID(0x1807)) return "NextDSTChangeService";
            if (uuid == UUID(0x180E)) return "PhoneAlertStatusService";
            if (uuid == UUID(0x1806)) return "ReferenceTimeUpdateService";
            if (uuid == UUID(0x1814)) return "RunningSpeedAndCadence";
            if (uuid == UUID(0x1813)) return "ScanParameters";
            if (uuid == UUID(0x1804)) return "TxPower";
            if (uuid == UUID(0x181C)) return "UserData";
            if (uuid == UUID(0xFFE0)) return "SimpleKeysService";

            return uuid.toStr();
        }

        String charNameFromUUID(const bluepy::UUID &uuid, const String &desc) const
        {
            using bluepy::UUID;

            String meta_name = m_meta["characteristicNames"][uuid.toStr()].asString();
            if (!meta_name.empty()) return meta_name;

            // Characteristic UUIDs
            if (uuid == UUID(0x2A00)) return "DeviceName";
            if (uuid == UUID(0x2A01)) return "Appearance";
            if (uuid == UUID(0x2A02)) return "PeripheralPrivacyFlag";
            if (uuid == UUID(0x2A03)) return "ReconnectionAddress";
            if (uuid == UUID(0x2A04)) return "PeripheralPreferredConnectionParameters";
            if (uuid == UUID(0x2A05)) return "ServiceChanged";
            if (uuid == UUID(0x2A07)) return "TxPowerLevel";
            if (uuid == UUID(0x2A19)) return "BatteryLevel";
            if (uuid == UUID(0x2A23)) return "SystemID";
            if (uuid == UUID(0x2A24)) return "ModelNumberString";
            if (uuid == UUID(0x2A25)) return "SerialNumberString";
            if (uuid == UUID(0x2A26)) return "FirmwareRevisionString";
            if (uuid == UUID(0x2A27)) return "HardwareRevisionString";
            if (uuid == UUID(0x2A28)) return "SoftwareRevisionString";
            if (uuid == UUID(0x2A29)) return "ManufacturerNameString";

            if (!desc.empty())
            {
                String name = simplify(desc);
                if (!name.empty())
                    return name;
            }

            return uuid.toStr();
        }

        String charTypeFromHandle(UInt32 handle) const
        {
            String meta_type = m_meta["characteristicTypes"][boost::lexical_cast<String>(handle)].asString();
            if (!meta_type.empty()) return meta_type;

            return "hex"; // hex by default
        }

        enum PropsMask
        {
            PROP_BROADCAST  = 0x01,
            PROP_READ       = 0x02,
            PROP_WRITE_woR  = 0x04,
            PROP_WRITE      = 0x08,
            PROP_NOTIFY     = 0x10,
            PROP_INDICATE   = 0x20
        };

        json::Value accessFromProperties(UInt32 props)
        {
            String res;

#define DO_TEST(MASK,RES) if (props&(MASK)) { res += RES; props&=~(MASK); }
            DO_TEST(PROP_BROADCAST, "B");
            DO_TEST(PROP_READ     , "R");
            DO_TEST(PROP_WRITE_woR, "w");
            DO_TEST(PROP_WRITE    , "W");
            DO_TEST(PROP_NOTIFY   , "N");
            DO_TEST(PROP_INDICATE , "I");
#undef DO_TEST

            if (props) // unknown flags
            {
                res += "-" + boost::lexical_cast<String>(props);
            }

            return res;
        }


        bluepy::CharacteristicPtr findNearestChar(UInt32 handle)
        {
            bluepy::CharacteristicPtr res;
            for (size_t i = 0; i < m_chars.size(); ++i)
            {
                bluepy::CharacteristicPtr ch = m_chars[i];
                if (handle < ch->getHandle())
                    break;

                res = ch;
            }

            return res;
        }

        String hex2bytes(const String &hex)
        {
            String res;
            if (hex.size()%2) throw std::runtime_error("invalid HEX string");
            res.reserve(hex.size()/2);
            for (size_t i = 0; i < hex.size(); i += 2)
            {
                int a = hive::misc::hex2int(hex[i+0]);
                int b = hive::misc::hex2int(hex[i+1]);
                if (a < 0 || b < 0) throw std::runtime_error("not a HEX string");
                res.push_back((a<<4) | b);
            }
            return res;
        }

    private:
        boost::asio::io_service &m_ios;
        String m_MAC;
        json::Value m_meta;
        bluepy::PeripheralPtr m_helper;
        std::vector<bluepy::ServicePtr> m_services;
        std::vector<bluepy::CharacteristicPtr> m_chars;
        int m_active_req;
        ajn::BusAttachment *m_AJ_bus;
        log::Logger m_log;
    };

    typedef boost::shared_ptr<BTDevice> BTDevicePtr;
    std::map<String, BTDevicePtr> m_bt_devices;


private:

    /**
     * @brief create device (alljoyn thread).
     *
     * status = createDevice(MAC, meta)
     */
    void do_createDevice(const ajn::InterfaceDescription::Member*, ajn::Message& message)
    {
        const ajn::MsgArg *args = 0;
                 size_t No_args = 0;
        message->GetArgs(No_args, args);
        if (2 == No_args)
        {
            char *MAC_str = 0;
            args[0].Get("s", &MAC_str);

            char *meta_str = 0;
            args[1].Get("s", &meta_str);

            json::Value meta;
            if (meta_str && *meta_str)
                meta = json::fromStr(meta_str);
            HIVELOG_DEBUG(m_log, "calling createDevice: MAC:\"" << MAC_str << "\" meta:\"" << meta_str << "\"");
            m_ios.post(boost::bind(&ManagerObj::safe_createDevice, shared_from_this(),
                                   String(MAC_str), meta, ajn::Message(message)));
        }
        else
            MethodReply(message, ER_INVALID_DATA);
    }

    /*
     * @brief create device (main thread).
     */
    void safe_createDevice(const String &MAC, const json::Value &meta, ajn::Message message)
    {
        unsigned int n = impl_createDevice(MAC, meta);

        ajn::MsgArg ret_args[1];
        ret_args[0].Set("u", n); // OK
        MethodReply(message, ret_args, 1);
    }

public:
    int impl_createDevice(const String &MAC, const json::Value &meta)
    {
        HIVELOG_DEBUG(m_log, "createDevice: MAC:\"" << MAC << "\" meta:\"" << toStr(meta) << "\"");

        int n = 0;
        BTDevicePtr &bt = m_bt_devices[MAC];
        if (!bt)
        {
            bluepy::PeripheralPtr helper = m_plist->findHelper(MAC);
            String objPath = meta["objectPath"].asString();
            if (objPath.empty()) objPath = "/" + BTDevice::simplify(MAC);
            bt = BTDevice::create(MAC, objPath, helper, meta);
            n += 1;

            bt->inspect();
            bt->registerWhenInsected(const_cast<ajn::BusAttachment&>(GetBusAttachment()), m_controllee);
        }

        return n;
    }


    /**
     * @brief delete device (alljoyn thread).
     *
     * status = deleteDevice(MAC)
     */
    void do_deleteDevice(const ajn::InterfaceDescription::Member*, ajn::Message& message)
    {
        const ajn::MsgArg *args = 0;
                 size_t No_args = 0;
        message->GetArgs(No_args, args);
        if (1 == No_args)
        {
            char *MAC_str = 0;
            args[0].Get("s", &MAC_str);

            HIVELOG_DEBUG(m_log, "calling deleteDevice: MAC:\"" << MAC_str << "\"");
            m_ios.post(boost::bind(&ManagerObj::safe_deleteDevice, shared_from_this(),
                                   String(MAC_str), ajn::Message(message)));
        }
        else
            MethodReply(message, ER_INVALID_DATA);
    }

    /**
     * @brief delete device (main thread).
     */
    void safe_deleteDevice(const String &MAC, ajn::Message message)
    {
        unsigned int n = impl_deleteDevice(MAC);

        ajn::MsgArg ret_args[1];
        ret_args[0].Set("u", n); // OK
        MethodReply(message, ret_args, 1);
    }

    int impl_deleteDevice(const String &MAC)
    {
        HIVELOG_DEBUG(m_log, "deleteDevice: MAC:\"" << MAC << "\"");
        int n = 0;

        BTDevicePtr bt;
        std::map<String, BTDevicePtr>::iterator it = m_bt_devices.find(MAC);
        if (it != m_bt_devices.end())
            bt = it->second;

        if (bt)
        {

            if (ajn::services::AboutServiceApi* about = ajn::services::AboutServiceApi::getInstance())
                about->RemoveObjectDescription(bt->GetPath(), bt->getAllInterfaces());
        }

        n += m_bt_devices.erase(MAC);
        return n;
    }


    /**
     * @brief get device list (alljoyn thread).
     */
    void do_getDeviceList(const ajn::InterfaceDescription::Member*, ajn::Message& message)
    {
        const ajn::MsgArg *args = 0;
                 size_t No_args = 0;
        message->GetArgs(No_args, args);
        if (0 == No_args)
        {
            HIVELOG_DEBUG(m_log, "calling getDeviceList");
            m_ios.post(boost::bind(&ManagerObj::safe_getDeviceList, shared_from_this(),
                                   ajn::Message(message)));
        }
        else
            MethodReply(message, ER_INVALID_DATA);
    }

    /**
     * @brief get device list (main thread)
     */
    void safe_getDeviceList(ajn::Message message)
    {
        HIVELOG_DEBUG(m_log, "getDeviceList");

        std::vector<const char*> c_list;
        typedef std::map<String, BTDevicePtr>::const_iterator Iterator;
        for (Iterator i = m_bt_devices.begin(); i != m_bt_devices.end(); ++i)
            c_list.push_back(i->first.c_str());

        ajn::MsgArg ret_args[1];
        ret_args[0].Set("as", c_list.size(),
            c_list.empty() ? 0 : &c_list[0]);
        ret_args[0].Stabilize();

        MethodReply(message, ret_args, 1);
    }

private:

    /**
     * @brief scanDevices (alljoyn thread).
     */
    void do_scanDevices(const ajn::InterfaceDescription::Member*, ajn::Message& message)
    {
        const ajn::MsgArg *args = 0;
                 size_t No_args = 0;
        message->GetArgs(No_args, args);
        if (1 == No_args)
        {
            unsigned int timeout_ms = 0;
            args[0].Get("u", &timeout_ms);

            HIVELOG_DEBUG(m_log, "calling scanDevices: timeout:" << timeout_ms << "ms");
            m_ios.post(boost::bind(&ManagerObj::safe_scanDevices, shared_from_this(),
                                   timeout_ms, ajn::Message(message)));
        }
        else
            MethodReply(message, ER_INVALID_DATA);
    }

    /**
     * @brief scanDevices (main thread).
     */
    void safe_scanDevices(unsigned int timeout_ms, ajn::Message message)
    {
        HIVELOG_DEBUG(m_log, "starting scanDevices");

        if (m_bt_dev)
        {
            m_bt_dev->scanStart(json::Value(), bluetooth::Device::ScanCallback());
            m_bt_dev->asyncReadSome();

            // report result later
            m_delayed->callLater(timeout_ms,
                boost::bind(&ManagerObj::done_scanDevices,
                    shared_from_this(), message));
        }
        else
            MethodReply(message, "com.devicehive.bluetooth.NoDeviceError", "No BTLE device connected");
    }


    void done_scanDevices(ajn::Message message)
    {
        HIVELOG_DEBUG(m_log, "ending scanDevices");
        std::vector<ajn::MsgArg> aj_list;

        m_bt_dev->readStop();
        m_bt_dev->scanStop();

        json::Value list = m_bt_dev->getFoundDevices();
        for (json::Value::MemberIterator i = list.membersBegin(); i != list.membersEnd(); ++i)
        {
            const String MAC = i->first;
            const String name = i->second.asString();

            ajn::MsgArg item;
            item.Set("{ss}", MAC.c_str(), name.c_str());
            item.Stabilize();

            aj_list.push_back(item);
        }

        ajn::MsgArg ret_args[1];
        ret_args[0].Set("a{ss}", aj_list.size(),
                aj_list.empty() ? NULL : &aj_list[0]);

        MethodReply(message, ret_args, 1);
    }


    /**
     * @brief getServices (alljoyn thread).
     */
    void do_getServices(const ajn::InterfaceDescription::Member*, ajn::Message& message)
    {
        const ajn::MsgArg *args = 0;
                 size_t No_args = 0;
        message->GetArgs(No_args, args);
        if (1 == No_args)
        {
            char *MAC_str = 0;
            args[0].Get("s", &MAC_str);

            HIVELOG_DEBUG(m_log, "calling getServices: MAC:\"" << MAC_str << "\"");
            m_ios.post(boost::bind(&ManagerObj::safe_getServices, shared_from_this(),
                                   String(MAC_str), ajn::Message(message)));
        }
        else
            MethodReply(message, ER_INVALID_DATA);
    }

    /**
     * @brief getServices (main thread).
     */
    void safe_getServices(const String &MAC, ajn::Message message)
    {
        bluepy::PeripheralPtr helper = m_plist->findHelper(MAC);
        helper->services(boost::bind(&ManagerObj::done_getServices,
            shared_from_this(), _1, _2, message));
    }

    void done_getServices(const String &status, const std::vector<bluepy::ServicePtr> &services, ajn::Message message)
    {
        if (status.empty())
        {
            std::vector<ajn::MsgArg> aj_list;
            for (size_t i = 0; i < services.size(); ++i)
            {
                bluepy::ServicePtr s = services[i];
                String uuid_str = s->getUUID().toStr();

                ajn::MsgArg item;
                item.Set("(suu)", uuid_str.c_str(),
                         s->getStart(), s->getEnd());
                item.Stabilize();

                aj_list.push_back(item);
            }

            ajn::MsgArg ret_args[1];
            ret_args[0].Set("a(suu)", aj_list.size(),
                    aj_list.empty() ? NULL : &aj_list[0]);

            MethodReply(message, ret_args, 1);
        }
        else
            MethodReply(message, "com.devicehive.bluetooth.StatusError", status.c_str());
    }


    /**
     * @brief getCharacteristics (alljoyn thread).
     */
    void do_getCharacteristics(const ajn::InterfaceDescription::Member*, ajn::Message& message)
    {
        const ajn::MsgArg *args = 0;
                 size_t No_args = 0;
        message->GetArgs(No_args, args);
        if (1 == No_args)
        {
            char *MAC_str = 0;
            args[0].Get("s", &MAC_str);

            HIVELOG_DEBUG(m_log, "calling getCharacteristics: MAC:\"" << MAC_str << "\"");
            m_ios.post(boost::bind(&ManagerObj::safe_getCharacteristics, shared_from_this(),
                                   String(MAC_str), ajn::Message(message)));
        }
        else
            MethodReply(message, ER_INVALID_DATA);
    }

    /**
     * @brief getCharacteristics (main thread).
     */
    void safe_getCharacteristics(const String &MAC, ajn::Message message)
    {
        bluepy::PeripheralPtr helper = m_plist->findHelper(MAC);
        helper->characteristics(boost::bind(&ManagerObj::done_getCharacteristics,
            shared_from_this(), _1, _2, message));
    }

    void done_getCharacteristics(const String &status, const std::vector<bluepy::CharacteristicPtr> &chars, ajn::Message message)
    {
        if (status.empty())
        {
            std::vector<ajn::MsgArg> aj_list;
            for (size_t i = 0; i < chars.size(); ++i)
            {
                bluepy::CharacteristicPtr ch = chars[i];
                String uuid_str = ch->getUUID().toStr();

                ajn::MsgArg item;
                item.Set("(suuu)", uuid_str.c_str(),
                         ch->getHandle(), ch->getProperties(),
                         ch->getValueHandle());
                item.Stabilize();

                aj_list.push_back(item);
            }

            ajn::MsgArg ret_args[1];
            ret_args[0].Set("a(suuu)", aj_list.size(),
                    aj_list.empty() ? NULL : &aj_list[0]);

            MethodReply(message, ret_args, 1);
        }
        else
            MethodReply(message, "com.devicehive.bluetooth.StatusError", status.c_str());
    }


    /**
     * @brief read (alljoyn thread).
     */
    void do_read(const ajn::InterfaceDescription::Member*, ajn::Message& message)
    {
        const ajn::MsgArg *args = 0;
                 size_t No_args = 0;
        message->GetArgs(No_args, args);
        if (2 == No_args)
        {
            char *MAC_str = 0;
            args[0].Get("s", &MAC_str);

            unsigned int handle = 0;
            args[1].Get("u", &handle);

            HIVELOG_DEBUG(m_log, "calling read: MAC:\"" << MAC_str << "\", handle:" << handle);
            m_ios.post(boost::bind(&ManagerObj::safe_read, shared_from_this(),
                                   String(MAC_str), handle, ajn::Message(message)));
        }
        else
            MethodReply(message, ER_INVALID_DATA);
    }

    /**
     * @brief read (main thread).
     */
    void safe_read(const String &MAC, UInt32 handle, ajn::Message message)
    {
        bluepy::PeripheralPtr helper = m_plist->findHelper(MAC);
        helper->readChar(handle, boost::bind(&ManagerObj::done_read,
            shared_from_this(), _1, _2, message));
    }

    void done_read(const String &status, const String &value, ajn::Message message)
    {
        if (status.empty())
        {
            // TODO: convert hex to bytes!

            ajn::MsgArg ret_args[1];
            ret_args[0].Set("s", value.c_str());
            MethodReply(message, ret_args, 1);
        }
        else
            MethodReply(message, "com.devicehive.bluetooth.StatusError", status.c_str());
    }


    /**
     * @brief write (alljoyn thread).
     */\
    void do_write(const ajn::InterfaceDescription::Member*, ajn::Message& message)
    {
        const ajn::MsgArg *args = 0;
                 size_t No_args = 0;
        message->GetArgs(No_args, args);
        if (4 == No_args)
        {
            char *MAC_str = 0;
            args[0].Get("s", &MAC_str);

            unsigned int handle = 0;
            args[1].Get("u", &handle);

            bool with_resp = false;
            args[2].Get("b", &with_resp);

            char *value = 0;
            args[3].Get("s", &value);

            // TODO: convert value from bytes to hex
            HIVELOG_DEBUG(m_log, "calling write: MAC:\"" << MAC_str << "\", handle:" << handle << " value:\"" << value << "\"");
            m_ios.post(boost::bind(&ManagerObj::safe_write, shared_from_this(),
                String(MAC_str), handle, with_resp, String(value), ajn::Message(message)));
        }
        else
            MethodReply(message, ER_INVALID_DATA);
    }

    /**
     * @brief write (main thread).
     */
    void safe_write(const String &MAC, UInt32 handle, bool withResp, const String &value, ajn::Message message)
    {
        bluepy::PeripheralPtr helper = m_plist->findHelper(MAC);
        helper->writeChar(handle, value, withResp,
            boost::bind(&ManagerObj::done_write,
            shared_from_this(), _1, message));
    }

    void done_write(const String &status, ajn::Message message)
    {
        if (status.empty())
        {
            ajn::MsgArg ret_args[1];
            ret_args[0].Set("u", 0); // OK
            MethodReply(message, ret_args, 1);
        }
        else
            MethodReply(message, "com.devicehive.bluetooth.StatusError", status.c_str());
    }

private:
    boost::asio::io_service &m_ios;
    bluepy::IPeripheralList *m_plist;
    basic_app::DelayedTaskList::SharedPtr m_delayed;
    bluetooth::DevicePtr m_bt_dev;
    ajn::services::ControlPanelControllee *m_controllee;
    log::Logger m_log;
};

} // alljoyn namespace


/// @brief The BTLE gateway example.
namespace btle_gw
{
    using namespace hive;

/// @brief Various contants and timeouts.
enum Timeouts
{
    STREAM_RECONNECT_TIMEOUT    = 10000, ///< @brief Try to open stream device each X milliseconds.
    SERVER_RECONNECT_TIMEOUT    = 10000, ///< @brief Try to open server connection each X milliseconds.
    RETRY_TIMEOUT               = 5000,  ///< @brief Common retry timeout, milliseconds.
    DEVICE_OFFLINE_TIMEOUT      = 5
};


/// @brief The simple gateway application.
/**
This application controls only one device connected via serial port or socket or pipe!

@see @ref page_simple_gw
*/
class Application:
    public basic_app::Application,
    public devicehive::IDeviceServiceEvents,
    public ajn::BusListener,
    public bluepy::IPeripheralList,
    public ajn::SessionPortListener
{
    typedef basic_app::Application Base; ///< @brief The base type.
    typedef Application This; ///< @brief The type alias.

public:


protected:

    /// @brief The default constructor.
    Application()
        : m_disableWebsockets(false)
        , m_disableWebsocketPingPong(false)
        , m_deviceRegistered(false)
    {}

public:

    /// @brief The shared pointer type.
    typedef boost::shared_ptr<Application> SharedPtr;


    /// @brief The factory method.
    /**
    @param[in] argc The number of command line arguments.
    @param[in] argv The command line arguments.
    @return The new application instance.
    */
    static SharedPtr create(int argc, const char* argv[])
    {
        SharedPtr pthis(new This());

        String networkName = "C++ network";
        String networkKey = "";
        String networkDesc = "C++ device test network";

        String deviceId = "3305fe00-9bc9-11e4-bd06-0800200c9a66";
        String deviceName = "btle_gw";
        String deviceKey = "7adbc600-9bca-11e4-bd06-0800200c9a66";

        String baseUrl;// = "http://ecloud.dataart.com/ecapi8";
        size_t web_timeout = 0; // zero - don't change
        String http_version;

        //pthis->m_helperPath = "/usr/bin/gatttool";
        pthis->m_helperPath = "bluepy-helper";

        String bluetoothName = "";

        // custom device properties
        for (int i = 1; i < argc; ++i) // skip executable name
        {
            if (boost::algorithm::iequals(argv[i], "--help"))
            {
                std::cout << argv[0] << " [options]";
                std::cout << "\t--helper <helper path>\n";
                std::cout << "\t--log <log file name>\n";
                std::cout << "\t--gatewayId <gateway identifier>\n";
                std::cout << "\t--gatewayName <gateway name>\n";
                std::cout << "\t--gatewayKey <gateway authentication key>\n";
                std::cout << "\t--networkName <network name>\n";
                std::cout << "\t--networkKey <network authentication key>\n";
                std::cout << "\t--networkDesc <network description>\n";
                std::cout << "\t--server <server URL>\n";
                std::cout << "\t--web-timeout <timeout, seconds>\n";
                std::cout << "\t--http-version <major.minor HTTP version>\n";
                std::cout << "\t--no-ws disable automatic websocket service switching\n";
                std::cout << "\t--no-ws-ping-pong disable websocket ping/pong messages\n";
                std::cout << "\t--bluetooth <BLE device name or address>\n";
                std::cout << "\t--sensortag <SensorTag device address>\n";

                exit(1);
            }
            else if (boost::algorithm::iequals(argv[i], "--helper") && i+1 < argc)
                pthis->m_helperPath = argv[++i];
            else if (boost::algorithm::iequals(argv[i], "--gatewayId") && i+1 < argc)
                deviceId = argv[++i];
            else if (boost::algorithm::iequals(argv[i], "--gatewayName") && i+1 < argc)
                deviceName = argv[++i];
            else if (boost::algorithm::iequals(argv[i], "--gatewayKey") && i+1 < argc)
                deviceKey = argv[++i];
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
            else if (boost::algorithm::iequals(argv[i], "--bluetooth") && i+1 < argc)
                bluetoothName = argv[++i];
            else if (boost::algorithm::iequals(argv[i], "--sensortag") && i+1 < argc)
                pthis->m_sensorTag = argv[++i];
        }

        if (pthis->m_helperPath.empty())
            throw std::runtime_error("no helper provided");

        pthis->m_bluetooth = bluetooth::Device::create(pthis->m_ios, bluetoothName);
        pthis->m_network = devicehive::Network::create(networkName, networkKey, networkDesc);
        pthis->m_device = devicehive::Device::create(deviceId, deviceName, deviceKey,
            devicehive::Device::Class::create("BTLE gateway", "0.1", false, DEVICE_OFFLINE_TIMEOUT),
                                                     pthis->m_network);
        pthis->m_device->status = "Online";

        if (!baseUrl.empty()) // create service
        {
            http::Url url(baseUrl);

            if (boost::iequals(url.getProtocol(), "ws")
                || boost::iequals(url.getProtocol(), "wss"))
            {
                if (pthis->m_disableWebsockets)
                    throw std::runtime_error("websockets are disabled by --no-ws switch");

                HIVELOG_INFO(pthis->m_log, "WebSocket service is used: " << baseUrl);
                devicehive::WebsocketService::SharedPtr service = devicehive::WebsocketService::create(
                    http::Client::create(pthis->m_ios), baseUrl, pthis);
                service->setPingPongEnabled(!pthis->m_disableWebsocketPingPong);
                if (0 < web_timeout)
                    service->setTimeout(web_timeout*1000); // seconds -> milliseconds

                pthis->m_service = service;
            }
            else
            {
                HIVELOG_INFO(pthis->m_log, "RESTful service is used: " << baseUrl);
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

protected:

    /// @brief Start the application.
    /**
    Tries to open stream device.
    */
    virtual void start()
    {
        HIVELOG_TRACE_BLOCK(m_log, "start()");

        Base::start();
        if (m_service)
            m_service->asyncConnect();
        m_delayed->callLater( // ASAP
            boost::bind(&This::tryToOpenBluetoothDevice,
                shared_from_this()));

        AJ_init();
    }


    /// @brief Stop the application.
    /**
    Stops the "open" timer.
    */
    virtual void stop()
    {
        HIVELOG_TRACE_BLOCK(m_log, "stop()");

        if (m_service)
            m_service->cancelAll();
        if (m_bluetooth)
            m_bluetooth->close();

        for (std::map<String, bluepy::PeripheralPtr>::iterator i = m_helpers.begin(); i != m_helpers.end(); ++i)
            i->second->stop();

        Base::stop();
    }

private:

    /// @brief Try to open bluetooth device.
    /**
    */
    void tryToOpenBluetoothDevice()
    {
        if (m_bluetooth)
        {
            m_bluetooth->async_open(boost::bind(&This::onBluetoothDeviceOpen,
                shared_from_this(), _1));
        }
    }

    void onBluetoothDeviceOpen(boost::system::error_code err)
    {
        if (!err)
        {
            HIVELOG_INFO(m_log, "got bluetooth device OPEN: #"
                      << m_bluetooth->getDeviceId() << " "
                      << m_bluetooth->getDeviceAddressStr());
        }
        else
        {
            HIVELOG_DEBUG(m_log, "cannot open bluetooth device: ["
                << err << "] " << err.message());

            m_delayed->callLater(STREAM_RECONNECT_TIMEOUT,
                boost::bind(&This::tryToOpenBluetoothDevice,
                    shared_from_this()));
        }
    }



    /// @brief Reset the bluetooth device.
    /**
    @brief tryToReopen if `true` then try to reopen stream device as soon as possible.
    */
    virtual void resetBluetoothDevice(bool tryToReopen)
    {
        HIVELOG_WARN(m_log, "bluetooth device RESET");
        if (m_bluetooth)
            m_bluetooth->close();

        if (tryToReopen && !terminated())
        {
            m_delayed->callLater( // ASAP
                boost::bind(&This::tryToOpenBluetoothDevice,
                    shared_from_this()));
        }
    }

private: // IDeviceServiceEvents

    /// @copydoc deviceIDeviceServiceEvents::onConnected()
    virtual void onConnected(ErrorCode err)
    {
        HIVELOG_TRACE_BLOCK(m_log, "onConnected()");

        if (!err)
        {
            HIVELOG_DEBUG_STR(m_log, "connected to the server");
            m_service->asyncGetServerInfo();
        }
        else
            handleServiceError(err, "connection");
    }


    /// @copydoc deviceIDeviceServiceEvents::onServerInfo()
    virtual void onServerInfo(boost::system::error_code err, devicehive::ServerInfo info)
    {
        if (!err)
        {
            // don't update last command timestamp on reconnect
            if (m_lastCommandTimestamp.empty())
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

            if (m_device)
                m_service->asyncRegisterDevice(m_device);
        }
        else
            handleServiceError(err, "getting server info");
    }


    /// @copydoc deviceIDeviceServiceEvents::onRegisterDevice()
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


    /// @copydoc deviceIDeviceServiceEvents::onInsertCommand()
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
                processed = handleGatewayCommand(command);
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

    /// @brief Handle gateway command.
    bool handleGatewayCommand(devicehive::CommandPtr command)
    {
        command->status = "Success";

        if (boost::iequals(command->name, "Hello"))
            command->result = "Good to see you!";
        else if (boost::iequals(command->name, "devices"))
            command->result = bluetooth::Device::getDevicesInfo();
        else if (boost::iequals(command->name, "info"))
        {
            if (!m_bluetooth || !m_bluetooth->is_open())
                throw std::runtime_error("No device");
            command->result = m_bluetooth->getDeviceInfo();
        }
        else if (boost::iequals(command->name, "exec/hciconfig"))
        {
            String cmd = "hciconfig ";
            cmd += command->params.asString();
            //command->result = shellExec(cmd);

            asyncShellExec(m_ios, cmd, boost::bind(&This::onAsyncExecSendRawResult,
                shared_from_this(), _1, _2, _3, command));
            return false; // pended
        }
        else if (boost::iequals(command->name, "exec/hcitool"))
        {
            String cmd = "hcitool ";
            cmd += command->params.asString();
            //command->result = shellExec(cmd);

            asyncShellExec(m_ios, cmd, boost::bind(&This::onAsyncExecSendRawResult,
                shared_from_this(), _1, _2, _3, command));
            return false; // pended
        }
        else if (boost::iequals(command->name, "exec/gatttool"))
        {
            String cmd = "gatttool ";
            cmd += command->params.asString();
            //command->result = shellExec(cmd);

            asyncShellExec(m_ios, cmd, boost::bind(&This::onAsyncExecSendRawResult,
                shared_from_this(), _1, _2, _3, command));
            return false; // pended
        }
        else if (boost::iequals(command->name, "scan/start")
              || boost::iequals(command->name, "scanStart")
              || boost::iequals(command->name, "startScan")
              || boost::iequals(command->name, "scan"))
        {
            if (!m_bluetooth || !m_bluetooth->is_open())
                throw std::runtime_error("No device");

            // force to update current pending scan command if any
            if (m_pendingScanCmdTimeout)
                m_pendingScanCmdTimeout->cancel();
            onScanCommandTimeout();

            m_bluetooth->scanStart(command->params,
                    boost::bind(&This::onScanFound, shared_from_this(), _1, _2)); // send notification on new device found
            m_bluetooth->asyncReadSome();

            const int def_timeout = boost::iequals(command->name, "scan") ? 20 : 0;
            const int timeout = command->params.get("timeout", def_timeout).asUInt8(); // limited range [0..255]
            if (timeout != 0)
            {
                m_pendingScanCmdTimeout = m_delayed->callLater(timeout*1000,
                    boost::bind(&This::onScanCommandTimeout, shared_from_this()));
            }

            m_scanReportedDevices.clear();
            m_pendingScanCmd = command;
            return false; // pended
        }
        else if (boost::iequals(command->name, "scan/stop")
              || boost::iequals(command->name, "scanStop")
              || boost::iequals(command->name, "stopScan"))
        {
            if (!m_bluetooth || !m_bluetooth->is_open())
                throw std::runtime_error("No device");
            m_bluetooth->readStop();
            m_bluetooth->scanStop();

            // force to update current pending scan command if any
            if (m_pendingScanCmdTimeout)
                m_pendingScanCmdTimeout->cancel();
            onScanCommandTimeout();
        }
        else if (boost::iequals(command->name, "xgatt/status"))
        {
            String device = command->params.get("device", json::Value::null()).asString();
            bluepy::PeripheralPtr helper = findHelper(device);
            if (!helper) throw std::runtime_error("cannot create helper");

            helper->status(boost::bind(&This::onHelperStatus, shared_from_this(), _1, command, helper));
            m_pendedCommands[helper].push_back(command);
            return false; // pended
        }
        else if (boost::iequals(command->name, "xgatt/connect"))
        {
            String device = command->params.get("device", json::Value::null()).asString();
            bluepy::PeripheralPtr helper = findHelper(device);
            if (!helper) throw std::runtime_error("cannot create helper");

            helper->connect(boost::bind(&This::onHelperConnect, shared_from_this(), _1, command, helper));
            m_pendedCommands[helper].push_back(command);
            return false; // pended
        }
        else if (boost::iequals(command->name, "xgatt/disconnect"))
        {
            String device = command->params.get("device", json::Value::null()).asString();
            bluepy::PeripheralPtr helper = findHelper(device);
            if (!helper) throw std::runtime_error("cannot create helper");

            helper->disconnect(boost::bind(&This::onHelperConnect, shared_from_this(), _1, command, helper));
            m_pendedCommands[helper].push_back(command);
            return false; // pended
        }
        else if (boost::iequals(command->name, "xgatt/primary")
              || boost::iequals(command->name, "xgatt/services"))
        {
            String device = command->params.get("device", json::Value::null()).asString();
            bluepy::PeripheralPtr helper = findHelper(device);
            if (!helper) throw std::runtime_error("cannot create helper");

            helper->services(boost::bind(&This::onHelperServices, shared_from_this(), _1, _2, command, helper));
            m_pendedCommands[helper].push_back(command);
            return false; // pended
        }
        else if (boost::iequals(command->name, "xgatt/characteristics")
              || boost::iequals(command->name, "xgatt/chars"))
        {
            String device = command->params.get("device", json::Value::null()).asString();
            bluepy::PeripheralPtr helper = findHelper(device);
            if (!helper) throw std::runtime_error("cannot create helper");

            // TODO: start/end/uuid arguments

            helper->characteristics(boost::bind(&This::onHelperCharacteristics, shared_from_this(), _1, _2, command, helper));
            m_pendedCommands[helper].push_back(command);
            return false; // pended
        }
        else if (boost::iequals(command->name, "xgatt/charRead")
              || boost::iequals(command->name, "xgatt/readChar")
              || boost::iequals(command->name, "xgatt/read"))
        {
            String device = command->params.get("device", json::Value::null()).asString();
            bluepy::PeripheralPtr helper = findHelper(device);
            if (!helper) throw std::runtime_error("cannot create helper");

            UInt32 handle = command->params.get("handle", json::Value::null()).asUInt32();

            helper->readChar(handle, boost::bind(&This::onHelperCharRead, shared_from_this(), _1, _2, command, helper));
            m_pendedCommands[helper].push_back(command);
            return false; // pended
        }
        else if (boost::iequals(command->name, "xgatt/charWrite")
              || boost::iequals(command->name, "xgatt/writeChar")
              || boost::iequals(command->name, "xgatt/write"))
        {
            String device = command->params.get("device", json::Value::null()).asString();
            bluepy::PeripheralPtr helper = findHelper(device);
            if (!helper) throw std::runtime_error("cannot create helper");

            UInt32 handle = command->params.get("handle", json::Value::null()).asUInt32();
            String value = command->params.get("value", json::Value::null()).asString();
            bool withResp = command->params.get("withResponse", false).asBool();

            helper->writeChar(handle, value, withResp, boost::bind(&This::onHelperCharWrite, shared_from_this(), _1, command, helper));
            m_pendedCommands[helper].push_back(command);
            return false; // pended
        }
        else if (boost::iequals(command->name, "gatt/primary")
              || boost::iequals(command->name, "gatt/services"))
        {
            String cmd = "gatttool --primary ";
            if (command->params.isObject())
            {
                cmd += gattParseAppOpts(command->params);
                cmd += gattParseMainOpts(command->params);
            }
            else
                cmd += command->params.asString();
            command->result = gattParsePrimary(shellExec(cmd));
        }
        else if (boost::iequals(command->name, "gatt/characteristics")
              || boost::iequals(command->name, "gatt/chars"))
        {
            String cmd = "gatttool --characteristics ";
            if (command->params.isObject())
            {
                cmd += gattParseAppOpts(command->params);
                cmd += gattParseMainOpts(command->params);
            }
            else
                cmd += command->params.asString();
            command->result = gattParseCharacteristics(shellExec(cmd));
        }
        else if (boost::iequals(command->name, "gatt/charRead")
              || boost::iequals(command->name, "gatt/readChar")
              || boost::iequals(command->name, "gatt/read"))
        {
            String cmd = "gatttool --char-read ";
            if (command->params.isObject())
            {
                cmd += gattParseAppOpts(command->params);
                cmd += gattParseMainOpts(command->params);
                cmd += gattParseCharOpts(command->params);
            }
            else
                cmd += command->params.asString();
            command->result = gattParseCharRead(shellExec(cmd));
        }
        else if (boost::iequals(command->name, "gatt/charWrite")
              || boost::iequals(command->name, "gatt/writeChar")
              || boost::iequals(command->name, "gatt/write"))
        {
            String cmd = command->params.get("request", true).asBool()
                       ? "gatttool --char-write-req "
                       : "gatttool --char-write ";
            if (command->params.isObject())
            {
                cmd += gattParseAppOpts(command->params);
                cmd += gattParseMainOpts(command->params);
                cmd += gattParseCharOpts(command->params);
            }
            else
                cmd += command->params.asString();
            command->result = gattParseCharWrite(shellExec(cmd));
        }
        else
            throw std::runtime_error("Unknown command");

        return true; // processed
    }

private:

    /** @brief Parse main gatttool application options
     *
    -i, --adapter=hciX                        Specify local adapter interface
    -b, --device=MAC                          Specify remote Bluetooth address
    -t, --addr-type=[public | random]         Set LE address type. Default: public
    -m, --mtu=MTU                             Specify the MTU size
    -p, --psm=PSM                             Specify the PSM for GATT/ATT over BR/EDR
    -l, --sec-level=[low | medium | high]     Set security level. Default: low
    */
    String gattParseAppOpts(const json::Value &opts)
    {
        String res;

        // adapter
        json::Value const& i = opts["adapter"];
        if (!i.isNull())
        {
            if (i.isConvertibleToString())
            {
                res += " --adapter=";
                res += i.asString();
            }
            else
                throw std::runtime_error("Invalid adapter option");
        }

        // device
        json::Value const& b = opts["device"];
        if (!b.isNull())
        {
            if (b.isConvertibleToString())
            {
                res += " --device=";
                res += b.asString();
            }
            else
                throw std::runtime_error("Invalid device option");
        }

        // address type
        json::Value const& t = opts["addressType"];
        if (!t.isNull())
        {
            if (t.isConvertibleToString())
            {
                res += " --addr-type=";
                res += t.asString();
            }
            else
                throw std::runtime_error("Invalid address type option");
        }

        // MTU
        json::Value const& m = opts["MTU"];
        if (!m.isNull())
        {
            if (m.isConvertibleToInteger())
            {
                res += " --mtu=";
                res += boost::lexical_cast<String>(m.asInteger());
            }
            else
                throw std::runtime_error("Invalid MTU option");
        }

        // PSM
        json::Value const& p = opts["PSM"];
        if (!p.isNull())
        {
            if (p.isConvertibleToInteger())
            {
                res += " --psm=";
                res += boost::lexical_cast<String>(p.asInteger());
            }
            else
                throw std::runtime_error("Invalid PSM option");
        }

        // security level
        json::Value const& s = opts["securityLevel"];
        if (!s.isNull())
        {
            if (s.isConvertibleToString())
            {
                res += " --sec-level=";
                res += s.asString();
            }
            else
                throw std::runtime_error("Invalid security level option");
        }

        return res;
    }


    /** @brief Parse primary service/characteristics options
     *
      -s, --start=0x0001                        Starting handle(optional)
      -e, --end=0xffff                          Ending handle(optional)
      -u, --uuid=0x1801                         UUID16 or UUID128(optional)
    */
    String gattParseMainOpts(const json::Value &opts)
    {
        String res;

        // starting handle
        json::Value const& s = opts["startingHandle"];
        if (!s.isNull())
        {
            if (s.isConvertibleToString())
            {
                const String arg = s.asString();

                res += " --start=";
                if (!boost::starts_with(arg, "0x"))
                    res += "0x";
                res += arg;
            }
            else
                throw std::runtime_error("Invalid starting handle option");
        }

        // ending handle
        json::Value const& e = opts["endingHandle"];
        if (!e.isNull())
        {
            if (e.isConvertibleToString())
            {
                const String arg = e.asString();

                res += " --end=";
                if (!boost::starts_with(arg, "0x"))
                    res += "0x";
                res += arg;
            }
            else
                throw std::runtime_error("Invalid ending handle option");
        }

        // uuid
        json::Value const& u = opts["UUID"];
        if (!u.isNull())
        {
            if (u.isConvertibleToString())
            {
                const String arg = u.asString();

                res += " --uuid=";
//                if (!boost::starts_with(arg, "0x"))
//                    res += "0x";
                res += u.asString();
            }
            else
                throw std::runtime_error("Invalid UUID option");
        }

        return res;
    }


    /** @brief Prase characteristics read/write options
     *
      -a, --handle=0x0001                       Read/Write characteristic by handle(required)
      -n, --value=0x0001                        Write characteristic value (required for write operation)
      -o, --offset=N                            Offset to long read characteristic by handle
    */
    String gattParseCharOpts(const json::Value &opts)
    {
        String res;

        // handle
        json::Value const& a = opts["handle"];
        if (!a.isNull())
        {
            if (a.isConvertibleToString())
            {
                const String arg = a.asString();

                res += " --handle=";
                if (!boost::starts_with(arg, "0x"))
                    res += "0x";
                res += arg;
            }
            else
                throw std::runtime_error("Invalid handle option");
        }

        // value
        json::Value const& n = opts["value"];
        if (!n.isNull())
        {
            if (n.isConvertibleToString())
            {
                const String arg = n.asString();

                res += " --value=";
//                if (!boost::starts_with(arg, "0x"))
//                    res += "0x";
                res += arg;
            }
            else
                throw std::runtime_error("Invalid value option");
        }

        // offset
        json::Value const& o = opts["offset"];
        if (!o.isNull())
        {
            if (o.isConvertibleToInteger())
            {
                res += " --offset=";
                res += boost::lexical_cast<String>(o.asUInt());
            }
            else
                throw std::runtime_error("Invalid offset option");
        }

        return res;
    }


    /**
     * @brief parse gatttool --primary output.
     */
    json::Value gattParsePrimary(const String &output)
    {
        json::Value res(json::Value::TYPE_ARRAY);

        IStringStream s(output);
        while (!s.eof())
        {
            String line;
            std::getline(s, line);
            if (!line.empty())
            {
                int start = 0, end = 0;
                char uuid[64];
                uuid[0] = 0;

                // "attr handle = 0x000c, end grp handle = 0x000f uuid: 00001801-0000-1000-8000-00805f9b34fb"
                if (sscanf(line.c_str(), "attr handle = 0x%4X, end grp handle = 0x%4X uuid: %63s", &start, &end, uuid) == 3)
                {
                    json::Value item;
                    item["startingHandle"] = dump::hex(UInt16(start));
                    item["endingHandle"] = dump::hex(UInt16(end));
                    item["UUID"] = boost::trim_copy(String(uuid));

                    res.append(item);
                }
                else
                    throw std::runtime_error("Unexpected response");
            }
        }

        return res;
    }


    /**
     * @brief parse gatttool --characteristics output.
     */
    json::Value gattParseCharacteristics(const String &output)
    {
        json::Value res(json::Value::TYPE_ARRAY);

        IStringStream s(output);
        while (!s.eof())
        {
            String line;
            std::getline(s, line);
            if (!line.empty())
            {
                int handle = 0, properties = 0, value_handle = 0;
                char uuid[64];
                uuid[0] = 0;

                // "handle = 0x0002, char properties = 0x02, char value handle = 0x0003, uuid = 00002a00-0000-1000-8000-00805f9b34fb"
                if (sscanf(line.c_str(), "handle = 0x%4X, char properties = 0x%2X, char value handle = 0x%4X, uuid = %63s", &handle, &properties, &value_handle, uuid) == 4)
                {
                    json::Value item;
                    item["handle"] = dump::hex(UInt16(handle));
                    item["properties"] = dump::hex(UInt16(properties));
                    item["valueHandle"] = dump::hex(UInt16(value_handle));
                    item["UUID"] = boost::trim_copy(String(uuid));

                    res.append(item);
                }
                else
                    throw std::runtime_error("Unexpected response");
            }
        }

        return res;
    }


    /**
     * @brief parse gatttool --char-read output.
     */
    json::Value gattParseCharRead(const String &output)
    {
        //Characteristic value/descriptor: 00 18
        const String signature = "Characteristic value/descriptor:";
        if (!boost::starts_with(output, signature))
            throw std::runtime_error("Unexpected response");

        IStringStream s(output);
        s.ignore(signature.size());

        String result;
        s >> std::hex;
        while (!s.eof())
        {
            int h = 0;
            if (s >> h)
            {
                //std::cerr << (int)h << "\n";
                result += dump::hex(UInt8(h));
            }
            else
                break;
        }

        json::Value res;
        res["hex"] = result;
        return res;
    }


    /**
     * @brief parse gatttool --char-write output.
     */
    json::Value gattParseCharWrite(const String &output)
    {
        if (!boost::iequals(output, "Characteristic value was written successfully"))
            throw std::runtime_error("Unexpected response");

        return json::Value::null();
    }


    void onPrintExecResult(boost::system::error_code err, int result, const String &output)
    {
        std::cerr << err << ", result:" << result << ", output:" << output << "\n";
    }


    void onAsyncExecSendRawResult(boost::system::error_code err, int result, const String &output, devicehive::CommandPtr command)
    {
        HIVELOG_DEBUG(m_log, "async_result: " << err << ", result:" << result << ", output:" << output);

        if (err)
        {
            command->status = "Failed";
            command->result = String(err.message());
        }
        else if (result != 0)
        {
            command->status = "Failed";
            command->result = result;
        }
        else
        {
            command->status = "Success";
            command->result = boost::trim_copy(output);
        }

        if (m_service && m_device)
            m_service->asyncUpdateCommand(m_device, command);
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


    /// @brief New device found.
    void onScanFound(const String &MAC, const String &name)
    {
        HIVELOG_INFO(m_log, "found " << MAC << " - " << name);
        bool reported = m_scanReportedDevices.find(MAC) != m_scanReportedDevices.end();
        if (m_device && m_service && !reported)
        {
            json::Value params;
            params[MAC] = name;

            m_service->asyncInsertNotification(m_device,
                devicehive::Notification::create("xgatt/scan", params));
            m_scanReportedDevices.insert(MAC); // mark as reported
        }
    }


    /// @brief Send current scan results
    void onScanCommandTimeout()
    {
        if (m_service && m_bluetooth && m_pendingScanCmd)
        {
            m_pendingScanCmd->result = m_bluetooth->getFoundDevices();
            m_service->asyncUpdateCommand(m_device, m_pendingScanCmd);
            m_pendingScanCmd.reset();

            try
            {
                m_bluetooth->scanStop();
                m_bluetooth->readStop();
            }
            catch (const std::exception &ex)
            {
                HIVELOG_WARN(m_log, "ERROR stopping scan: " << ex.what());
            }
        }
    }

private:

    /// @brief Handle the service error.
    /**
    @param[in] err The error code.
    @param[in] hint The custom hint.
    */
    void handleServiceError(boost::system::error_code err, const char *hint)
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


    /// @brief Execute OS command.
    String shellExec(const String &cmd)
    {
        HIVELOG_DEBUG(m_log, "SHELL executing: " << cmd);
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) throw std::runtime_error("unable to execute command");

        String result;
        while (!feof(pipe))
        {
            char buf[256];
            if (fgets(buf, sizeof(buf), pipe) != NULL)
                result += buf;
        }
        int ret = pclose(pipe);
        boost::trim(result);

        HIVELOG_DEBUG(m_log, "SHELL status: " << ret
                      << ", result: " << result);

        if (ret != 0) throw std::runtime_error("failed to execute command");
        return result;
    }


    /**
     * @brief The AsyncExec context.
     */
    class AsyncExec
    {
    public:
        typedef boost::function3<void, boost::system::error_code, int, String> Callback;

    public:
        AsyncExec(boost::asio::io_service &ios, FILE *pipe, Callback cb)
            : m_stream(ios, fileno(pipe))
            , m_pipe(pipe)
            , m_callback(cb)
        {}

        ~AsyncExec()
        {
        }

    public:

        template<typename Callback>
        void readAll(Callback cb)
        {
            boost::asio::async_read(m_stream, m_buffer,
                boost::asio::transfer_all(), cb);
        }

        String getResult(int *res)
        {
            if (m_pipe)
            {
                int r = pclose(m_pipe);
                if (res) *res = r;
            }

            return bufAsStr(m_buffer.data());
        }

        void done(boost::system::error_code err)
        {
            if (err == boost::asio::error::eof) // reset EOF error
                err = boost::system::error_code();

            int result = 0;
            String output = getResult(&result);

            if (Callback cb = m_callback)
            {
                m_callback = Callback();
                cb(err, result, output);
            }
        }

    public:
        template<typename Buf>
        static String bufAsStr(const Buf &buf)
        {
            return String(boost::asio::buffers_begin(buf),
                          boost::asio::buffers_end(buf));
        }

    private:
        boost::asio::posix::stream_descriptor m_stream;
        boost::asio::streambuf m_buffer;
        FILE *m_pipe;
        Callback m_callback;
    };

    typedef boost::shared_ptr<AsyncExec> AsyncExecPtr;

    /// @brief Execute OS command asynchronously.
    AsyncExecPtr asyncShellExec(boost::asio::io_service &ios, const String &cmd, AsyncExec::Callback cb)
    {
        HIVELOG_DEBUG(m_log, "async SHELL executing: " << cmd);
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) throw std::runtime_error("unable to execute command");

        AsyncExecPtr async(new AsyncExec(ios, pipe, cb));
        async->readAll(boost::bind(&AsyncExec::done, async,
                       boost::asio::placeholders::error));

        return async;
    }

private:

    std::map<String, bluepy::PeripheralPtr> m_helpers;
    std::map<bluepy::PeripheralPtr, std::list<devicehive::CommandPtr> > m_pendedCommands;
    bluepy::PeripheralPtr findHelper(const String &device)
    {
        if (device.empty()) throw std::runtime_error("no device address provided");

        std::map<String, bluepy::PeripheralPtr>::const_iterator i = m_helpers.find(device);
        if (i != m_helpers.end())
        {
            // if (i->second->isValid())
                return i->second;
        }

        bluepy::PeripheralPtr helper = bluepy::Peripheral::create(m_ios, m_helperPath, device);
        helper->callWhenTerminated(boost::bind(&This::onHelperTerminated, shared_from_this(), _1, helper));
        helper->callOnNewNotification(boost::bind(&This::onHelperNotification, shared_from_this(), _1, _2, helper));
        helper->callOnUnintendedDisconnect(boost::bind(&This::onHelperDisconnected, shared_from_this(), _1, helper));
        helper->callOnUnhandledError(boost::bind(&This::onHelperError, shared_from_this(), _1, helper));
        helper->setIdleTimeout(60*1000);
        m_helpers[device] = helper;
        return helper;
    }


    /**
     * @brief Helper terminated.
     */
    void onHelperTerminated(boost::system::error_code err, bluepy::PeripheralPtr helper)
    {
        HIVE_UNUSED(err);

        const String &device = helper->getAddress();
        HIVELOG_INFO(m_log, device << " stopped and removed");

        // unbind callbacks to release shared pointers...
        helper->callWhenTerminated(bluepy::Peripheral::TerminatedCallback());
        helper->callOnNewNotification(bluepy::Peripheral::NotificationCallback());
        helper->callOnUnintendedDisconnect(bluepy::Peripheral::DisconnectCallback());
        helper->callOnUnhandledError(bluepy::Peripheral::ErrorCallback());
        helper->setIdleTimeout(0);

        // release helpers
        m_helpers.erase(device);
        if (m_device && m_service && !terminated())
        {
            // fail all pending commands
            std::list<devicehive::CommandPtr> &cmd_list = m_pendedCommands[helper];
            for (std::list<devicehive::CommandPtr>::iterator i = cmd_list.begin(); i != cmd_list.end(); ++i)
            {
                (*i)->status = "Failed";
                //(*i)->result = json::Value::null();
                m_service->asyncUpdateCommand(m_device, *i);
            }
        }
        m_pendedCommands.erase(helper);
    }


    /**
     * @brief Send notification.
     */
    void onHelperNotification(UInt32 handle, const String &value, bluepy::PeripheralPtr helper)
    {
        if (m_device && m_service && m_deviceRegistered)
        {
            json::Value params;
            params["device"] = helper->getAddress();
            params["handle"] = handle;
            params["valueHex"] = value;

            m_service->asyncInsertNotification(m_device,
                devicehive::Notification::create("xgatt/value", params));
        }
    }


    /**
     * @brief Stop helper on disconnection.
     */
    void onHelperDisconnected(const String &status, bluepy::PeripheralPtr helper)
    {
        HIVE_UNUSED(status);

        if (m_device && m_service && m_deviceRegistered)
        {
            json::Value params;
            params["device"] = helper->getAddress();

            m_service->asyncInsertNotification(m_device,
                devicehive::Notification::create("xgatt/diconnected", params));
        }

        HIVELOG_WARN(m_log, "BTLE device is diconnected, stopping...");
        helper->stop();
    }


    /**
     * @brief Stop helper on error.
     */
    void onHelperError(const String &status, bluepy::PeripheralPtr helper)
    {
        if (m_device && m_service && m_deviceRegistered)
        {
            json::Value params;
            params["device"] = helper->getAddress();
            params["error"] = status;

            m_service->asyncInsertNotification(m_device,
                devicehive::Notification::create("xgatt/error", params));
        }

        HIVELOG_WARN(m_log, "BTLE device error: \"" << status << "\", stopping...");
        helper->stop();
    }


    /**
     * @brief Update 'status' command.
     */
    void onHelperStatus(const String &state, devicehive::CommandPtr cmd, bluepy::PeripheralPtr helper)
    {
        if (state.empty())
        {
            cmd->status = "Failed";
        }
        else if (boost::iequals(state, "conn"))
        {
            cmd->status = "Success";
            cmd->result = String("Connected");
        }
        else if (boost::iequals(state, "disc"))
        {
            cmd->status = "Success";
            cmd->result = String("Disconnected");
        }
        else
        {
            cmd->status = "Success";
            cmd->result = state;
        }

        m_pendedCommands[helper].remove(cmd);
        if (m_device && m_service)
            m_service->asyncUpdateCommand(m_device, cmd);
    }


    /**
     * @brief Update 'connect' command.
     */
    void onHelperConnect(bool connected, devicehive::CommandPtr cmd, bluepy::PeripheralPtr helper)
    {
        if (connected)
        {
            cmd->status = "Success";
            cmd->result = String("Connected");
        }
        else
        {
            cmd->status = "Success";
            cmd->result = String("Disconnected");
        }

        m_pendedCommands[helper].remove(cmd);
        if (m_device && m_service)
            m_service->asyncUpdateCommand(m_device, cmd);
    }


    /**
     * @brief Update 'primary' command.
     */
    void onHelperServices(const String &status, const std::vector<bluepy::ServicePtr> &services,
                          devicehive::CommandPtr cmd, bluepy::PeripheralPtr helper)
    {
        if (status.empty())
        {
            cmd->status = "Success";
            cmd->result = json::Value(json::Value::TYPE_ARRAY);
            for (size_t i = 0; i < services.size(); ++i)
                cmd->result.append(services[i]->toJson());
        }
        else
        {
            cmd->status = "Failed";
            cmd->result = status;
        }

        m_pendedCommands[helper].remove(cmd);
        if (m_device && m_service)
            m_service->asyncUpdateCommand(m_device, cmd);
    }


    /**
     * @brief Update 'characteristics' command.
     */
    void onHelperCharacteristics(const String &status, const std::vector<bluepy::CharacteristicPtr> &chars,
                                 devicehive::CommandPtr cmd, bluepy::PeripheralPtr helper)
    {
        if (status.empty())
        {
            cmd->status = "Success";
            cmd->result = json::Value(json::Value::TYPE_ARRAY);
            for (size_t i = 0; i < chars.size(); ++i)
                cmd->result.append(chars[i]->toJson());
        }
        else
        {
            cmd->status = "Failed";
            cmd->result = status;
        }

        m_pendedCommands[helper].remove(cmd);
        if (m_device && m_service)
            m_service->asyncUpdateCommand(m_device, cmd);
    }

    /**
     * @brief Update 'read' command.
     */
    void onHelperCharRead(const String &status, const String &value, devicehive::CommandPtr cmd, bluepy::PeripheralPtr helper)
    {
        if (status.empty())
        {
            //cmd->result["handle"] = handle;
            cmd->result["valueHex"] = value;
            cmd->status = "Success";
        }
        else
        {
            cmd->status = "Failed";
            cmd->result = status;
        }

        m_pendedCommands[helper].remove(cmd);
        if (m_device && m_service)
            m_service->asyncUpdateCommand(m_device, cmd);
    }


    /**
     * @brief Update 'write' command.
     */
    void onHelperCharWrite(const String &status, devicehive::CommandPtr cmd, bluepy::PeripheralPtr helper)
    {
        if (status.empty())
        {
            cmd->status = "Success";
            //cmd->result["valueHex"] = value;
        }
        else
        {
            cmd->status = "Failed";
            cmd->result = status;
        }

        m_pendedCommands[helper].remove(cmd);
        if (m_device && m_service)
            m_service->asyncUpdateCommand(m_device, cmd);
    }

private:

    void AJ_init()
    {
        using namespace alljoyn;

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
        HIVELOG_INFO(m_log, "connected to BUS: \"" << m_AJ_bus->GetUniqueName().c_str() << "\"");

        m_AJ_bus->RegisterBusListener(*this);

        if (1) // initialize manager object
        {
            QStatus status;
            m_AJ_mngr.reset(new ManagerObj(m_ios, *m_AJ_bus, this, m_delayed, m_bluetooth));

            status = m_AJ_bus->RegisterBusObject(*m_AJ_mngr);
            AJ_check(status, "unable to register manager object");
        }

        ajn::services::AboutPropertyStoreImpl* props = new ajn::services::AboutPropertyStoreImpl();
        AJ_fillAbout(props);

        ajn::services::AboutServiceApi::Init(*m_AJ_bus, *props);
        if (!ajn::services::AboutServiceApi::getInstance())
            throw std::runtime_error("cannot create about service");

        status = ajn::services::AboutServiceApi::getInstance()->Register(SERVICE_PORT);
        AJ_check(status, "failed to register about service");

        status = m_AJ_bus->RegisterBusObject(*ajn::services::AboutServiceApi::getInstance());
        AJ_check(status, "failed to register about bus object");


        if (1)
        {
            using namespace ajn::services;

            //ControlPanelService* service = ControlPanelService::getInstance();
            ControlPanelControllee *controllee = m_AJ_mngr->getControllee();
            //status = service->initControllee(m_AJ_bus.get(), controllee);
            //AJ_check(status, "failed to init controllee");

            //m_delayed->callLater(10000, boost::bind(&This::onAddCPPage, shared_from_this(), controllee));

            String ST_meta_str = "{objectPrefix: 'SensorTag', objectPath: '/SensorTag', maximumAttribute: 136, "
                    "interfaceNames: {"
                        "'f000aa00-0451-4000-b000-000000000000': 'IR_TemperatureService',"
                        "'f000aa10-0451-4000-b000-000000000000': 'AccelerometerService',"
                        "'f000aa20-0451-4000-b000-000000000000': 'HumidityService',"
                        "'f000aa30-0451-4000-b000-000000000000': 'MagnetometerService',"
                        "'f000aa40-0451-4000-b000-000000000000': 'BarometerService',"
                        "'f000aa50-0451-4000-b000-000000000000': 'GyroscopeService',"
                        "'f000aa60-0451-4000-b000-000000000000': 'TestService',"
                        "'f000ccc0-0451-4000-b000-000000000000': 'ConnectionControlService',"
                        "'f000ffc0-0451-4000-b000-000000000000': 'OAD_Service'"
                    "}}";

            if (!m_sensorTag.empty())
            {
                m_delayed->callLater(5000, boost::bind(&alljoyn::ManagerObj::impl_createDevice,
                                m_AJ_mngr, m_sensorTag, json::fromStr(ST_meta_str)));
            }
        }

        std::vector<qcc::String> interfaces;
        interfaces.push_back(MANAGER_IFACE_NAME);
        interfaces.push_back(RAW_IFACE_NAME);
        status = ajn::services::AboutServiceApi::getInstance()->AddObjectDescription(MANAGER_OBJ_PATH, interfaces);
        AJ_check(status, "failed to add object description");


        ajn::SessionOpts opts(ajn::SessionOpts::TRAFFIC_MESSAGES, true, ajn::SessionOpts::PROXIMITY_ANY, ajn::TRANSPORT_ANY);
        ajn::SessionPort sp = SERVICE_PORT;
        status = m_AJ_bus->BindSessionPort(sp, opts, *this);
        AJ_check(status, "unable to bind service port");

        status = m_AJ_bus->AdvertiseName(m_AJ_bus->GetUniqueName().c_str(), ajn::TRANSPORT_ANY);
        AJ_check(status, "unable to advertise name");

        status = ajn::services::AboutServiceApi::getInstance()->Announce();
        AJ_check(status, "unable to announce");
    }


//    void onAddCPPage(ajn::services::ControlPanelControllee *controllee)
//    {
//        using namespace ajn::services;
//        QStatus status;

//        if (controllee)
//        {
//            std::cerr << "adding new page\n";

//            ControlPanelControlleeUnit *unit = new ControlPanelControlleeUnit("Device_Test_Page");
//            status = controllee->addControlPanelUnit(unit);
//            AJ_check(status, "cannot add controlpanel unit");

//            ControlPanel *root_cp = ControlPanel::createControlPanel(LanguageSets::get("btle_gw_lang_set"));
//            if (!root_cp) throw std::runtime_error("cannot create controlpanel");
//            status = unit->addControlPanel(root_cp);
//            AJ_check(status, "cannot add root controlpanel");

//            Container *root = new Container("root2", NULL);
//            status = root_cp->setRootWidget(root);
//            AJ_check(status, "cannot set root widget");
//            root->setEnabled(true);
//            root->setIsSecured(false);
//            root->setBgColor(0x200);
//            if (1)
//            {
//                std::vector<qcc::String> v;
//                v.push_back("Device management (test)");
//                root->setLabels(v);
//            }
//            if (1)
//            {
//                std::vector<uint16_t> v;
//                v.push_back(VERTICAL_LINEAR);
//                v.push_back(HORIZONTAL_LINEAR);
//                root->setHints(v);
//            }

//            status = unit->registerObjects(m_AJ_bus.get());
//            AJ_check(status, "init failed2");
//        }

//        ControlPanelService* service = ControlPanelService::getInstance();
//        status = service->shutdownControllee();
//        AJ_check(status, "failed to shutdown controllee");

//        m_delayed->callLater(5000, boost::bind(&This::onInitControllee, shared_from_this(), controllee));
//    }

//    void onInitControllee(ajn::services::ControlPanelControllee *controllee)
//    {
//        using namespace ajn::services;
//        QStatus status;

//        std::cerr << "init controllee again...";
//        ControlPanelService* service = ControlPanelService::getInstance();
//        status = service->initControllee(m_AJ_bus.get(), controllee);
//        AJ_check(status, "failed to init controllee again");
//    }


    void AJ_fillAbout(ajn::services::AboutPropertyStoreImpl *props)
    {
        props->setDeviceId("58b02520-b101-11e4-ab27-0800200c9a66");
        props->setAppId("620b7840-b101-11e4-ab27-0800200c9a66");

        std::vector<qcc::String> languages(1);
        languages[0] = "en";
        props->setSupportedLangs(languages);
        props->setDefaultLang("en");

        props->setAppName("Manager Obj", "en");
        props->setModelNumber("N/A");
        props->setDateOfManufacture("1999-01-01");
        props->setSoftwareVersion("0.0.0 build 1");
        props->setAjSoftwareVersion(ajn::GetVersion());
        props->setHardwareVersion("1.0a");

        props->setDeviceName("BLE gateway", "en");
        props->setDescription("This is an Alljoyn to BLE gateway", "en");
        props->setManufacturer("DataArt", "en");

        props->setSupportUrl("http://www.devicehive.com");
    }

private: // ajn::BusListener implementation

    virtual void ListenerRegistered(ajn::BusAttachment *bus)
    {
        HIVELOG_DEBUG(m_log, "listener registered for: \"" << bus->GetUniqueName().c_str() << "\"");
    }

    virtual void ListenerUnregistered()
    {
        HIVELOG_DEBUG(m_log, "listener unregistered");
    }

    virtual void FoundAdvertisedName(const char* name, ajn::TransportMask transport, const char* namePrefix)
    {
        HIVELOG_DEBUG(m_log, "found advertized name: \"" << name << "\", prefix: \"" << namePrefix << "\"");
    }

    virtual void LostAdvertisedName(const char* name, ajn::TransportMask transport, const char* namePrefix)
    {
        HIVELOG_DEBUG(m_log, "lost advertized name: \"" << name << "\", prefix: \"" << namePrefix << "\"");
    }

    virtual void NameOwnerChanged(const char* busName, const char* previousOwner, const char* newOwner)
    {
        HIVELOG_DEBUG(m_log, "name owner changed, bus name: \"" << (busName?busName:"<null>")
                  << "\", from: \"" << (previousOwner?previousOwner:"<null>")
                  << "\", to: \"" << (newOwner?newOwner:"<null>")
                  << "\"");
    }

    virtual void PropertyChanged(const char* propName, const ajn::MsgArg* propValue)
    {
        HIVELOG_DEBUG(m_log, "property changed, name: \"" << propName << "\"");
    }

    virtual void BusStopping()
    {
        HIVELOG_DEBUG(m_log, "bus stopping");
    }

    virtual void BusDisconnected()
    {
        HIVELOG_DEBUG(m_log, "bus disconnected");
    }

private: // SessionPortListener

    virtual bool AcceptSessionJoiner(ajn::SessionPort sessionPort, const char* joiner, const ajn::SessionOpts& opts)
    {
        if (sessionPort != alljoyn::SERVICE_PORT)
        {
            HIVELOG_WARN(m_log, "rejecting join attempt on unexpected session port " << sessionPort);
            return false;
        }

        HIVELOG_INFO(m_log, "accepting join attempt from \"" << joiner << "\"");
        return true;
    }

    virtual void SessionJoined(ajn::SessionPort sessionPort, ajn::SessionId id, const char* joiner)
    {
        HIVELOG_INFO(m_log, "session #" << id << " joined on "
                  << sessionPort << " port (joiner: \""
                    << joiner << "\")");
    }


private:
    boost::shared_ptr<ajn::BusAttachment> m_AJ_bus;
    boost::shared_ptr<alljoyn::ManagerObj> m_AJ_mngr;

private:
    String m_helperPath; ///< @brief The helper path.
    bluetooth::DevicePtr m_bluetooth;  ///< @brief The bluetooth device.

    devicehive::CommandPtr m_pendingScanCmd;
    basic_app::DelayedTask::SharedPtr m_pendingScanCmdTimeout;
    std::set<String> m_scanReportedDevices; // set of devices already reported

    String m_sensorTag; // MAC to join sensor tag

private:
    devicehive::IDeviceServicePtr m_service; ///< @brief The cloud service.
    bool m_disableWebsockets;       ///< @brief No automatic websocket switch.
    bool m_disableWebsocketPingPong; ///< @brief Disable websocket PING/PONG messages.

private:
    devicehive::DevicePtr m_device; ///< @brief The device.
    devicehive::NetworkPtr m_network; ///< @brief The network.
    String m_lastCommandTimestamp; ///< @brief The timestamp of the last received command.
    bool m_deviceRegistered; ///< @brief The DeviceHive cloud "registered" flag.

private:
    // TODO: list of pending commands
    std::vector<devicehive::NotificationPtr> m_pendingNotifications; ///< @brief The list of pending notification.
};


/**
 * @brief Get log file name from command line.
 */
inline String getLogFileName(int argc, const char* argv[])
{
    String fileName = "btle_gw.log";

    for (int i = 0; i < argc; ++i)
    {
        if (boost::iequals(argv[i], "--log") && i+1<argc)
            fileName = argv[++i];
    }

    return fileName;
}


/// @brief The BTLE gateway application entry point.
/**
Creates the Application instance and calls its Application::run() method.

@param[in] argc The number of command line arguments.
@param[in] argv The command line arguments.
*/
inline void main(int argc, const char* argv[])
{
    { // configure logging
        using namespace log;

        Target::File::SharedPtr file = Target::File::create(getLogFileName(argc, argv));
        file->setAutoFlushLevel(LEVEL_TRACE)
             .setMaxFileSize(1*1024*1024)
             .setNumberOfBackups(1)
             .startNew();
        file->setFormat(Format::create("%T [%I] %N %L %M\n"));

        Target::SharedPtr console = Logger::root().getTarget();
        console->setFormat(Format::create("%N: %M\n"))
                .setMinimumLevel(LEVEL_INFO);

        Logger::root().setTarget(Target::Tie::create(file, console))
                      .setLevel(LEVEL_TRACE);

        // disable annoying messages
        Logger("/hive/websocket").setTarget(file);
        Logger("/hive/http").setTarget(file)
                            .setLevel(LEVEL_DEBUG);
    }

    Application::create(argc, argv)->run();
}

} // btle_gw namespace


///////////////////////////////////////////////////////////////////////////////
/** @page page_btle_gw BTLE gateway example

This page is under construction!

The libbluetooth-dev package should be installed.

@example examples/btle_gw.hpp
*/

#endif // __EXAMPLES_BTLE_GW_HPP_
