#include <elle/serialization/json/SerializerIn.hh>

#include <limits>

#include <elle/Backtrace.hh>
#include <elle/chrono.hh>
#include <elle/finally.hh>
#include <elle/format/base64.hh>
#include <elle/json/exceptions.hh>
#include <elle/memory.hh>
#include <elle/meta.hh>
#include <elle/printf.hh>
#include <elle/serialization/Error.hh>
#include <elle/serialization/json/Error.hh>
#include <elle/time.hh>

ELLE_LOG_COMPONENT("elle.serialization.json.SerializerIn");

namespace elle
{
  namespace serialization
  {
    namespace json
    {
      /*-------------.
      | Construction |
      `-------------*/

      SerializerIn::SerializerIn(std::istream& input,
                                 bool versioned)
        : Super(versioned)
        , _partial(false)
      {
        this->_load_json(input);
      }

      SerializerIn::SerializerIn(std::istream& input,
                                 Versions versions,
                                 bool versioned)
        : Super(std::move(versions), versioned)
        , _partial(false)
      {
        this->_load_json(input);
      }

      SerializerIn::SerializerIn(elle::json::Json input, bool versioned)
        : Super(versioned)
        , _partial(false)
        , _json(std::move(input))
      {
        this->_current.push_back(&this->_json);
      }

      void
      SerializerIn::_load_json(std::istream& input)
      {
        try
        {
          this->_json = elle::json::read(input);
          this->_current.push_back(&this->_json);
        }
        catch (elle::json::ParseError const& e)
        {
          Error exception("json parse error");
          exception.inner_exception(std::current_exception());
          throw exception;
        }
      }

      void
      SerializerIn::_serialize(int64_t& v)
      {
        v = this->_check_type<int64_t>();
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
               throw Overflow(this->current_name(), sizeof(type) * 8, true, value);
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
        this->_serialize_int<int32_t>(v);
      }

      void
      SerializerIn::_serialize(uint32_t& v)
      {
        this->_serialize_int<uint32_t>(v);
      }

      void
      SerializerIn::_serialize(int16_t& v)
      {
        this->_serialize_int<int16_t>(v);
      }

      void
      SerializerIn::_serialize(uint16_t& v)
      {
        this->_serialize_int<uint16_t>(v);
      }

      void
      SerializerIn::_serialize(int8_t& v)
      {
        this->_serialize_int<int8_t>(v);
      }

      void
      SerializerIn::_serialize(uint8_t& v)
      {
        this->_serialize_int<uint8_t>(v);
      }

      template <typename T>
      void
      SerializerIn::_serialize_int(T& v)
      {
        int64_t value;
        this->_serialize(value);
        if (value > std::numeric_limits<T>::max())
          throw Overflow(this->current_name(), sizeof(T) * 8, true, value);
        if (value < std::numeric_limits<T>::min())
          throw Overflow(this->current_name(), sizeof(T) * 8, false, value);
        v = value;
      }

      void
      SerializerIn::_serialize(double& v)
      {
        v = this->_check_type<double, int64_t>();
      }

      void
      SerializerIn::_serialize(bool& v)
      {
        v = this->_check_type<bool>();
      }

      void
      SerializerIn::_serialize_named_option(std::string const& name,
                                            bool,
                                            std::function<void ()> const& f)
      {
        auto& object = this->_check_type<elle::json::Object>();
        auto it = object.find(name);
        if (it != object.end())
          f();
        else
          ELLE_DEBUG("skip option as JSON key is missing");
      }

      void
      SerializerIn::_serialize_option(bool,
                                      std::function<void ()> const& f)
      {
        if (this->_current.back()->type() != typeid(elle::json::NullType))
          f();
        else
          ELLE_DEBUG("skip option as JSON value is null");
      }

      void
      SerializerIn::_serialize(std::string& v)
      {
        v = this->_check_type<std::string>();
      }

      void
      SerializerIn::_serialize(elle::Buffer& buffer)
      {
        auto& str = this->_check_type<std::string>();
        std::stringstream encoded(str);
        elle::format::base64::Stream base64(encoded);
        {
          elle::IOStream output(buffer.ostreambuf());
          std::copy(std::istreambuf_iterator<char>(base64),
                    std::istreambuf_iterator<char>(),
                    std::ostreambuf_iterator<char>(output));
        }
      }

      void
      SerializerIn::_serialize(boost::posix_time::ptime& time)
      {
        auto& str = this->_check_type<std::string>();
        try
        {
          time = to_posix_time(str);
        }
        catch (elle::Error const& e)
        {
          throw FieldError(this->current_name(), e.what());
        }
      }

      void
      SerializerIn::_serialize(Time& time)
      {
        boost::posix_time::ptime t;
        this->_serialize(t);
        time = from_boost<Time::clock, Time::duration>(t);
      }

      void
      SerializerIn::_serialize_time_duration(std::int64_t& ticks,
                                             std::int64_t& num,
                                             std::int64_t& denom)
      {
        auto const repr = this->_check_type<std::string>();
        elle::chrono::duration_parse(repr, ticks, num, denom);
      }

      bool
      SerializerIn::_enter(std::string const& name)
      {
        auto& object = this->_check_type<elle::json::Object>();
        auto it = object.find(name);
        if (it == object.end())
          if (this->_partial)
            return false;
          else
            throw MissingKey(name);
        else
        {
          this->_current.push_back(&it->second);
          return true;
        }
      }

      void
      SerializerIn::_leave(std::string const& name)
      {
        this->_current.pop_back();
      }

      void
      SerializerIn::_serialize_array(
        int size,
        std::function<void ()> const& serialize_element)
      {
        auto& array = this->_check_type<elle::json::Array>();
        for (auto& elt: array)
        {
          this->_current.push_back(&elt);
          serialize_element();
          this->_current.pop_back();
        }
      }

      void
      SerializerIn::_deserialize_dict_key(
        std::function<void (std::string const&)> const& f)
      {
        auto& current = *this->_current.back();
        if (current.type() == typeid(elle::json::Object))
        {
          const auto& object = boost::any_cast<elle::json::Object>(current);
          for (auto& elt: object)
            if (this->_enter(elt.first))
            {
              elle::SafeFinally leave([&] { this->_leave(elt.first); });
              f(elt.first);
            }
        }
        else if (current.type() == typeid(elle::json::Array))
        {
          auto& array = boost::any_cast<elle::json::Array&>(current);
          for (auto& elt: array)
          {
            if (elt.type() == typeid(elle::json::Array))
            {
              auto& subarray = boost::any_cast<elle::json::Array&>(elt);
              if (subarray.size() == 2
                  && subarray.front().type() == typeid(std::string))
              {
                auto key = boost::any_cast<std::string>(subarray.front());
                elle::SafeFinally leave([&] { this->_leave(key); });
                this->_current.push_back(&subarray.back());
                f(key);
              }
            }
          }
        }
      }

      template <typename T, typename ... Types>
      struct
      any_casts
      {
        static
        T&
        cast(std::string const& name, boost::any& value)
        {
          throw TypeError(name, typeid(T), value.type());
        }
      };

      template <typename T, typename First, typename ... Tail>
      struct
      any_casts<T, First, Tail ...>
      {
        static
        T&
        cast(std::string const& name, boost::any& value)
        {
          if (value.type() == typeid(First))
          {
            value = T(boost::any_cast<First&>(value));
            return boost::any_cast<T&>(value);
          }
          else
            return any_casts<T, Tail ...>::cast(name, value);
        }
      };

      template <typename T, typename ... Alternatives>
      T&
      SerializerIn::_check_type()
      {
        auto& current = *this->_current.back();
        auto name = this->_names.empty() ? "" : this->_names.back();
        if (current.type() == typeid(T))
          return boost::any_cast<T&>(current);
        return any_casts<T, Alternatives ...>::cast(name, current);
      }
    }
  }
}
