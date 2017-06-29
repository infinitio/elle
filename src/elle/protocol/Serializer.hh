#pragma once

#include <chrono>
#include <iostream>

#include <elle/reactor/mutex.hh>

#include <elle/attribute.hh>
#include <elle/compiler.hh>

#include <elle/protocol/Stream.hh>

#ifdef EOF
# undef EOF
#endif

namespace elle
{
  namespace protocol
  {
    /// A serializer that wraps a std::iostream and is in charge of:
    /// - negotiating the version of the protocol.
    /// - splitting packets into small chunks.
    /// - write and read control bytes (e. Transfer is interrupted without
    ///   closing the connection).
    /// - ensuring data integrity (via a checksum).
    /// - etc.
    ///
    /// When a serializer is constructed on top a std::iostream, it will push
    /// its version and read the peer version in order to agree what version to
    /// use (the smallest).
    ///
    /// @code{.cc}
    ///
    /// elle::reactor::network::TCPSocket socket("127.0.0.1", 8182);
    /// elle::protocol::Serializer serializer(socket, elle::Version{0, 3, 0},
    ///                                       true);
    /// // On top of that Serializer, you can create a ChanneledStream.
    /// elle::protocol::ChanneledStream cstream(serializer);
    ///
    /// @endcode
    class ELLE_API Serializer
      : public Stream
    {
    /*------.
    | Types |
    `------*/
    public:
      using Super = Stream;
      class EOF
        : public elle::Error
      {
      public:
        EOF();
      };

    /*-------------.
    | Construction |
    `-------------*/
    public:
      /// Construct a Serializer.
      ///
      /// @param stream The underlying std::iostream.
      /// @param version The version of the protocol.
      /// @param checksum Whether it should read and write the checksum of
      ///                 packets sent.
      Serializer(std::iostream& stream,
                 elle::Version const& version = elle::Version(0, 1, 0),
                 bool checksum = true,
                 elle::DurationOpt ping_period = {},
                 elle::DurationOpt ping_timeout = {},
                 elle::Buffer::Size chunk_size = 2 << 16);
      ~Serializer();

    /*----------.
    | Receiving |
    `----------*/
    protected:
      /// Read a complete packet from the underlying stream.
      ///
      /// @returns A buffer containing the data sent by the peer.
      elle::Buffer
      _read() override;

    /*--------.
    | Sending |
    `--------*/
    protected:
      /// Write data to the stream.
      ///
      /// @param packet The packet to write.
      void
      _write(elle::Buffer const& packet) override;

    /*----------.
    | Printable |
    `----------*/
    public:
      void
      print(std::ostream& stream) const override;

    /*--------.
    | Details |
    `--------*/
      ELLE_ATTRIBUTE_RX(std::iostream&, stream);
      ELLE_ATTRIBUTE_R(elle::Version, version, override);
      ELLE_ATTRIBUTE_R(elle::Buffer::Size, chunk_size);
      ELLE_ATTRIBUTE_R(bool, checksum);
      ELLE_ATTRIBUTE_RX(boost::signals2::signal<void ()>, ping_timeout);
    public:
      class Impl;
    private:
      ELLE_ATTRIBUTE(std::unique_ptr<Impl>, impl);
    };
  }
}
