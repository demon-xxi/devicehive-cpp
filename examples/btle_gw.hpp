/** @file
@brief The BTLE gateway example.
@author Sergey Polichnoy <sergey.polichnoy@dataart.com>
@see @ref page_simple_gw
*/
#ifndef __EXAMPLES_BTLE_GW_HPP_
#define __EXAMPLES_BTLE_GW_HPP_

#include <DeviceHive/restful.hpp>
#include <DeviceHive/websocket.hpp>
#include "basic_app.hpp"

// 'libbluetooth-dev' should be installed
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include <sys/types.h>
#include <sys/wait.h>

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


// bluepy-helper interface
namespace bluepy
{

/**
 * @brief The sub process.
 *
 * Uses pipes for stderr, stdout, stdin.
 */
class Process:
    public boost::enable_shared_from_this<Process>
{
public:
    typedef boost::function1<void, boost::system::error_code> TerminatedCallback;
    typedef boost::function1<void, String> ReadLineCallback;


    /**
     * @brief Set terminated callback.
     */
    void callWhenTerminated(TerminatedCallback cb)
    {
        m_terminated_cb = cb;
    }

    /**
     * @brief Set STDERR read line callback.
     */
    void callOnNewStderrLine(ReadLineCallback cb)
    {
        m_stderr_line_cb = cb;
    }

    /**
     * @brief Set STDERR read line callback.
     */
    void callOnNewStdoutLine(ReadLineCallback cb)
    {
        m_stdout_line_cb = cb;
    }

protected:

    /**
     * @brief Create sub process.
     * @param ios The IO service.
     * @param argv List of arguments. First item is the executable path.
     */
    Process(boost::asio::io_service &ios, const std::vector<String> &argv, const String &name)
        : m_stderr(ios)
        , m_stdout(ios)
        , m_stdin(ios)
        , m_valid(false)
        , m_pid(-1)
        , m_log("bluepy/Process/" + name)
    {
        int pipe_stderr[2];
        int pipe_stdout[2];
        int pipe_stdin[2];

        if (pipe(pipe_stderr) != 0) throw std::runtime_error("cannot create stderr pipe");
        if (pipe(pipe_stdout) != 0) throw std::runtime_error("cannot create stdout pipe");
        if (pipe(pipe_stdin) != 0)  throw std::runtime_error("cannot create stdin pipe");

        m_pid = fork();
        if (m_pid < 0) throw std::runtime_error("cannot create child process");

        if (m_pid) // parent
        {
            // close unused pipe ends
            close(pipe_stderr[1]);
            close(pipe_stdout[1]);
            close(pipe_stdin[0]);

            // assign used pipe ends
            m_stderr.assign(pipe_stderr[0]);
            m_stdout.assign(pipe_stdout[0]);
            m_stdin.assign(pipe_stdin[1]);

            m_valid = true;
        }
        else // child
        {
            // copy used pipe ends
            dup2(pipe_stderr[1], STDERR_FILENO);
            dup2(pipe_stdout[1], STDOUT_FILENO);
            dup2(pipe_stdin[0], STDIN_FILENO);

            // close all pipe ends
            close(pipe_stderr[0]);
            close(pipe_stderr[1]);
            close(pipe_stdout[0]);
            close(pipe_stdout[1]);
            close(pipe_stdin[0]);
            close(pipe_stdin[1]);

            char* c_argv[256];
            size_t c_argc = 0;
            for (; c_argc < argv.size() && c_argc < 255; ++c_argc)
                c_argv[c_argc] = const_cast<char*>(argv[c_argc].c_str());
            c_argv[c_argc++] = 0;

            // execute
            execv(c_argv[0], c_argv);
            exit(-1); // if we are here - something wrong
        }

        HIVELOG_TRACE(m_log, "created");
    }

public:

    /**
     * @brief The factory method.
     */
    static boost::shared_ptr<Process> create(boost::asio::io_service &ios,
                                             const std::vector<String> &argv,
                                             const String &name)
    {
        boost::shared_ptr<Process> pthis(new Process(ios, argv, name));
        pthis->readStderr();
        pthis->readStdout();
        return pthis;
    }


    /**
     * @brief Terminate sub process and get status.
     */
    int terminate()
    {
        HIVELOG_TRACE(m_log, "killing...");
        kill(m_pid, SIGINT);

        int status = 0;
        HIVELOG_TRACE(m_log, "waiting...");
        waitpid(m_pid, &status, 0); // warning blocking call!

        HIVELOG_INFO(m_log, "status:" << status);
        return status;
    }


    /**
     * @brief Trivial destructor.
     */
    virtual ~Process()
    {
        HIVELOG_TRACE(m_log, "deleted");
    }


    /**
     * @brief Check process is valid.
     */
    bool isValid() const
    {
        return m_valid;
    }

public:

    /**
     * @brief Write command to the STDIN pipe.
     */
    void writeStdin(const String &cmd)
    {
        bool in_progress = !m_in_queue.empty();
        m_in_queue.push_back(cmd);

        if (!in_progress)
            writeNextStdin();
    }

private:

    /**
     * @brief Write next command from the queue to STDIN pipe.
     */
    void writeNextStdin()
    {
        if (!m_in_queue.empty() && m_valid)
        {
            HIVELOG_INFO(m_log, "writing \"" << escape(m_in_queue.front()) << "\" to STDIN");
            boost::asio::async_write(m_stdin,
                boost::asio::buffer(m_in_queue.front()),
                boost::asio::transfer_all(), boost::bind(&Process::onWriteStdin, shared_from_this(),
                    boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
        }
    }

    /**
     * @brief Got some data written into STDIN pipe.
     */
    void onWriteStdin(boost::system::error_code err, size_t len)
    {
        // std::cerr << "STDIN write:" << err << " " << len << " bytes\n";
        if (!err)
        {
            if (!m_in_queue.empty())
            {
                //len == m_in_queue.front().size()
                HIVE_UNUSED(len);

                m_in_queue.pop_front();

                if (!m_in_queue.empty())
                {
                    // write next command ASAP
                    m_stdin.get_io_service().post(boost::bind(
                        &Process::writeNextStdin, shared_from_this()));
                }
            }
        }
        else
        {
            HIVELOG_ERROR(m_log, "STDIN write error: ["
                        << err << "] " << err.message());
            handleError(err);
        }
    }

private:

    /**
     * @brief Start asynchronous STDERR reading.
     */
    void readStderr()
    {
        boost::asio::async_read(m_stderr, m_err_buf, boost::asio::transfer_at_least(1),
            boost::bind(&Process::onReadStderr, shared_from_this(),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }


    /**
     * @brief Start asynchronous STDOUT reading.
     */
    void readStdout()
    {
        boost::asio::async_read(m_stdout, m_out_buf, boost::asio::transfer_at_least(1),
            boost::bind(&Process::onReadStdout, shared_from_this(),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }

private:

    /**
     * @brief Handle error.
     */
    void handleError(boost::system::error_code err)
    {
        m_valid = false;
        if (m_terminated_cb)
        {
            m_stderr.get_io_service().post(boost::bind(m_terminated_cb, err));
            m_terminated_cb = TerminatedCallback(); // send once
        }
    }

    /**
     * @brief Handle STDERR line.
     */
    void handleStderr(const String &line)
    {
        HIVELOG_INFO(m_log, "STDERR line: \"" << escape(line) << "\"");
        if (m_stderr_line_cb)
        {
            m_stderr.get_io_service().post(boost::bind(m_stderr_line_cb, line));
        }
    }


    /**
     * @brief Handle STDOUT line.
     */
    void handleStdout(const String &line)
    {
        HIVELOG_INFO(m_log, "STDOUT line: \"" << escape(line) << "\"");
        if (m_stdout_line_cb)
        {
            m_stdout.get_io_service().post(boost::bind(m_stdout_line_cb, line));
        }
    }


    /**
     * @brief Got some data read from STDERR pipe.
     */
    void onReadStderr(boost::system::error_code err, size_t len)
    {
        //std::cerr << "STDOUT read:" << err << " " << len << " bytes\n";
        if (!err)
        {
            String line;
            m_err_buf.commit(len);
            while (readLine(m_err_buf, line))
                handleStderr(line);

            if (m_valid) // continue
                readStderr();
        }
        else
        {
            HIVELOG_ERROR(m_log, "STDERR read error: ["
                        << err << "] " << err.message());
            handleError(err);
        }
    }


    /**
     * @brief Got some data read from STDOUT pipe.
     */
    void onReadStdout(boost::system::error_code err, size_t len)
    {
        //std::cerr << "STDOUT read:" << err << " " << len << " bytes\n";
        if (!err)
        {
            //std::cerr << "[" << len << "] ";

            String line;
            m_err_buf.commit(len);
            while (readLine(m_out_buf, line))
                handleStdout(line);

            if (m_valid) // continue
                readStdout();
        }
        else
        {
            HIVELOG_ERROR(m_log, "STDOUT read error: ["
                        << err << "] " << err.message());
            handleError(err);
        }
    }


    /**
     * @brief Try to read line from stream buffer.
     * @return `true` if line has read.
     */
    bool readLine(boost::asio::streambuf &buf, String &line)
    {
        if (size_t pos = readLine_(buf.data(), line))
        {
            buf.consume(pos);
            return true;
        }

        return false;
    }

    template<typename Buffer>
    size_t readLine_(const Buffer &buf, String &line)
    {
        return readLine_(boost::asio::buffers_begin(buf),
                         boost::asio::buffers_end(buf), line);
    }

    template<typename In>
    size_t readLine_(In first, In last, String &line)
    {
        In end = std::find(first, last, '\n');

        if (end != last)
        {
            line.assign(first, end);
            return std::distance(first, end)+1; // including '\n'
        }

        return 0; // not found
    }

public:

    /**
     * @brief Escape non-printable symbols.
     */
    static String escape(const String &str)
    {
        String res;
        res.reserve(str.size());

        for (size_t i = 0; i < str.size(); ++i)
        {
            switch (str[i])
            {
                case '\n': res += "\\n"; break;
                case '\r': res += "\\r"; break;
                default: res += str[i]; break;
            }
        }

        return res;
    }

private:
    boost::asio::posix::stream_descriptor m_stderr; // child's stderr pipe. read end.
    boost::asio::posix::stream_descriptor m_stdout; // child's stdout pipe. read end.
    boost::asio::posix::stream_descriptor m_stdin;  // child's stdin pipe. write end.

    boost::asio::streambuf m_err_buf;
    boost::asio::streambuf m_out_buf;
    std::list<String> m_in_queue;

    bool m_valid;
    pid_t m_pid; // child's PID

    // callbacks
    TerminatedCallback m_terminated_cb;
    ReadLineCallback m_stderr_line_cb;
    ReadLineCallback m_stdout_line_cb;

    hive::log::Logger m_log;
};


/**
 * @brief Process shared pointer type.
 */
typedef boost::shared_ptr<Process> ProcessPtr;

} // bluepy namespace


/// @brief The simple gateway application.
/**
This application controls only one device connected via serial port or socket or pipe!

@see @ref page_simple_gw
*/
class Application:
    public basic_app::Application,
    public devicehive::IDeviceServiceEvents
{
    typedef basic_app::Application Base; ///< @brief The base type.
    typedef Application This; ///< @brief The type alias.

public:

    /// @brief Bluetooth device.
    class BluetoothDevice:
        public boost::enable_shared_from_this<BluetoothDevice>,
        private hive::NonCopyable
    {
    protected:

        /// @brief Main constructor.
        BluetoothDevice(boost::asio::io_service &ios, const String &name)
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
        typedef boost::shared_ptr<BluetoothDevice> SharedPtr;

        /// @brief The factory method.
        static SharedPtr create(boost::asio::io_service &ios, const String &name)
        {
            return SharedPtr(new BluetoothDevice(ios, name));
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

        /**
         * @brief Start scan operation.
         */
        void scanStart(const json::Value &opts)
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
        }

    public:
        void asyncReadSome()
        {
            if (!m_read_active && m_stream.is_open())
            {
                m_read_active = true;
                boost::asio::async_read(m_stream, m_read_buf,
                    boost::asio::transfer_at_least(1),
                    boost::bind(&BluetoothDevice::onReadSome, this->shared_from_this(),
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

                        m_scan_devices[addr] = name;
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

            return "(unknown)";
        }

    public:


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
    typedef BluetoothDevice::SharedPtr BluetoothDevicePtr;

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

        String baseUrl = "http://ecloud.dataart.com/ecapi8";
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
                std::cout << "\t--bluetooth <BTL device name or address>\n";

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
        }

        if (pthis->m_helperPath.empty())
            throw std::runtime_error("no helper provided");

        pthis->m_bluetooth = BluetoothDevice::create(pthis->m_ios, bluetoothName);
        pthis->m_network = devicehive::Network::create(networkName, networkKey, networkDesc);
        pthis->m_device = devicehive::Device::create(deviceId, deviceName, deviceKey,
            devicehive::Device::Class::create("BTLE gateway", "0.1", false, DEVICE_OFFLINE_TIMEOUT),
                                                     pthis->m_network);
        pthis->m_device->status = "Online";

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

protected:

    /// @brief Start the application.
    /**
    Tries to open stream device.
    */
    virtual void start()
    {
        HIVELOG_TRACE_BLOCK(m_log, "start()");

        Base::start();
        m_service->asyncConnect();
        m_delayed->callLater( // ASAP
            boost::bind(&This::tryToOpenBluetoothDevice,
                shared_from_this()));
    }


    /// @brief Stop the application.
    /**
    Stops the "open" timer.
    */
    virtual void stop()
    {
        HIVELOG_TRACE_BLOCK(m_log, "stop()");

        m_service->cancelAll();
        if (m_bluetooth)
            m_bluetooth->close();

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

    /// @copydoc devicehive::IDeviceServiceEvents::onConnected()
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


    /// @copydoc devicehive::IDeviceServiceEvents::onServerInfo()
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
            command->result = BluetoothDevice::getDevicesInfo();
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

            m_bluetooth->scanStart(command->params);
            m_bluetooth->asyncReadSome();

            const int def_timeout = boost::iequals(command->name, "scan") ? 20 : 0;
            const int timeout = command->params.get("timeout", def_timeout).asUInt8(); // limited range [0..255]
            if (timeout != 0)
            {
                m_pendingScanCmdTimeout = m_delayed->callLater(timeout*1000,
                    boost::bind(&This::onScanCommandTimeout, shared_from_this()));
            }

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
        else if (boost::iequals(command->name, "gatt/primary"))
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
        else if (boost::iequals(command->name, "x/gatt/primary"))
        {
            String device = command->params.get("device", json::Value::null()).asString();
            bluepy::ProcessPtr proc = findHelper(device);
            if (!proc) throw std::runtime_error("cannot create helper pipe");

            proc->writeStdin("primary\n");
            //return false;
        }
        else if (boost::iequals(command->name, "gatt/characteristics"))
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

    std::map<String, bluepy::ProcessPtr> m_helpers;
    bluepy::ProcessPtr findHelper(const String &device)
    {
        std::map<String, bluepy::ProcessPtr>::const_iterator i = m_helpers.find(device);
        if (i != m_helpers.end())
        {
            if (i->second->isValid())
                return i->second;
        }

        std::vector<String> argv;
        argv.push_back(m_helperPath);

        bluepy::ProcessPtr helper = bluepy::Process::create(m_ios, argv, device);
        helper->callWhenTerminated(boost::bind(&This::onHelperTerminated, shared_from_this(), _1, device));
        m_helpers[device] = helper;
        return helper;
    }


    /**
     * @brief Helper terminated.
     */
    void onHelperTerminated(boost::system::error_code err, const String &device)
    {
        HIVE_UNUSED(err);

        // release helper
        m_helpers.erase(device);
    }

private:
    String m_helperPath; ///< @brief The helper path.
    BluetoothDevicePtr m_bluetooth;  ///< @brief The bluetooth device.

    devicehive::CommandPtr m_pendingScanCmd;
    basic_app::DelayedTask::SharedPtr m_pendingScanCmdTimeout;

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


/// @brief The BTLE gateway application entry point.
/**
Creates the Application instance and calls its Application::run() method.

@param[in] argc The number of command line arguments.
@param[in] argv The command line arguments.
*/
inline void main(int argc, const char* argv[])
{
    { // configure logging
        using namespace hive::log;

        Target::File::SharedPtr file = Target::File::create("btle_gw.log");
        file->setAutoFlushLevel(LEVEL_TRACE)
             .setMaxFileSize(1*1024*1024)
             .setNumberOfBackups(1)
             .startNew();

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
