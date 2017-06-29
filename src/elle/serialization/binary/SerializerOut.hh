#pragma once

#include <vector>

#include <elle/attribute.hh>
#include <elle/serialization/SerializerOut.hh>

namespace elle
{
  namespace serialization
  {
    namespace binary
    {
      /// A specialized SerializerOut for binary.
      ///
      /// Serialize object to their binary representation.
      ///
      /// Details:
      /// - In binary, order matters. Do not reorder members afterward,
      ///   otherwise the existing serialized version won't be deserializable
      ///   anymore.
      class ELLE_API SerializerOut
        : public serialization::SerializerOut
      {
      /*------.
      | Types |
      `------*/
      public:
        using Self = SerializerOut;
        using Super = serialization::SerializerOut;

      /*-------------.
      | Construction |
      `-------------*/
      public:
        /// Construct a SerializerOut for binary.
        ///
        /// @see elle::serialization::SerializerOut.
        SerializerOut(std::ostream& output, bool versioned = true);
        /// Construct a SerializerOut for binary.
        ///
        /// @see elle::serialization::SerializerOut.
        SerializerOut(std::ostream& output,
                      Versions versions, bool versioned = true);
        virtual
        ~SerializerOut();
      private:
        void
        _write_magic(std::ostream& output);

      /*--------------.
      | Serialization |
      `--------------*/
      protected:
        void
        _size(int size) override;
        bool
        _text() const override;
        void
        _serialize_array(int size,
                         std::function<void ()> const& f) override;
        void
        _serialize_dict_key(std::string const& name,
                            std::function<void ()> const& f) override;
        void
        _serialize(int64_t& v) override;
        void
        _serialize(uint64_t& v) override;
        void
        _serialize(ulong& v) override;
        void
        _serialize(int32_t& v) override;
        void
        _serialize(uint32_t& v) override;
        void
        _serialize(int16_t& v) override;
        void
        _serialize(uint16_t& v) override;
        void
        _serialize(int8_t& v) override;
        void
        _serialize(uint8_t& v) override;
        void
        _serialize(double& v) override;
        void
        _serialize(bool& v) override;
        void
        _serialize(std::string& v) override;
        void
        _serialize(elle::Buffer& v) override;
        void
        _serialize(boost::posix_time::ptime& v) override;
        void
        _serialize(Time& v) override;
        void
        _serialize_time_duration(std::int64_t& ticks,
                                 std::int64_t& num,
                                 std::int64_t& denom) override;
        void
        _serialize_named_option(std::string const& name,
                                bool filled,
                                std::function<void ()> const& f) override;
        void
        _serialize_option(bool filled,
                          std::function<void ()> const& f) override;
      public:
        static
        size_t
        serialize_number(std::ostream& output,
                         int64_t number);
        ELLE_ATTRIBUTE_R(std::ostream&, output);
      private:
        void
        _serialize_number(int64_t number);
      };
    }
  }
}
