#pragma once

#include <utility>

#include <boost/operators.hpp>

#include <elle/cryptography/dh/PrivateKey.hh>
#include <elle/cryptography/fwd.hh>
#include <elle/cryptography/types.hh>
#include <elle/cryptography/Oneway.hh>
#include <elle/cryptography/Cipher.hh>

#include <elle/attribute.hh>
#include <elle/operator.hh>

//
// ---------- Class -----------------------------------------------------------
//

namespace elle
{
  namespace cryptography
  {
    namespace dh
    {
      /// Represent a public key in the DH asymmetric cryptosystem.
      class PublicKey
        : public elle::Printable
        , private boost::totally_ordered<PublicKey>
      {
        /*-------------.
        | Construction |
        `-------------*/
      public:
        /// Construct a public key out of its private counterpart.
        explicit
        PublicKey(PrivateKey const& k);
        /// Construct a public key based on the given EVP_PKEY key whose
        /// ownership is transferred.
        explicit
        PublicKey(::EVP_PKEY* key);
        /// Construct a public key based on the given DH key whose
        /// ownership is transferred to the public key.
        explicit
        PublicKey(::DH* dh);
        PublicKey(PublicKey const& other);
        PublicKey(PublicKey&& other);
        virtual
        ~PublicKey() = default;

        /*--------.
        | Methods |
        `--------*/
      private:
        /// Construct the object based on the given DH structure whose
        /// ownership is transferred to the callee.
        void
        _construct(::DH* dh);
        /// Check that the key is valid.
        void
        _check() const;
      public:
        /// Return the public key's size in bytes.
        uint32_t
        size() const;
        /// Return the public key's length in bits.
        uint32_t
        length() const;

        /*----------.
        | Operators |
        `----------*/
      public:
        bool
        operator ==(PublicKey const& other) const;
        ELLE_OPERATOR_NO_ASSIGNMENT(PublicKey);

        /*----------.
        | Printable |
        `----------*/
      public:
        void
        print(std::ostream& stream) const override;

        /*-----------.
        | Attributes |
        `-----------*/
      public:
        ELLE_ATTRIBUTE_R(types::EVP_PKEY, key);
      };
    }
  }
}

namespace std
{
  template <>
  struct hash<elle::cryptography::dh::PublicKey>
  {
    size_t
    operator ()(elle::cryptography::dh::PublicKey const& value) const;
  };
}
