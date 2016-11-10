#include <elle/UUID.hh>
#include <elle/serialization/json.hh>
#include <elle/test.hh>
#include <elle/optional.hh>

#include <das/Symbol.hh>
#include <das/model.hh>
#include <das/printer.hh>

struct serialization
{
  static elle::Version version;
};
elle::Version serialization::version(0, 0, 0);

DAS_SYMBOL(id);
DAS_SYMBOL(name);
DAS_SYMBOL(model);
DAS_SYMBOL(device);

class Device
{
public:
  std::string name;
  boost::optional<std::string> model;
  elle::UUID id;
  Device(std::string name_,
         boost::optional<std::string> model_ = boost::none,
         elle::UUID id_ = elle::UUID::random())
    : name(std::move(name_))
    , model(std::move(model_))
    , id(std::move(id_))
  {}

  Device()
    : name()
    , model()
    , id()
  {}

  bool
  operator ==(Device const& other) const
  {
    return this->id == other.id && this->name == other.name;
  }

  typedef serialization serialization_tag;
};

// DAS_MODEL_FIELD(Device, name);
// DAS_MODEL_FIELD(Device, model);
// DAS_MODEL_FIELD(Device, id);

// using das::operator <<;

using DasDevice =
  das::Model<
    Device,
    elle::meta::List<Symbol_id, Symbol_name, Symbol_model>>;

DAS_MODEL_DEFAULT(Device, DasDevice);

static
void
printer()
{
  BOOST_CHECK_EQUAL(
    elle::sprintf("%s", Device("name", std::string("model"), elle::UUID())),
    "Device(id = 00000000-0000-0000-0000-000000000000, name = name, model = model)");
}

class User
{
public:
  std::string name;
  Device device;

  typedef serialization serialization_tag;
};

using DasUser =
  das::Model<
    User,
    elle::meta::List<Symbol_name, Symbol_device>>;

ELLE_TEST_SUITE()
{
  auto& suite = boost::unit_test::framework::master_test_suite();
}
