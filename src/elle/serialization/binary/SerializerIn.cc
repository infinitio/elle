#include <elle/serialization/binary/SerializerIn.hh>

#include <elle/meta.hh> // static_if
#include <elle/serialization/json/Error.hh>
#include <elle/time.hh>

ELLE_LOG_COMPONENT("elle.serialization.binary.SerializerIn")

namespace elle
{
  namespace serialization
  {
    namespace binary
    {
      SerializerIn::SerializerIn(std::istream& input,
                                 bool versioned)
        : Super(versioned)
        , _input(input)
      {
        this->_check_magic(input);
      }

      SerializerIn::SerializerIn(std::istream& input,
                                 Versions versions,
                                 bool versioned)
        : Super(std::move(versions), versioned)
        , _input(input)
      {
        this->_check_magic(input);
      }

      void
      SerializerIn::_check_magic(std::istream& input)
      {
        char magic;
        input.read(&magic, 1);
        if (input.gcount() != 1)
          err<Error>("unable to read magic");
        if (magic != 0)
          err<Error>("wrong magic for binary serialization: 0x%2x (expected 0)",
                     int(static_cast<unsigned char>(magic)));
      }

      bool
      SerializerIn::_text() const
      {
        return false;
      }

      void
      SerializerIn::_serialize(int64_t& v)
      {
        v = _serialize_number();
      }

      void
      SerializerIn::_serialize(uint64_t& v)
      {
        int64_t value;
        this->_serialize(value);
        v = static_cast<uint64_t>(value);
      }

      void
      SerializerIn::_serialize(ulong& v)
      {
        meta::static_if<need_unsigned_long>
          ([this](unsigned long& v)
           {
             uint64_t value;
             this->_serialize(value);
             using type = unsigned long;
             using limits = std::numeric_limits<type>;
             if (value > limits::max())
               throw json::Overflow(this->current_name(), sizeof(type) * 8, true, value);
             v = value;
           },
           [](auto& v)
           {
             unreachable();
           })
          (v);
      }

      void
      SerializerIn::_serialize(int32_t& v)
      {
        this->_serialize_int(v);
      }

      void
      SerializerIn::_serialize(uint32_t& v)
      {
        this->_serialize_int(v);
      }

      void
      SerializerIn::_serialize(int16_t& v)
      {
        this->_serialize_int(v);
      }

      void
      SerializerIn::_serialize(uint16_t& v)
      {
        this->_serialize_int(v);
      }

      void
      SerializerIn::_serialize(int8_t& v)
      {
        this->_serialize_int(v);
      }

      void
      SerializerIn::_serialize(uint8_t& v)
      {
        this->_serialize_int(v);
      }

      template <typename T>
      void
      SerializerIn::_serialize_int(T& v)
      {
        int64_t value;
        this->_serialize(value);
        using limits = std::numeric_limits<T>;
        if (value > limits::max())
          throw json::Overflow(this->current_name(), sizeof(T) * 8, true, value);
        if (value < limits::min())
          throw json::Overflow(this->current_name(), sizeof(T) * 8, false, value);
        v = value;
      }

      void
      SerializerIn::_serialize(double& v)
      {
        input().read((char*)(void*)&v, sizeof(double));
      }

      void
      SerializerIn::_serialize(bool& v)
      {
        int val;
        _serialize(val);
        if (val != 0 && val != 1)
          throw json::Overflow(this->current_name(), 1, true, val);
        v = val ? true : false;
      }

      void
      SerializerIn::_serialize_named_option(std::string const& name,
                                            bool,
                                            std::function<void ()> const& f)
      {
        f();
      }

      void
      SerializerIn::_serialize_option(bool,
                                      std::function<void ()> const& f)
      {
        bool filled;
        this->_serialize(filled);
        if (filled)
          f();
      }

      void
      SerializerIn::_serialize(std::string& v)
      {
        elle::Buffer b;
        this->_serialize(b);
        v = b.string();
      }

      void
      SerializerIn::_serialize(elle::Buffer& buffer)
      {
        int sz = _serialize_number();
        ELLE_DEBUG("%s: deserialize size: %s", *this, sz);
        buffer.size(sz);
        input().read((char*)buffer.mutable_contents(), sz);
        if (input().gcount() != sz)
          err<Error>("%s: short read when deserializing \"%s\":"
                     " expected %s, got %s",
                     *this, this->current_name(), sz, input().gcount());
      }

      void
      SerializerIn::_serialize(boost::posix_time::ptime& time)
      {
        std::string str;
        this->_serialize(str);
        try
        {
          time = to_posix_time(str);
        }
        catch (elle::Error const& e)
        {
          throw json::FieldError(this->current_name(), e.what());
        }
      }

      void
      SerializerIn::_serialize(Time& time)
      {
        auto str = this->deserialize<std::string>();
        auto&& in = std::istringstream{str};
        in >> date::parse("%Y-%m-%dT%H:%M:%S%z", time);
        // FIXME: check eof.
      }

      void
      SerializerIn::_serialize_time_duration(std::int64_t& ticks,
                                             std::int64_t& num,
                                             std::int64_t& denom)
      {
        ticks = this->_serialize_number();
        num = this->_serialize_number();
        denom = this->_serialize_number();
      }

      bool
      SerializerIn::_enter(std::string const& name)
      {
        ELLE_TRACE_SCOPE("%s: enter \"%s\"", *this, name);
        return true;
      }

      void
      SerializerIn::_leave(std::string const& name)
      {
        ELLE_TRACE_SCOPE("%s: leave \"%s\"", *this, name);
      }

      void
      SerializerIn::_serialize_array(
        int,
        std::function<void ()> const& serialize_element)
      {
        int count = _serialize_number();
        for (int i=0; i<count; ++i)
          serialize_element();
      }

      void
      SerializerIn::_deserialize_dict_key(
        std::function<void (std::string const&)> const& f)
      {
        int count = _serialize_number();
        for (int i = 0; i < count; ++i)
        {
          std::string key;
          this->_serialize(key);
          f(key);
        }
      }

      static
      char
      get(std::istream& s)
      {
        int res = s.get();
        if (res == EOF)
          err<Error>("end of stream while reading number");
        return res;
      }

      int64_t
      SerializerIn::_serialize_number()
      {
        int64_t res;
        SerializerIn::serialize_number(this->input(), res);
        return res;
      }

      size_t
      SerializerIn::serialize_number(std::istream& input,
                                     int64_t& res)
      {
        ELLE_DEBUG_SCOPE("deserialize number");
        unsigned char c = get(input);
        int64_t value;
        bool negative = c & 0x80;
        size_t size = 0;
        if (!(c & 0x40))
        {
          ELLE_DUMP("1-byte coding");
          value = c&0x3f;
          size = 1;
        }
        else if (! (c&0x20))
        {
          ELLE_DUMP("2-bytes coding");
          unsigned char c2 = get(input);
          value = ((c&0x1F) << 8) + c2;
          size = 2;
        }
        else if (! (c&0x10))
        {
          ELLE_DUMP("4-bytes coding");
          unsigned char c2 = get(input);
          unsigned char c3 = get(input);
          value = ((c&0x0F) << 16) + (c2 << 8) + c3;
          size = 3;
        }
        else
        {
          ELLE_DUMP("8-bytes coding");
          input.read((char*)(void*)&value, 8);
          size = 9;
          /*
          unsigned char elems[8];
          input.read((char*)elems, 8);
          value = 0;
          for (int i=0; i<8; ++i)
            value += uint64_t(elems[i]) << (8*i);*/
        }
        res = negative ? - (int64_t)value : value;
        ELLE_DEBUG("value: %s", res);
        return size;
      }
    }
  }
}
