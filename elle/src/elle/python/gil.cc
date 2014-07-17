
#include <elle/python/gil.hh>

namespace elle
{
  namespace python
  {
    GilData& gil_data()
    {
      static GilData result;
      return result;
    }
  }
}