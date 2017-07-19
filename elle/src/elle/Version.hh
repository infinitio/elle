#ifndef ELLE_VERSION_HH
# define ELLE_VERSION_HH

# include <elle/types.hh>
# include <elle/operator.hh>
# include <elle/attribute.hh>
# include <elle/Printable.hh>
# include <elle/serialize/fwd.hh>
# include <elle/serialize/construct.hh>

# include <elle/serialization/fwd.hh>

namespace elle
{
  /// This class represents the version of the Infinit software.
  class Version
    : public elle::Printable
  {
    /*-------------.
    | Construction |
    `-------------*/
  public:
    Version(); // XXX[to remove later]
    inline
    constexpr
    Version(elle::Natural8 major,
            elle::Natural8 minor,
            elle::Natural8 subminor);
    ELLE_SERIALIZE_CONSTRUCT_DECLARE(Version);

    /*-----------.
    | Interfaces |
    `-----------*/
  public:
    // printable
    void
    print(std::ostream& stream) const;

    // serializable
    ELLE_SERIALIZE_FRIEND_FOR(Version);

    /*----------.
    | Operators |
    `----------*/
    elle::Boolean
    operator ==(Version const&) const;
    elle::Boolean
    operator <(Version const&) const;
    elle::Boolean
    operator >(Version const&) const;
    ELLE_OPERATOR_NEQ(Version);
    ELLE_OPERATOR_LTE(Version);
    ELLE_OPERATOR_GTE(Version);

  /*-----------.
  | Attributes |
  `-----------*/
  private:
    ELLE_ATTRIBUTE(elle::Natural8, major);
    ELLE_ATTRIBUTE(elle::Natural8, minor);
    ELLE_ATTRIBUTE(elle::Natural8, subminor);

  /*--------------.
  | Serialization |
  `--------------*/
  public:
    void
    serialize(elle::serialization::Serializer& s);
  };
}

# include <elle/Version.hxx>

#endif