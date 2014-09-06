/** @file
@brief The DeviceHive Restful service.
@author Sergey Polichnoy <sergey.polichnoy@dataart.com>
*/
#ifndef __DEVICEHIVE_RESTFUL_HPP_
#define __DEVICEHIVE_RESTFUL_HPP_

#include <DeviceHive/service.hpp>

#include <hive/http.hpp>

namespace devicehive
{

/// @brief The Restful service API.
/**
This class helps devices to comminucate with server via RESTful interface.

There are a lot of API wrapper methods:
    - asyncGetServerInfo()
    - asyncRegisterDevice()
    - asyncUpdateDeviceData()
    - ...

All of these methods accept callback functor which is used to report result of each operation.
*/
class RestfulServiceBase:
    public boost::enable_shared_from_this<RestfulServiceBase>
{
    typedef RestfulServiceBase This; ///< @brief The type alias.

protected:

    /// @brief The main constructor.
    /**
    @param[in] httpClient The HTTP client instance.
    @param[in] baseUrl The base URL.
    @param[in] name The custom name. Optional.
    */
    RestfulServiceBase(http::ClientPtr httpClient, String const& baseUrl, String const& name)
        : m_http(httpClient)
        , m_http_major(1)
        , m_http_minor(0)
        , m_log("/devicehive/rest/" + name)
        , m_baseUrl(baseUrl)
        , m_timeout_ms(60000)
    {}

public:

    /// @brief Trivial destructor.
    virtual ~RestfulServiceBase()
    {}

public:

    /// @brief The shared pointer type.
    typedef boost::shared_ptr<RestfulServiceBase> SharedPtr;


    /// @brief The factory method.
    /**
    @param[in] httpClient The HTTP client instance.
    @param[in] baseUrl The base URL.
    @param[in] name The optional custom name.
    @return The new RESTful service instance.
    */
    static SharedPtr create(http::ClientPtr httpClient,
        String const& baseUrl, String const& name = String())
    {
        return SharedPtr(new This(httpClient, baseUrl, name));
    }

public:

    /// @brief Get the web request timeout.
    /**
    @return The web request timeout, milliseconds.
    */
    size_t getTimeout() const
    {
        return m_timeout_ms;
    }


    /// @brief Set web request timeout.
    /**
    @param[in] timeout_ms The web request timeout, milliseconds.
    @return Self reference.
    */
    This& setTimeout(size_t timeout_ms)
    {
        m_timeout_ms = timeout_ms;
        return *this;
    }

public:


    /// @brief Set HTTP version.
    /**
    @param[in] major The HTTP major version.
    @param[in] minor The HTTP minor version.
    */
    This& setHttpVersion(int major, int minor)
    {
        m_http_major = major;
        m_http_minor = minor;
        return *this;
    }

public:

    /// @brief Get HTTP client.
    /**
    @return The HTTP client.
    */
    http::ClientPtr getHttpClient() const
    {
        return m_http;
    }


    /// @brief Cancel all requests.
    void cancelAll()
    {
        // TODO: cancel only related requests
        m_http->cancelAll();
    }


/// @name Server Info
/// @{
public:

    /// @brief The "server info" callback type.
    typedef boost::function2<void, boost::system::error_code, devicehive::ServerInfo> ServerInfoCallback;


    /// @brief Get the server info.
    /**
    @param[in] callback The callback functor.
    @return Corresponding HTTP task.
    */
    http::Client::TaskPtr asyncGetServerInfo(ServerInfoCallback callback)
    {
        http::Url::Builder urlb(m_baseUrl);
        urlb.appendPath("info");

        http::RequestPtr req = http::Request::GET(urlb.build());
        req->setVersion(m_http_major, m_http_minor);

        HIVELOG_DEBUG(m_log, "getting server info");
        http::Client::TaskPtr task = m_http->send(req, m_timeout_ms);
        if (task)
        {
            task->callWhenDone(boost::bind(&This::onServerInfo,
                shared_from_this(), task, callback));
        }
        return task;
    }

private:

    /// @brief The "server info" completion handler.
    /**
    @param[in] task The HTTP task.
    @param[in] callback The callback functor.
    */
    void onServerInfo(http::Client::TaskPtr task, ServerInfoCallback callback)
    {
        ServerInfo info;

        boost::system::error_code err = verifyTaskResponse(task, "server info");
        if (!err)
        {
            try
            {
                const json::Value jval = json::fromStr(task->response->getContent());
                HIVELOG_DEBUG(m_log, "got \"server info\" response: " << json::toStrHH(jval));

                info.api_version = jval["apiVersion"].asString();
                info.timestamp = jval["serverTimestamp"].asString();
                info.alternativeUrl = jval["webSocketServerUrl"].asString();
            }
            catch (std::exception const& ex)
            {
                HIVELOG_ERROR(m_log, "failed to parse \"server info\" response: " << ex.what());
                err = boost::asio::error::fault; // TODO: useful error code
            }
        }

        if (callback)
            callback(err, info);
    }
/// @}


/// @name Device
/// @{
public:

    /// @brief The "register device" callback type.
    typedef boost::function2<void, boost::system::error_code, devicehive::DevicePtr> RegisterDeviceCallback;

    /// @brief The "update device" callback type.
    typedef boost::function2<void, boost::system::error_code, devicehive::DevicePtr> UpdateDeviceCallback;


    /// @brief Register device on the server.
    /**
    @param[in] device The device to register.
    @param[in] callback The callback functor.
    @return Corresponding HTTP task.
    */
    http::Client::TaskPtr asyncRegisterDevice(Device::SharedPtr device, RegisterDeviceCallback callback)
    {
        http::Url::Builder urlb(m_baseUrl);
        urlb.appendPath("device")
            .appendPath(device->id);

        const json::Value jcontent = Serializer::toJson(device);

        http::RequestPtr req = http::Request::PUT(urlb.build());
        req->addHeader(http::header::Content_Type, "application/json")
            .addHeader("Auth-DeviceID", device->id)
            .addHeader("Auth-DeviceKey", device->key)
            .setVersion(m_http_major, m_http_minor)
            .setContent(json::toStr(jcontent));

        HIVELOG_DEBUG(m_log, "registering device: " << json::toStrHH(jcontent));
        http::Client::TaskPtr task = m_http->send(req, m_timeout_ms);
        if (task)
        {
            task->callWhenDone(boost::bind(&This::onRegisterDevice,
                shared_from_this(), task, device, callback));
        }
        return task;
    }


    /// @brief Update device data on the server.
    /**
    @param[in] device The device to update.
    @param[in] callback The callback functor.
    @return Corresponding HTTP task.
    */
    http::Client::TaskPtr asyncUpdateDeviceData(Device::SharedPtr device, UpdateDeviceCallback callback)
    {
        http::Url::Builder urlb(m_baseUrl);
        urlb.appendPath("device")
            .appendPath(device->id);

        json::Value jcontent;
        jcontent["data"] = device->data;

        http::RequestPtr req = http::Request::PUT(urlb.build());
        req->addHeader(http::header::Content_Type, "application/json")
            .addHeader("Auth-DeviceID", device->id)
            .addHeader("Auth-DeviceKey", device->key)
            .setVersion(m_http_major, m_http_minor)
            .setContent(json::toStr(jcontent));

        HIVELOG_DEBUG(m_log, "updating device data: " << json::toStrHH(jcontent));
        http::Client::TaskPtr task = m_http->send(req, m_timeout_ms);
        if (task)
        {
            task->callWhenDone(boost::bind(&This::onUpdateDeviceData,
                shared_from_this(), task, device, callback));
        }
        return task;
    }

private:

    /// @brief The "register device" completion handler.
    /**
    @param[in] task The HTTP task.
    @param[in] device The device registered.
    @param[in] callback The callback functor.
    */
    void onRegisterDevice(http::Client::TaskPtr task, DevicePtr device, RegisterDeviceCallback callback)
    {
        boost::system::error_code err = verifyTaskResponse(task, "register device");
        if (!err)
        {
            if (task->response->getStatusCode() != http::status::NO_CONTENT)
            {
                try
                {
                    const json::Value jval = json::fromStr(task->response->getContent());
                    HIVELOG_DEBUG(m_log, "got \"register device\" response: " << json::toStrHH(jval));
                    Serializer::fromJson(jval, device);
                }
                catch (std::exception const& ex)
                {
                    HIVELOG_ERROR(m_log, "failed to parse \"register device\" response: " << ex.what());
                    err = boost::asio::error::fault; // TODO: useful error code
                }
            }
        }

        if (callback)
            callback(err, device);
    }


    /// @brief The "update device data" completion handler.
    /**
    @param[in] task The HTTP task.
    @param[in] device The device updated.
    @param[in] callback The callback functor.
    */
    void onUpdateDeviceData(http::Client::TaskPtr task, DevicePtr device, UpdateDeviceCallback callback)
    {
        boost::system::error_code err = verifyTaskResponse(task, "update device");
        if (!err)
        {
            if (task->response->getStatusCode() != http::status::NO_CONTENT)
            {
                try
                {
                    const json::Value jval = json::fromStr(task->response->getContent());
                    HIVELOG_DEBUG(m_log, "got \"update device\" response: " << json::toStrHH(jval));
                    Serializer::fromJson(jval, device);
                }
                catch (std::exception const& ex)
                {
                    HIVELOG_ERROR(m_log, "failed to parse \"update device\" response: " << ex.what());
                    err = boost::asio::error::fault; // TODO: useful error code
                }
            }
        }

        if (callback)
            callback(err, device);
    }
/// @}


/// @name Device command
/// @{
public:

    /// @brief The "poll commands" callback type.
    typedef boost::function3<void, boost::system::error_code, devicehive::DevicePtr, std::vector<devicehive::CommandPtr> > PollCommandsCallback;


    /// @brief Poll commands from the server.
    /**
    @param[in] device The device to poll commands for.
    @param[in] timestamp The timestamp of the last received command. Empty for server's "now".
    @param[in] callback The callback functor.
    @return Corresponding HTTP task.
    */
    http::Client::TaskPtr asyncPollCommands(DevicePtr device, String const& timestamp, PollCommandsCallback callback)
    {
        http::Url::Builder urlb(m_baseUrl);
        urlb.appendPath("device")
            .appendPath(device->id)
            .appendPath("command/poll");
        if (!timestamp.empty())
            urlb.appendQuery("timestamp=" + timestamp);

        http::RequestPtr req = http::Request::GET(urlb.build());
        req->addHeader("Auth-DeviceID", device->id)
            .addHeader("Auth-DeviceKey", device->key)
            .setVersion(m_http_major, m_http_minor);

        HIVELOG_DEBUG(m_log, "poll commands for \"" << device->id << "\"");
        http::Client::TaskPtr task = m_http->send(req, m_timeout_ms);
        if (task)
        {
            task->callWhenDone(boost::bind(&This::onPollCommands,
                shared_from_this(), task, device, callback));
        }
        return task;
    }

private:

    /// @brief The "poll commands" completion handler.
    /**
    @param[in] task The HTTP task.
    @param[in] device The device to poll commands for.
    @param[in] callback The callback functor.
    */
    void onPollCommands(http::Client::TaskPtr task, DevicePtr device, PollCommandsCallback callback)
    {
        std::vector<CommandPtr> commands;

        boost::system::error_code err = verifyTaskResponse(task, "poll commands");
        if (!err)
        {
            try
            {
                const json::Value jval = json::fromStr(task->response->getContent());
                HIVELOG_DEBUG(m_log, "got \"poll commands\" response: " << json::toStrHH(jval));
                if (jval.isArray())
                {
                    const size_t N = jval.size();
                    commands.reserve(N);

                    for (size_t i = 0; i < N; ++i)
                    {
                        CommandPtr command = Command::create();
                        Serializer::fromJson(jval[i], command);
                        commands.push_back(command);
                    }
                }
                else
                    throw std::runtime_error("response is not an array");
            }
            catch (std::exception const& ex)
            {
                HIVELOG_ERROR(m_log, "failed to parse \"poll commands\" response: " << ex.what());
                err = boost::asio::error::fault; // TODO: useful error code
            }
        }

        if (callback)
            callback(err, device, commands);
    }

public:

    /// @brief The "update command" callback type.
    typedef boost::function3<void, boost::system::error_code, devicehive::DevicePtr, devicehive::CommandPtr> UpdateCommandCallback;


    /// @brief Send command result to the server.
    /**
    @param[in] device The device.
    @param[in] command The command to update.
    @param[in] callback The callback functor.
    @return Corresponding HTTP task.
    */
    http::Client::TaskPtr asyncUpdateCommand(Device::SharedPtr device, CommandPtr command, UpdateCommandCallback callback = UpdateCommandCallback())
    {
        http::Url::Builder urlb(m_baseUrl);
        urlb.appendPath("device")
            .appendPath(device->id)
            .appendPath("command")
            .appendPath(boost::lexical_cast<String>(command->id));

        json::Value jcontent;
        jcontent["status"] = command->status;
        jcontent["result"] = command->result;

        http::RequestPtr req = http::Request::PUT(urlb.build());
        req->addHeader(http::header::Content_Type, "application/json")
            .addHeader("Auth-DeviceID", device->id)
            .addHeader("Auth-DeviceKey", device->key)
            .setVersion(m_http_major, m_http_minor)
            .setContent(json::toStr(jcontent));

        HIVELOG_DEBUG(m_log, "updating command: " << json::toStrHH(jcontent));
        http::Client::TaskPtr task = m_http->send(req, m_timeout_ms);
        if (task)
        {
            task->callWhenDone(boost::bind(&This::onUpdateCommand,
                shared_from_this(), task, device, command, callback));
        }
        return task;
    }

private:

    /// @brief The "update command" completion handler.
    /**
    @param[in] task The HTTP task.
    @param[in] device The corresponding device.
    @param[in] command The updated command.
    @param[in] callback The callback functor.
    */
    void onUpdateCommand(http::Client::TaskPtr task, Device::SharedPtr device, CommandPtr command, UpdateCommandCallback callback)
    {
        boost::system::error_code err = verifyTaskResponse(task, "update command");
        if (!err)
        {
            if (task->response->getStatusCode() != http::status::NO_CONTENT)
            {
                try
                {
                    const json::Value jval = json::fromStr(task->response->getContent());
                    HIVELOG_DEBUG(m_log, "got \"update command\" response: " << json::toStrHH(jval));
                    Serializer::fromJson(jval, command);
                }
                catch (std::exception const& ex)
                {
                    HIVELOG_ERROR(m_log, "failed to parse \"update command\" response: " << ex.what());
                    err = boost::asio::error::fault; // TODO: useful error code
                }
            }
        }

        if (callback)
            callback(err, device, command);
    }
/// @}


/// @name Device notification
/// @{
public:

    /// @brief The "insert notification" callback type.
    typedef boost::function3<void, boost::system::error_code, devicehive::DevicePtr, devicehive::NotificationPtr> InsertNotificationCallback;

    /// @brief Send notification to the server.
    /**
    @param[in] device The device.
    @param[in] notification The notification.
    @param[in] callback The callback functor.
    @return Corresponding HTTP task.
    */
    http::Client::TaskPtr asyncInsertNotification(Device::SharedPtr device, NotificationPtr notification, InsertNotificationCallback callback = InsertNotificationCallback())
    {
        http::Url::Builder urlb(m_baseUrl);
        urlb.appendPath("device")
            .appendPath(device->id)
            .appendPath("notification");

        const json::Value jcontent = Serializer::toJson(notification);

        http::RequestPtr req = http::Request::POST(urlb.build());
        req->addHeader(http::header::Content_Type, "application/json")
            .addHeader("Auth-DeviceID", device->id)
            .addHeader("Auth-DeviceKey", device->key)
            .setVersion(m_http_major, m_http_minor)
            .setContent(json::toStr(jcontent));

        HIVELOG_DEBUG(m_log, "inserting notification: " << json::toStrHH(jcontent));
        http::Client::TaskPtr task = m_http->send(req, m_timeout_ms);
        if (task)
        {
            task->callWhenDone(boost::bind(&This::onInsertNotification,
                shared_from_this(), task, device, notification, callback));
        }
        return task;
    }

private:

    /// @brief The "insert notification" completion handler.
    /**
    @param[in] task The HTTP task.
    @param[in] device The corresponding device.
    @param[in] notification The inserted notification.
    @param[in] callback The callback functor.
    */
    void onInsertNotification(http::Client::TaskPtr task, Device::SharedPtr device, NotificationPtr notification, InsertNotificationCallback callback)
    {
        boost::system::error_code err = verifyTaskResponse(task, "insert notification");
        if (!err)
        {
            if (task->response->getStatusCode() != http::status::NO_CONTENT)
            {
                try
                {
                    json::Value jval = json::fromStr(task->response->getContent());
                    HIVELOG_DEBUG(m_log, "got \"insert notification\" response: " << json::toStrHH(jval));
                    Serializer::fromJson(jval, notification);
                }
                catch (std::exception const& ex)
                {
                    HIVELOG_ERROR(m_log, "failed to parse \"insert notification\" response: " << ex.what());
                    err = boost::asio::error::fault; // TODO: useful error code
                }
            }
        }

        if (callback)
            callback(err, device, notification);
    }
/// @}

private:

    /// @brief Verify a HTTP task response.
    /**
    Checks response presence and status code.

    @param[in] task The HTTP task.
    @param[in] hint The operation hint to print error messages.
    @return The error code.
    */
    boost::system::error_code verifyTaskResponse(http::Client::TaskPtr task, const char *hint)
    {
        boost::system::error_code err = task->errorCode;

        if (err)
        {
            HIVELOG_WARN(m_log, "failed to get \"" << hint
                << "\": [" << err << "] " << err.message());
        }
        else if (!task->response)
        {
            HIVELOG_WARN(m_log, "failed to get \"" << hint << "\": no response");
            err = boost::asio::error::fault; // TODO: useful error code
        }
        else if (!task->response->isStatusSuccessful())
        {
            HIVELOG_WARN(m_log, "failed to get \"" << hint
                << "\": HTTP status: " << task->response->getStatusCode()
                << " " << task->response->getStatusPhrase());
            err = boost::asio::error::fault; // TODO: useful error code
        }

        return err;
    }

private:
    http::Client::SharedPtr m_http; ///< @brief The HTTP client.
    int m_http_major; ///< @brief The HTTP major version.
    int m_http_minor; ///< @brief The HTTP minor version.

    hive::log::Logger m_log;        ///< @brief The logger.
    http::Url m_baseUrl;            ///< @brief The base URL.
    size_t m_timeout_ms;            ///< @brief The HTTP request timeout, milliseconds.
};


/// @brief The REST service.
class RestfulService:
    public RestfulServiceBase,
    public IDeviceService
{
    typedef RestfulServiceBase Base; ///< @brief The base class.
    typedef RestfulService This; ///< @brief The type alias.

protected:

    /// @brief The main constructor.
    /**
    @param[in] httpClient The HTTP client instance.
    @param[in] baseUrl The base URL.
    @param[in] callbacks The events handler.
    @param[in] name The custom name. Optional.
    */
    RestfulService(http::ClientPtr httpClient, String const& baseUrl, boost::shared_ptr<IDeviceServiceEvents> callbacks, String const& name)
        : Base(httpClient, baseUrl, name)
        , m_callbacks(callbacks)
    {}

public:

    /// @brief The shared pointer type.
    typedef boost::shared_ptr<RestfulService> SharedPtr;


    /// @brief The factory method.
    /**
    @param[in] httpClient The HTTP client instance.
    @param[in] baseUrl The base URL.
    @param[in] callbacks The events handler.
    @param[in] name The custom name. Optional.
    @return The new instance.
    */
    static SharedPtr create(http::Client::SharedPtr httpClient, String const& baseUrl,
        boost::shared_ptr<IDeviceServiceEvents> callbacks, String const& name = String())
    {
        return SharedPtr(new This(httpClient, baseUrl, callbacks, name));
    }


    /// @brief Get the shared pointer.
    /**
    @return The shared pointer to this instance.
    */
    SharedPtr shared_from_this()
    {
        return boost::dynamic_pointer_cast<This>(Base::shared_from_this());
    }

public: // IDeviceService

    /// @copydoc IDeviceService::cancelAll()
    virtual void cancelAll()
    {
        Base::cancelAll();
        m_devices.clear();
    }

public:

    /// @copydoc IDeviceService::asyncConnect()
    virtual void asyncConnect()
    {
        if (boost::shared_ptr<IDeviceServiceEvents> cb = m_callbacks.lock())
        {
            // just call callback method later...
            getHttpClient()->getIoService().post(
                boost::bind(&IDeviceServiceEvents::onConnected,
                    cb, boost::system::error_code()));
        }
        else
            assert(!"callback is dead or not initialized");
    }


    /// @copydoc IDeviceService::asyncGetServerInfo()
    virtual void asyncGetServerInfo()
    {
        if (boost::shared_ptr<IDeviceServiceEvents> cb = m_callbacks.lock())
        {
            Base::asyncGetServerInfo(
                boost::bind(&IDeviceServiceEvents::onServerInfo,
                    cb, _1, _2));
        }
        else
            assert(!"callback is dead or not initialized");
    }

public:

    /// @copydoc IDeviceService::asyncRegisterDevice()
    virtual void asyncRegisterDevice(DevicePtr device)
    {
        if (boost::shared_ptr<IDeviceServiceEvents> cb = m_callbacks.lock())
        {
            Base::asyncRegisterDevice(device,
                boost::bind(&IDeviceServiceEvents::onRegisterDevice,
                    cb, _1, _2));
        }
        else
            assert(!" callback is dead or not initialized");
    }


    /// @copydoc IDeviceService::asyncUpdateDeviceData()
    virtual void asyncUpdateDeviceData(DevicePtr device)
    {
        if (boost::shared_ptr<IDeviceServiceEvents> cb = m_callbacks.lock())
        {
            Base::asyncUpdateDeviceData(device,
                boost::bind(&IDeviceServiceEvents::onUpdateDeviceData,
                    cb, _1, _2));
        }
        else
            assert(!"callback is dead or not initialized");
    }

public:

    /// @copydoc IDeviceService::asyncSubscribeForCommands()
    virtual void asyncSubscribeForCommands(DevicePtr device, String const& timestamp)
    {
        if (m_devices.find(device) == m_devices.end())
        {
            DeviceData &dd = m_devices[device];
            dd.lastCommandTimestamp = timestamp;
            dd.pollTask = Base::asyncPollCommands(device, dd.lastCommandTimestamp,
                boost::bind(&This::onPollCommands, shared_from_this(), _1, _2, _3));
        }
        // else // already subscribed, do nothing (TODO: maybe update timestamp?)
    }


    /// @copydoc IDeviceService::asyncUnsubscribeFromCommands()
    virtual void asyncUnsubscribeFromCommands(DevicePtr device)
    {
        std::map<DevicePtr, DeviceData>::iterator it = m_devices.find(device);
        if (it != m_devices.end())
        {
            DeviceData &dd = it->second;
            if (dd.pollTask)
                dd.pollTask->cancel();
            m_devices.erase(it);
        }
        // else // not subscribed, do nothing
    }

private:

    /// @brief The "poll commands" callback.
    /**
    @param[in] err The error code.
    @param[in] device The device.
    @param[in] commands The list of commands.
    */
    virtual void onPollCommands(boost::system::error_code err, DevicePtr device, std::vector<CommandPtr> commands)
    {
        if (boost::shared_ptr<IDeviceServiceEvents> cb = m_callbacks.lock())
        {
            if (!err)
            {
                DeviceData &dd = m_devices[device];

                for (size_t i = 0; i < commands.size(); ++i)
                {
                    cb->onInsertCommand(err, device, commands[i]);
                    dd.lastCommandTimestamp = commands[i]->timestamp;
                }

                // start polling again
                dd.pollTask = Base::asyncPollCommands(device, dd.lastCommandTimestamp,
                    boost::bind(&This::onPollCommands, shared_from_this(), _1, _2, _3));
            }
            else
                cb->onInsertCommand(err, device, CommandPtr());
        }
        else
            assert(!"callback is dead or not initialized");
    }

public:

    /// @copydoc IDeviceService::asyncUpdateCommand()
    virtual void asyncUpdateCommand(DevicePtr device, CommandPtr command)
    {
        if (boost::shared_ptr<IDeviceServiceEvents> cb = m_callbacks.lock())
        {
            Base::asyncUpdateCommand(device, command,
                boost::bind(&IDeviceServiceEvents::onUpdateCommand,
                    cb, _1, _2, _3));
        }
        else
            assert(!" callback is dead or not initialized");
    }

public:

    /// @copydoc IDeviceService::asyncInsertNotification()
    virtual void asyncInsertNotification(DevicePtr device, NotificationPtr notification)
    {
        if (boost::shared_ptr<IDeviceServiceEvents> cb = m_callbacks.lock())
        {
            Base::asyncInsertNotification(device, notification,
                boost::bind(&IDeviceServiceEvents::onInsertNotification,
                    cb, _1, _2, _3));
        }
        else
            assert(!" callback is dead or not initialized");
    }

private:
    boost::weak_ptr<IDeviceServiceEvents> m_callbacks;

private:

    /// @brief Device related data.
    struct DeviceData
    {
        http::Client::TaskPtr pollTask;
        String lastCommandTimestamp;
    };

    std::map<DevicePtr, DeviceData> m_devices;
};

} // devicehive namespace

#endif // __DEVICEHIVE_RESTFUL_HPP_
