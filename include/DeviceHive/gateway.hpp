/** @file
@brief The DeviceHive gateway prototype (experimental).
@author Dmitry Kinev <dmitry.kinev@dataart.com>
@author Sergey Polichnoy <sergey.polichnoy@dataart.com>
*/
#ifndef __DEVICEHIVE_GATEWAY_HPP_
#define __DEVICEHIVE_GATEWAY_HPP_

#include <hive/json.hpp>
#include <hive/bin.hpp>

#if !defined(HIVE_PCH)
#   include <boost/shared_ptr.hpp>
#   include <boost/asio.hpp>
#   include <boost/bind.hpp>
#   include <vector>
#endif // HIVE_PCH

#include <boost/uuid/uuid_io.hpp>


/// @brief The DeviceHive gateway prototype (experimental).
namespace gateway
{
    using namespace hive;


/// @brief The message intents.
/**
Intent numbers under 256 are reserved for future use by the system.

Intent numbers 256 and above are reserved for user usage.
*/
enum MessageIntent
{
    INTENT_REGISTRATION_REQUEST,  ///< @brief The registration request.
    INTENT_REGISTRATION_RESPONSE, ///< @brief The registration response.
    INTENT_COMMAND_RESULT_RESPONSE, ///< @brief The command result response.
    INTENT_REGISTRATION2_RESPONSE, ///< @brief The registration response (JSON format).

    INTENT_USER = 256             ///< @brief The minimum user intent.
};


/// @brief The binary data types.
enum DataType
{
    // fixed size types
    DT_NULL,    ///< @brief The NULL type, zero length.
    DT_UINT8,   ///< @brief The unsigned byte, 1 byte.
    DT_UINT16,  ///< @brief The unsigned word, 2 bytes, little-endian.
    DT_UINT32,  ///< @brief The unsigned double word, 4 bytes, little-endian.
    DT_UINT64,  ///< @brief The unsigned quad word, 8 bytes, little-endian.
    DT_INT8,    ///< @brief The signed byte, 1 byte.
    DT_INT16,   ///< @brief The signed word, 2 bytes, little-endian.
    DT_INT32,   ///< @brief The signed double word, 4 bytes, little-endian.
    DT_INT64,   ///< @brief The signed quad word, 8 bytes, little-endian.
    DT_SINGLE,  ///< @brief The float, 4 bytes.
    DT_DOUBLE,  ///< @brief The double, 8 bytes.
    DT_BOOL,    ///< @brief The boolean, 1 byte.
    DT_UUID,    ///< @brief The UUID, 16 bytes, little-endian.

    // variable size types
    DT_STRING,  ///< @brief The UTF string, variable length.
    DT_BINARY,  ///< @brief The binary data, variable length.
    DT_ARRAY,   ///< @brief The array data, variable length.
    DT_OBJECT   ///< @brief The object data, variable length.
};


/// @brief The message layout.
/**
Defines relation between JSON data model and the binary data model, i.e.
map the parameter names to the message intents and visa versa.
*/
class Layout
{
public:

    /// @brief The layout element.
    class Element
    {
    public:
        const String name; ///< @brief The element name.
        const DataType dataType; ///< @brief The element data type.

        /// @brief The sublayout for the complex data types.
        /**
        Should be non NULL for array and object.
        No sense for other regular types.
        */
        const boost::shared_ptr<Layout> sublayout;

    protected:

        /// @brief The main constructor.
        /**
        @param[in] name_ The element name.
        @param[in] dataType_ The element data type.
        @param[in] sublayout_ The sublayout for complex data types.
        */
        Element(String const& name_, DataType dataType_, boost::shared_ptr<Layout> sublayout_)
            : name(name_), dataType(dataType_), sublayout(sublayout_)
        {}

    public:

        /// @brief The trivial destructor.
        virtual ~Element()
        {}

    public:

        /// @brief The shared pointer type.
        typedef boost::shared_ptr<Element> SharedPtr;


        /// @brief The factory method.
        /**
        @param[in] name The element name.
        @param[in] dataType The element data type.
        @param[in] sublayout The sublayout for complex data types.
        @return The new layout element.
        */
        static SharedPtr create(String const& name, DataType dataType, boost::shared_ptr<Layout> sublayout = boost::shared_ptr<Layout>())
        {
            return SharedPtr(new Element(name, dataType, sublayout));
        }
    };

protected:

    /// @brief The default constructor.
    Layout()
    {}

public:

    /// @brief The shared pointer type.
    typedef boost::shared_ptr<Layout> SharedPtr;


    /// @brief The factory method.
    /**
    @return The new empty layout.
    */
    static SharedPtr create()
    {
        return SharedPtr(new Layout());
    }


    /// @brief Create "Registration Request" layout.
    /**
    @return The new "Registration Request" layout.
    */
    static SharedPtr createRegistrationRequest()
    {
        SharedPtr layout = create();
        layout->add("data", DT_NULL);
        return layout;
    }


    /// @brief Create "Registration Response" layout.
    /**
    @return The new "Registration Response" layout.
    */
    static SharedPtr createRegistrationResponse()
    {
        SharedPtr deviceClass = create();
        deviceClass->add("name", DT_STRING);
        deviceClass->add("version", DT_STRING);

        SharedPtr equipment = create();
        equipment->add("name", DT_STRING);
        equipment->add("code", DT_STRING);
        equipment->add("type", DT_STRING);

        SharedPtr parameter = create();
        parameter->add("type", DT_UINT8);
        parameter->add("name", DT_STRING);

        SharedPtr commandElem = create();
        commandElem->add("intent", DT_UINT16);
        commandElem->add("name", DT_STRING);
        commandElem->add("params", DT_ARRAY, parameter);

        SharedPtr layout = create();
        layout->add("id", DT_UUID);
        layout->add("key", DT_STRING);
        layout->add("name", DT_STRING);
        layout->add("deviceClass", DT_OBJECT, deviceClass);
        layout->add("equipment", DT_ARRAY, equipment);
        layout->add("notifications", DT_ARRAY, commandElem);
        layout->add("commands", DT_ARRAY, commandElem);
        return layout;
    }


    /// @brief Create "Registration 2 Response" layout.
    /**
    @return The new "Registration 2 Response" layout.
    */
    static SharedPtr createRegistration2Response()
    {
        SharedPtr layout = create();
        layout->add("json", DT_STRING);
        return layout;
    }


    /// @brief Create "Command Result Response" layout.
    /**
    @return The new "Command Result Response" layout.
    */
    static SharedPtr createCommandResultResponse()
    {
        SharedPtr layout = create();
        layout->add("id", DT_UINT32);
        layout->add("status", DT_STRING);
        layout->add("result", DT_STRING);
        return layout;
    }

public:

    /// @brief Add the new element to the layout.
    /**
    @param[in] name The element name.
    @param[in] dataType The element data type.
    @param[in] sublayout The sublayout for complex data types.
    */
    void add(String const& name, DataType dataType, SharedPtr sublayout = SharedPtr())
    {
        add(Element::create(name, dataType, sublayout));
    }


    /// @brief Add element to the layout.
    /**
    @param[in] elem The layout element.
    */
    void add(Element::SharedPtr elem)
    {
        if (elem)
        {
            assert(!find(elem->name) && "layout element already exists");
            m_content.push_back(elem);
        }
    }


    /// @brief Find the layout element by name.
    /**
    @param[in] name The element name.
    @return The layout element or NULL.
    */
    Element::SharedPtr find(String const& name) const
    {
        const size_t N = m_content.size();
        for (size_t i = 0; i < N; ++i)
            if (m_content[i]->name == name)
                return m_content[i];

        return Element::SharedPtr(); // not found
    }

public:

    /// @brief The layout content.
    typedef std::vector<Element::SharedPtr> Content;

    /// @brief The layout element iterator.
    typedef Content::const_iterator ElementIterator;


    /// @brief Get the begin of layout elements.
    /**
    @return The begin of layout elements.
    */
    ElementIterator elementsBegin() const
    {
        return m_content.begin();
    }


    /// @brief Get the end of layout elements.
    /**
    @return The end of layout elements.
    */
    ElementIterator elementsEnd() const
    {
        return m_content.end();
    }

private:
    Content m_content; ///< @brief The conent.
};


/// @brief The layout manager.
/**
Maps the intents to the layouts.
*/
class LayoutManager
{
public:

    /// @brief The default constructor.
    /**
    It creates and registers system intents.
    */
    LayoutManager()
    {
        registerSystemIntent(INTENT_REGISTRATION_REQUEST, Layout::createRegistrationRequest());
        registerSystemIntent(INTENT_REGISTRATION_RESPONSE, Layout::createRegistrationResponse());
        registerSystemIntent(INTENT_REGISTRATION2_RESPONSE, Layout::createRegistration2Response());
        registerSystemIntent(INTENT_COMMAND_RESULT_RESPONSE, Layout::createCommandResultResponse());
    }

protected:

    /// @brief Register system intent.
    /**
    @param[in] intent The message intent.
    @param[in] layout The message layout.
    */
    void registerSystemIntent(int intent, Layout::SharedPtr layout)
    {
        m_container[intent] = layout;
    }

public:

    /// @brief Register user intent.
    /**
    @param[in] intent The message intent.
    @param[in] layout The message layout.
    */
    void registerIntent(int intent, Layout::SharedPtr layout)
    {
        assert(INTENT_USER <= intent && "invalid user intent");
        m_container[intent] = layout;
    }


    /// @brief Unregister user intent.
    /**
    @param[in] intent The message intent.
    */
    void unregisterIntent(int intent)
    {
        assert(INTENT_USER <= intent && "invalid user intent");
        if (INTENT_USER <= intent)
            m_container.erase(intent);
    }

public:

    /// @brief Find the message layout.
    /**
    @param[in] intent The message intent.
    @return The message laout or NULL.
    */
    Layout::SharedPtr find(int intent) const
    {
        Container::const_iterator i = m_container.find(intent);
        return (i != m_container.end()) ? i->second : Layout::SharedPtr();
    }

private:

    /// @brief The container type.
    typedef std::map<int, Layout::SharedPtr> Container;

    Container m_container; ///< @brief The intents.
};


/// @brief The binary frame.
/**
This class contains full formated frame:

|     field | size in bytes
|-----------|--------------
| signature | 2
|   version | 1
|     flags | 1
|    length | 2
|    intent | 2
|   payload | N
|  checksum | 1

The empty frame doesn't contain any data.
*/
class Frame:
    public bin::FrameContent
{
public:

    /// @brief Constants.
    enum Const
    {
        SIGNATURE1  = 0xC5, ///< @brief The signature (first byte).
        SIGNATURE2  = 0xC3, ///< @brief The signature (second byte).
        VERSION     = 0x01  ///< @brief The protocol version.
    };

private:

    /// @brief Various lengths.
    enum Length
    {
        SIGNATURE_LEN   = 2, ///< @brief The signature field length in bytes.
        VERSION_LEN     = 1, ///< @brief The version field length in bytes.
        FLAGS_LEN       = 1,   ///< @brief The flags field length in bytes.

        INTENT_LEN      = 2, ///< @brief The intent field length in bytes.
        LENGTH_LEN      = 2, ///< @brief The length field length in bytes.
        CHECKSUM_LEN    = 1, ///< @brief The checksum field length in bytes.

        /// @brief The total header length in bytes.
        HEADER_LEN      = SIGNATURE_LEN + VERSION_LEN + FLAGS_LEN
                        + INTENT_LEN + LENGTH_LEN,

        /// @brief The total footer length in bytes.
        FOOTER_LEN      = CHECKSUM_LEN
    };

private:

    /// @brief The default constructor.
    /**
    Constructs the empty frame.
    */
    Frame()
    {}

public:

    /// @brief The shared pointer type.
    typedef boost::shared_ptr<Frame> SharedPtr;


    /// @brief Construct the frame from the payload.
    /**
    @param[in] intent The message intent.
    @param[in] payload The frame data payload.
    @return The new frame instance.
    */
    static SharedPtr create(int intent, String const& payload)
    {
        SharedPtr pthis(new Frame());
        pthis->init(intent, payload);
        return pthis;
    }

public:

    /// @brief Parse the payload from the frame.
    /**
    @param[out] payload The frame data payload to parse.
    @return `true` if data payload successfully parsed.
    */
    bool getPayload(String & payload) const
    {
        if (HEADER_LEN+FOOTER_LEN <= m_content.size())
        {
            payload.assign(
                m_content.begin()+HEADER_LEN, // skip signature+intent+length
                m_content.end()-FOOTER_LEN);  // skip checksum
            return true;
        }

        return false; // empty
    }

public:

    ///@brief The frame parse result.
    enum ParseResult
    {
        RESULT_SUCCESS,      ///< @brief Frame succcessfully parsed.
        RESULT_INCOMPLETE,   ///< @brief Need more data.
        RESULT_BADCHECKSUM,  ///< @brief Bad checksum found.
        RESULT_BADSIGNATURE, ///< @brief Bad signature.
        RESULT_BADVERSION    ///< @brief Bad protocol version.
    };


    /// @brief Assign the frame content.
    /**
    This method searches frame signature, veryfies the checksum
    and assigns the frame content.

    @param[in] first The begin of input data.
    @param[in] last The end of input data.
    @param[out] n_skip The number of bytes processed.
    @param[in,out] result The parse result. May be NULL.
    @return Parsed frame or NULL.
    */
    template<typename In>
    static SharedPtr parseFrame(In first, In last, size_t &n_skip, ParseResult *result)
    {
        ParseResult res = RESULT_INCOMPLETE;
        SharedPtr frame; // empty frame

        // search for signature
        for (; first != last; ++first, ++n_skip)
        {
            if (UInt8(*first) == SIGNATURE1)
                break;
        }

        const size_t buf_len = std::distance(first, last);
        if (HEADER_LEN+FOOTER_LEN <= buf_len)
        {
            const In s_first = first;

            ++first; // skip signature1
            if (UInt8(*first++) == SIGNATURE2) // check signature2
            {
                if (UInt8(*first++) == VERSION) // check version
                {
                    ++first; // skip flags
                    const int len_lsb = UInt8(*first++);
                    const int len_msb = UInt8(*first++);
                    const size_t len = (len_msb<<8) | len_lsb;
                    ++first; ++first; // skip intent

                    if (size_t(HEADER_LEN+len+FOOTER_LEN) <= buf_len)
                    {
                        if (checksum(s_first, s_first+(HEADER_LEN+len+FOOTER_LEN)) != 0x00)
                        {
                            // bad checksum
                            res = RESULT_BADCHECKSUM;
                            n_skip += 1; // skip signature (first byte)
                        }
                        else
                        {
                            // assign the frame content
                            frame.reset(new Frame());
                            frame->m_content.assign(s_first,
                                s_first+(HEADER_LEN+len+FOOTER_LEN));
                            n_skip += HEADER_LEN+len+FOOTER_LEN;
                            res = RESULT_SUCCESS;
                        }
                    }
                    // else incomplete
                }
                else
                {
                    // unsupported version
                    res = RESULT_BADVERSION;
                    n_skip += 1; // skip signature (first byte)
                }
            }
            else
            {
                // bad signature
                res = RESULT_BADSIGNATURE;
                n_skip += 1; // skip signature (first byte)
            }
        }
        // else incomplete

        if (result)
            *result = res;
        return frame;
    }


    /// @brief Assign the frame content from stream buffer.
    /**
    This method searches frame signature, veryfies the checksum
    and assigns the frame content.

    @param[in,out] sb The input data buffer.
    @param[in,out] result The parse result. May be NULL.
    @return Parsed frame or NULL.
    */
    static SharedPtr parseFrame(boost::asio::streambuf & sb, ParseResult *result)
    {
        const boost::asio::streambuf::const_buffers_type bufs = sb.data();
        size_t n_skip = 0; // number of bytes to skip
        SharedPtr frame = parseFrame(boost::asio::buffers_begin(bufs),
            boost::asio::buffers_end(bufs), n_skip, result);
        sb.consume(n_skip);
        return frame;
    }

public:

    /// @brief Get the frame intent.
    /**
    @return The frame intent or `-1` if it's unknown.
    */
    int getIntent() const
    {
        if (HEADER_LEN+0+FOOTER_LEN <= m_content.size())
        {
            size_t offset = SIGNATURE_LEN+VERSION_LEN+FLAGS_LEN+LENGTH_LEN;

            const int id_lsb = m_content[offset++];
            const int id_msb = m_content[offset++];

            return (id_msb<<8) | id_lsb;
        }

        return -1; // unknown
    }

private:

    /// @brief Calculate the checksum byte.
    /**
    @param[in] first The begin of frame data payload.
    @param[in] last The end of frame data payload.
    @return The checksum byte.
    */
    template<typename In>
    static UInt8 checksum(In first, In last)
    {
        int cs = 0;
        for (; first != last; ++first)
            cs += UInt8(*first);
        return UInt8(0xFF - (cs&0xFF));
    }


    /// @brief Initialize frame content from payload data.
    /**
    @param[in] intent The payload intent.
    @param[in] payload The frame payload data.
    */
    void init(int intent, String const& payload)
    {
        const size_t length = payload.size();
        assert(length < 64*1024 && "frame data payload too big");

        m_content.reserve(HEADER_LEN+length+FOOTER_LEN);

        // header (signature+intent+length)
        m_content.push_back(SIGNATURE1);
        m_content.push_back(SIGNATURE2);
        m_content.push_back(VERSION);
        m_content.push_back(0); // flags
        m_content.push_back((length)&0xFF);    // length, LSB
        m_content.push_back((length>>8)&0xFF); // length, MSB
        m_content.push_back((intent)&0xFF);    // intent, LSB
        m_content.push_back((intent>>8)&0xFF); // intent, MSB

        // payload
        m_content.insert(m_content.end(),
            payload.begin(), payload.end());

        // checksum
        m_content.push_back(checksum(
            m_content.begin(),
            m_content.end()));
    }
};


/// @brief The gateway engine.
/**
Stores the commands and notifications.
*/
class Engine
{
public:

    /// @brief Find the command intent by name.
    /**
    @param[in] name The command name.
    @return The command intent or `-1` if not found.
    */
    int findCommandIntentByName(String const& name) const
    {
        std::map<String, int>::const_iterator i = m_commands.find(name);
        return (i != m_commands.end()) ? i->second : -1;
    }


    /// @brief Find the notification name by intent.
    /**
    @param[in] intent The notification intent.
    @return The notification name or empty string if not found.
    */
    String findNotificationNameByIntent(int intent) const
    {
        std::map<int, String>::const_iterator i = m_notifications.find(intent);
        return (i != m_notifications.end()) ? i->second : String();
    }


    /// @brief Convert JSON payload into binary frame.
    /**
    @param[in] intent The message intent.
    @param[in] data The JSON frame payload.
    @return The binary frame. May be NULL for unknown intents.
    */
    Frame::SharedPtr jsonToFrame(int intent, json::Value const& data) const
    {
        if (Layout::SharedPtr layout = m_layouts.find(intent))
        {
            OStringStream payload;
            bin::OStream bs(payload);
            Serializer::json2bin(data,
                bs, layout);

            return Frame::create(intent, payload.str());
        }
        //else HIVELOG_WARN(m_log, "unknown layout for intent #" << intent);

        return Frame::SharedPtr(); // no conversion
    }


    /// @brief Convert binary frame into JSON payload.
    /**
    @param[in] frame The frame to convert.
    @return The JSON frame payload. May be empty for unknown intents.
    */
    json::Value frameToJson(Frame::SharedPtr frame) const
    {
        if (Layout::SharedPtr layout = m_layouts.find(frame->getIntent()))
        {
            String payload;
            if (frame->getPayload(payload))
            {
                IStringStream is(payload);
                bin::IStream bs(is);

                return Serializer::bin2json(bs, layout);
            }
            //else HIVELOG_WARN(m_log, "unable to get frame payload, intent #" << frame->getIntent());
        }
        //else HIVELOG_WARN(m_log, "unknown layout for intent #" << frame->getIntent());

        return json::Value(); // no conversion
    }

public:

    /// @brief Handle the basic part register response message.
    /**
    This method updates the known layouts based on information from registration response message.
    @param[in] jval The frame payload data.
    */
    void handleRegisterResponse(json::Value const& jval)
    {
        { // update commands
            json::Value const& commands = jval["commands"];
            const size_t N = commands.size();
            for (size_t i = 0; i < N; ++i)
            {
                json::Value const& command = commands[i];

                Layout::SharedPtr layout = gateway::Layout::create();
                layout->add("id", gateway::DT_UINT32);

                int intent = command["intent"].asUInt16();
                String name = command["name"].asString();

                if (Layout::SharedPtr params = parseCommandParams(command["params"]))
                    layout->add("parameters", gateway::DT_OBJECT, params);

                m_layouts.registerIntent(intent, layout);
                m_commands[name] = intent;
            }
        }

        { // update notifications
            json::Value const& notifications = jval["notifications"];
            const size_t N = notifications.size();
            for (size_t i = 0; i < N; ++i)
            {
                json::Value const& notification = notifications[i];

                int intent = notification["intent"].asUInt16();
                String name = notification["name"].asString();

                if (Layout::SharedPtr layout = parseCommandParams(notification["params"]))
                {
                    m_layouts.registerIntent(intent, layout);
                    m_notifications[intent] = name;
                }
            }
        }
    }


    /// @brief Parse the command or notification parameters.
    /**
    @param[in] jval The JSON value related to parameter.
    @return The layout related to parameter. May be NULL for empty parameter.
    */
    static Layout::SharedPtr parseCommandParams(json::Value const& jval)
    {
        const size_t N = jval.size();
        if (!N) return Layout::SharedPtr();

        Layout::SharedPtr layout = gateway::Layout::create();
        for (size_t i = 0; i < N; ++i)
        {
            json::Value const& param = jval[i];

            String name = param["name"].asString();
            int type = param["type"].asUInt8();

            // TODO: check type range
            layout->add(name, DataType(type));
        }

        return layout;
    }


    /// @brief Handle the basic part register 2 response message.
    /**
    This method updates the known layouts based on information from registration 2 response message.
    @param[in] jval The frame payload data.
    */
    void handleRegister2Response(json::Value const& jval)
    {
        { // update commands
            json::Value const& commands = jval["commands"];
            const size_t N = commands.size();
            for (size_t i = 0; i < N; ++i)
            {
                json::Value const& command = commands[i];

                Layout::SharedPtr layout = gateway::Layout::create();
                layout->add("id", gateway::DT_UINT32);
                layout->add(parseCommandParamsField("parameters", command["params"]));

                int intent = command["intent"].asUInt16();
                String name = command["name"].asString();

                m_layouts.registerIntent(intent, layout);
                m_commands[name] = intent;
            }
        }

        { // update notifications
            json::Value const& notifications = jval["notifications"];
            const size_t N = notifications.size();
            for (size_t i = 0; i < N; ++i)
            {
                json::Value const& notification = notifications[i];

                int intent = notification["intent"].asUInt16();
                String name = notification["name"].asString();

                Layout::SharedPtr layout = gateway::Layout::create();
                layout->add(parseCommandParamsField("parameters", notification["params"]));

                m_layouts.registerIntent(intent, layout);
                m_notifications[intent] = name;
            }
        }
    }


    /// @brief Parse the command or notification parameters (Struct in JSON format).
    /**
    @param[in] jval The JSON value related to structure (array of fields).
    @return The layout related to parameter. May be NULL for empty parameter.
    */
    static Layout::SharedPtr parseCommandParamsStruct(json::Value const& jval)
    {
        if (!jval.isNull() && !jval.isObject())
            throw std::runtime_error("invalid structure description");

        if (jval.empty())
            return Layout::SharedPtr();

        Layout::SharedPtr layout = gateway::Layout::create();
        json::Value::MemberIterator i = jval.membersBegin();
        const json::Value::MemberIterator e = jval.membersEnd();
        for (; i != e; ++i)
        {
            String const& name = i->first;
            json::Value const& field = i->second;

            layout->add(parseCommandParamsField(name, field));
        }

        return layout;
    }


    /// @brief Parse the command or notification parameters (Field in JSON format).
    /**
    @param[in] name The element name.
    @param[in] jval The JSON value related to field.
    @return The layout element.
    */
    static Layout::Element::SharedPtr parseCommandParamsField(String const& name, json::Value const& jval)
    {
        if (jval.isNull()) // NULL as primitive type
            return Layout::Element::create(name, DT_NULL);
        else if (jval.isString()) // a primitive type
        {
            return Layout::Element::create(name,
                parsePrimitiveDataType(jval.asString()));
        }
        else if (jval.isObject()) // object
        {
            return Layout::Element::create(name, DT_OBJECT,
                parseCommandParamsStruct(jval));
        }
        else if (jval.isArray()) // array
        {
            if (jval.size() != 1)
                throw std::runtime_error("invalid array field [1 element expected]");

            Layout::SharedPtr layout = Layout::create();
            layout->add(parseCommandParamsField("", jval[0]));

            return Layout::Element::create(name, DT_ARRAY, layout);
        }
        else
            throw std::runtime_error("unknown field type");
    }


    /// @brief Parse primitive data type.
    /**
    @param[in] type The primitive data type string.
    @return The parsed data type.
    @throw std::runtime_error if type is unknown.
    */
    static DataType parsePrimitiveDataType(String const& type)
    {
        if (boost::iequals(type, "bool"))
            return DT_BOOL;
        else if (boost::iequals(type, "u8") || boost::iequals(type, "uint8"))
            return DT_UINT8;
        else if (boost::iequals(type, "i8") || boost::iequals(type, "int8"))
            return DT_INT8;
        else if (boost::iequals(type, "u16") || boost::iequals(type, "uint16"))
            return DT_UINT16;
        else if (boost::iequals(type, "i16") || boost::iequals(type, "int16"))
            return DT_INT16;
        else if (boost::iequals(type, "u32") || boost::iequals(type, "uint32"))
            return DT_UINT32;
        else if (boost::iequals(type, "i32") || boost::iequals(type, "int32"))
            return DT_INT32;
        else if (boost::iequals(type, "u64") || boost::iequals(type, "uint64"))
            return DT_UINT64;
        else if (boost::iequals(type, "i64") || boost::iequals(type, "int64"))
            return DT_INT64;
        else if (boost::iequals(type, "f") || boost::iequals(type, "single"))
            return DT_SINGLE;
        else if (boost::iequals(type, "ff") || boost::iequals(type, "double"))
            return DT_DOUBLE;
        else if (boost::iequals(type, "uuid") || boost::iequals(type, "guid"))
            return DT_UUID;
        else if (boost::iequals(type, "s") || boost::iequals(type, "str") || boost::iequals(type, "string"))
            return DT_STRING;
        else if (boost::iequals(type, "b") || boost::iequals(type, "bin") || boost::iequals(type, "binary"))
            return DT_BINARY;
        else
            throw std::runtime_error("unknown primitive type");
    }

public:

    /// @brief The binary serializer.
    /**
    Converts JSON values to binary and visa versa.
    */
    class Serializer
    {
    public:

        /// @brief Convert binary data to JSON value.
        /**
        @param[in,out] bs The binary input stream.
        @param[in] layout The layout.
        @return The JSON value.
        */
        static json::Value bin2json(bin::IStream & bs, Layout::SharedPtr layout)
        {
            json::Value jval;

            Layout::ElementIterator i = layout->elementsBegin();
            Layout::ElementIterator e = layout->elementsEnd();
            for (; i != e; ++i)
            {
                const Layout::Element::SharedPtr elem = *i;

                if (!elem->name.empty())
                    jval[elem->name] = bin2json(bs, elem);
                else
                {
                    assert((i+1) == e && "one element expected");
                    jval = bin2json(bs, elem);
                }
            }

            return jval;
        }


        /// @brief Convert binary data to JSON value.
        /**
        @param[in,out] bs The binary input stream.
        @param[in] layoutElement The layout element.
        @return The JSON value.
        */
        static json::Value bin2json(bin::IStream & bs, Layout::Element::SharedPtr layoutElement)
        {
            switch (layoutElement->dataType)
            {
                case DT_NULL:   return json::Value();
                case DT_UINT8:  return json::Value(bs.getUInt8());
                case DT_UINT16: return json::Value(bs.getUInt16LE());
                case DT_UINT32: return json::Value(bs.getUInt32LE());
                case DT_UINT64: return json::Value(bs.getUInt64LE());
                case DT_INT8:   return json::Value(bs.getInt8());
                case DT_INT16:  return json::Value(bs.getInt16LE());
                case DT_INT32:  return json::Value(bs.getInt32LE());
                case DT_INT64:  return json::Value(bs.getInt64LE());
                case DT_BOOL:   return json::Value(bs.getUInt8() != 0);

                case DT_SINGLE:
                {
                    float buf = 0.0f;
                    bs.getBuffer(&buf, sizeof(buf));
                    return json::Value(double(buf));
                } break;

                case DT_DOUBLE:
                {
                    double buf = 0.0;
                    bs.getBuffer(&buf, sizeof(buf));
                    return json::Value(buf);
                } break;

                case DT_UUID:
                {
                    boost::uuids::uuid buf;
                    bs.getBuffer(buf.data,
                        sizeof(buf.data));
                    return json::Value(to_string(buf));
                } break;

                case DT_STRING:
                case DT_BINARY:
                {
                    if (const UInt32 len = bs.getUInt16LE()) // read if non-empty
                    {
                        std::vector<UInt8> buf(len);
                        bs.getBuffer(&buf[0], len);
                        return json::Value(String(buf.begin(), buf.end()));
                    }
                    else
                        return json::Value("");
                } break;

                case DT_ARRAY:
                {
                    json::Value jarr(json::Value::TYPE_ARRAY);
                    const UInt32 N = bs.getUInt16LE();
                    for (size_t i = 0; i < N; ++i)
                        jarr.append(bin2json(bs, layoutElement->sublayout));
                    return jarr;
                } break;

                case DT_OBJECT:
                {
                    return bin2json(bs, layoutElement->sublayout);
                } break;
            }

            assert(!"unknown data type");
            return json::Value();
        }

    public:

        /// @brief Convert JSON value to binary data.
        /**
        @param[in] jval The JSON value.
        @param[in,out] bs The binary output stream.
        @param[in] layout The layout.
        */
        static void json2bin(json::Value const& jval, bin::OStream & bs, Layout::SharedPtr layout)
        {
            Layout::ElementIterator i = layout->elementsBegin();
            Layout::ElementIterator e = layout->elementsEnd();
            for (; i != e; ++i)
            {
                const Layout::Element::SharedPtr elem = *i;

                if (!elem->name.empty())
                    json2bin(jval[elem->name], bs, elem);
                else
                {
                    assert((i+1) == e && "one element expected");
                    json2bin(jval, bs, elem);
                }
            }
        }


        /// @brief Convert JSON value to binary data.
        /**
        @param[in] jval The JSON value.
        @param[in,out] bs The binary output stream.
        @param[in] layoutElement The layout element.
        */
        static void json2bin(json::Value const& jval, bin::OStream & bs, Layout::Element::SharedPtr layoutElement)
        {
            // TODO: more checks on data types!!!
            switch (layoutElement->dataType)
            {
                case DT_NULL:   break;
                case DT_UINT8:  bs.putUInt8(jval.asUInt8()); break;
                case DT_UINT16: bs.putUInt16LE(jval.asUInt16()); break;
                case DT_UINT32: bs.putUInt32LE(jval.asUInt32()); break;
                case DT_UINT64: bs.putUInt64LE(jval.asUInt64()); break;
                case DT_INT8:   bs.putInt8(jval.asInt8()); break;
                case DT_INT16:  bs.putInt16LE(jval.asInt16()); break;
                case DT_INT32:  bs.putInt32LE(jval.asInt32()); break;
                case DT_INT64:  bs.putInt64LE(jval.asInt64()); break;
                case DT_BOOL:   bs.putUInt8(jval.asBool()); break;

                case DT_SINGLE:
                {
                    const float buf = float(jval.asDouble());
                    bs.putBuffer(&buf, sizeof(buf));
                } break;

                case DT_DOUBLE:
                {
                    const double buf = jval.asDouble();
                    bs.putBuffer(&buf, sizeof(buf));
                } break;

                case DT_UUID:
                {
                    String str = jval.asString();
                    IStringStream iss(str);
                    boost::uuids::uuid buf;
                    iss >> buf;
                    bs.putBuffer(buf.data,
                        sizeof(buf.data));
                } break;

                case DT_STRING:
                case DT_BINARY:
                {
                    String buf = jval.asString();
                    bs.putUInt16LE(buf.size());
                    bs.putBuffer(buf.data(), buf.size());
                } break;

                case DT_ARRAY:
                {
                    if (!jval.isArray())
                    {
                        OStringStream ess;
                        ess << "\"" << layoutElement->name << "\" is not an array";
                        throw std::runtime_error(ess.str().c_str());
                    }

                    const size_t N = jval.size();
                    bs.putUInt16LE(N);
                    for (size_t i = 0; i < N; ++i)
                        json2bin(jval[i], bs, layoutElement->sublayout);
                } break;

                case DT_OBJECT:
                {
                    json2bin(jval, bs, layoutElement->sublayout);
                } break;

                default:
                    assert(!"unknown data type");
            }
        }
    };

protected:
    LayoutManager m_layouts; ///< @brief The registered intents and its layouts.
    std::map<String, int> m_commands; ///< @brief The registered commands.
    std::map<int, String> m_notifications; ///< @brief The registered notifications.
};


class Debug
{
public:

    /// @brief Dump the layout.
    static void dump(Layout::SharedPtr layout, hive::OStream &os, size_t indent = 0)
    {
        if (!layout)
            return;

        for (Layout::ElementIterator i = layout->elementsBegin(); i != layout->elementsEnd(); ++i)
        {
            dump(*i, os, indent);
            os << "\n";
        }
    }


    /// @brief Dump the layout element.
    static void dump(Layout::Element::SharedPtr elem, hive::OStream &os, size_t indent = 0)
    {
        if (!elem)
            return;

        json::Formatter::writeIndent(os, indent);

        if (!elem->name.empty())
        {
            os << elem->name
                << ": ";
        }
        switch (elem->dataType)
        {
            case DT_NULL:   os << "NULL";   break;
            case DT_UINT8:  os << "UInt8";  break;
            case DT_UINT16: os << "UInt16"; break;
            case DT_UINT32: os << "UInt32"; break;
            case DT_UINT64: os << "UInt64"; break;
            case DT_INT8:   os << "Int8";   break;
            case DT_INT16:  os << "Int16";  break;
            case DT_INT32:  os << "Int32";  break;
            case DT_INT64:  os << "Int64";  break;
            case DT_SINGLE: os << "Float";  break;
            case DT_DOUBLE: os << "Double"; break;
            case DT_BOOL:   os << "Bool";   break;
            case DT_UUID:   os << "UUID";   break;
            case DT_STRING: os << "String"; break;
            case DT_BINARY: os << "Binary"; break;

            case DT_ARRAY:
                os << "Array of {\n";
                dump(elem->sublayout, os, indent+1);
                json::Formatter::writeIndent(os, indent);
                os << "}";
                break;

            case DT_OBJECT:
                os << "Object {\n";
                dump(elem->sublayout, os, indent+1);
                json::Formatter::writeIndent(os, indent);
                os << "}";
                break;
        }
    }
};


/// @brief The gateway %API.
/**
Uses external stream object.
*/
template<typename StreamT>
class API:
    public bin::Transceiver<StreamT, Frame>
{
    /// @brief The base type.
    typedef bin::Transceiver<StreamT, Frame> Base;

    /// @brief The type alias.
    typedef API<StreamT> This;

private:

    /// @brief The default constructor.
    /**
    @param[in] stream The external stream.
    */
    explicit API(StreamT &stream)
        : Base("gateway/API", stream)
    {}

public:

    /// @brief The shared pointer type.
    typedef boost::shared_ptr<This> SharedPtr;


    /// @brief The factory method.
    /**
    @param[in] stream The external stream.
    @return The new instance.
    */
    static SharedPtr create(StreamT &stream)
    {
        return SharedPtr(new This(stream));
    }
};


/// @brief The Serial submodule.
/**
This is helper class.

You have to call initSerialModule() method before use any of the class methods.
The best place to do that is static factory method of your application.
*/
class SerialModule
{
    ///< @brief The this type alias.
    typedef SerialModule This;

protected:

    /// @brief The main constructor.
    /**
    @param[in] ios The IO service.
    @param[in] logger The logger.
    */
    SerialModule(boost::asio::io_service &ios, hive::log::Logger const& logger)
        : m_serialOpenTimer(ios)
        , m_serial(ios)
        , m_serialBaudrate(0)
        , m_log_(logger)
    {}


    /// @brief The trivial destructor.
    virtual ~SerialModule()
    {}


    /// @brief Initialize serial module.
    /**
    @param[in] portName The serial port name.
    @param[in] baudrate The serial baudrate.
    @param[in] pthis The this pointer.
    */
    void initSerialModule(String const& portName, UInt32 baudrate, boost::shared_ptr<SerialModule> pthis)
    {
        m_serialPortName = portName;
        m_serialBaudrate = baudrate;
        m_this = pthis;
    }


    /// @brief Cancel all serial tasks.
    void cancelSerialModule()
    {
        m_serialOpenTimer.cancel();
        m_serial.close();
    }

protected:

    /// @brief Try to open serial device asynchronously.
    /**
    @param[in] wait_sec The number of seconds to wait before open.
    */
    virtual void asyncOpenSerial(long wait_sec)
    {
        assert(!m_this.expired() && "Application is dead or not initialized");

        HIVELOG_TRACE(m_log_, "try to open serial after " << wait_sec << " seconds");
        m_serialOpenTimer.expires_from_now(boost::posix_time::seconds(wait_sec));
        m_serialOpenTimer.async_wait(boost::bind(&This::onTryToOpenSerial,
            m_this.lock(), boost::asio::placeholders::error));
    }


    /// @brief Try to open serial device.
    /**
    @return The error code.
    */
    virtual boost::system::error_code openSerial()
    {
        boost::asio::serial_port & port = m_serial;
        boost::system::error_code err;

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

        return err; // OK
    }


    /// @brief Try to open serial port device callback.
    /**
    This method is called as the "open serial timer" callback.

    @param[in] err The error code.
    */
    virtual void onTryToOpenSerial(boost::system::error_code err)
    {
        if (!err)
            onOpenSerial(openSerial());
        else if (err == boost::asio::error::operation_aborted)
            HIVELOG_DEBUG_STR(m_log_, "open serial device timer cancelled");
        else
        {
            HIVELOG_ERROR(m_log_, "open serial device timer error: ["
                << err << "] " << err.message());
        }
    }


    /// @brief The serial port is opened.
    /**
    @param[in] err The error code.
    */
    virtual void onOpenSerial(boost::system::error_code err)
    {
        if (!err)
        {
            HIVELOG_DEBUG(m_log_,
                "got serial device \"" << m_serialPortName
                << "\" at baudrate: " << m_serialBaudrate);
        }
        else
        {
            HIVELOG_DEBUG(m_log_, "cannot open serial device \""
                << m_serialPortName << "\": ["
                << err << "] " << err.message());
        }
    }


    /// @brief Reset the serial device.
    /**
    @brief tryToReopen if `true` then try to reopen serial as soon as possible.
    */
    virtual void resetSerial(bool tryToReopen)
    {
        HIVELOG_WARN(m_log_, "serial device reset");
        m_serial.close();

        if (tryToReopen)
            asyncOpenSerial(0); // ASAP
    }

protected:
    boost::asio::deadline_timer m_serialOpenTimer; ///< @brief Open the serial port device timer.
    boost::asio::serial_port m_serial; ///< @brief The serial port device.
    String m_serialPortName; ///< @brief The serial port name.
    UInt32 m_serialBaudrate; ///< @brief The serial baudrate.

private:
    boost::weak_ptr<SerialModule> m_this; ///< @brief The weak pointer to this.
    hive::log::Logger m_log_; ///< @brief The module logger.
};

} // gateway namespace

#endif // __DEVICEHIVE_GATEWAY_HPP_
