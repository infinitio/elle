//
// ---------- header ----------------------------------------------------------
//
// project       elle
//
// license       infinit
//
// file          /home/mycure/infinit/elle/network/Identifier.cc
//
// created       julien quintard   [wed mar  3 13:55:58 2010]
// updated       julien quintard   [sun mar  7 21:00:59 2010]
//

//
// ---------- includes --------------------------------------------------------
//

#include <elle/network/Identifier.hh>

namespace elle
{
  namespace network
  {

//
// ---------- definitions -----------------------------------------------------
//

    ///
    /// this variable defines an unused hence null Identifier.
    ///
    const Identifier			Identifier::Null;

//
// ---------- constructors & destructors --------------------------------------
//

    ///
    /// default constructor.
    ///
    Identifier::Identifier():
      value(0)
    {
    }

//
// ---------- methods ---------------------------------------------------------
//

    ///
    /// this method generates a new unique identifier.
    ///
    Status		Identifier::Generate()
    {
      enter();

      // try until the generated identifier is different from Null.
      do
	{
	  // generate random bytes.
	  if (RAND_bytes((unsigned char*)&this->value,
			 sizeof(this->value)) == 0)
	    escape(::ERR_error_string(ERR_get_error(), NULL));
	} while (*this == Identifier::Null);

      leave();
    }

//
// ---------- entity ----------------------------------------------------------
//

    ///
    /// this method check if two identifiers match.
    ///
    Boolean		Identifier::operator==(const Identifier& element) const
    {
      return (this->value == element.value);
    }

//
// ---------- archivable ------------------------------------------------------
//

    ///
    /// this method serializes the identifier.
    ///
    Status		Identifier::Serialize(Archive&		archive) const
    {
      enter();

      // serialize the attributes.
      if (archive.Serialize(this->value) == StatusError)
	escape("unable to serialize the identifier attributes");

      leave();
    };

    ///
    /// this method extracts the identifier.
    ///
    Status		Identifier::Extract(Archive&		archive)
    {
      enter();

      // extract the attributes.
      if (archive.Extract(this->value) == StatusError)
	escape("unable to extract the identifier attributes");

      leave();
    };

//
// ---------- dumpable --------------------------------------------------------
//

    ///
    /// this method dumps an identifier.
    ///
    Status		Identifier::Dump(const Natural32	margin) const
    {
      String		alignment(margin, ' ');

      enter();

      std::cout << alignment << "[Identifier] " << this->value << std::endl;

      leave();
    }

//
// ---------- operators -------------------------------------------------------
//

    ///
    /// this operator compares two identifiers.
    ///
    Boolean		operator<(const Identifier&		lhs,
				  const Identifier&		rhs)
    {
      return (lhs.value < rhs.value);
    }

  }
}
