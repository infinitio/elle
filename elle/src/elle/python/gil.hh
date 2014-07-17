#ifndef ELLE_PYTHON_GIL_HH
# define ELLE_PYTHON_GIL_HH

#include <boost/python.hpp>

#include <elle/assert.hh>

/* boost::python call policy that release/acquires the gil
* (global interpreter lock).
* Pass as third argument to def, or to init's [] operator:
* - def("foo", &foo, release_gil_call_policies())
* - init<...>()[release_gil_call_policies]
*/

/*
 plan a): do nothing, one os thread, hopefully yielding does not matter
 plan b): assume python will dislike moving from one stack to an other
 and do something. But the python doc is unclear on whether it performs its own
 TLS or just honors saveState/restoreState.
*/
namespace elle
{
  namespace python
  {
    struct GilData
    {
      std::vector<PyThreadState*> state;
      bool initialized;
    };
    GilData& gil_data();
    struct release_gil_call_policies: public boost::python::default_call_policies
    {
      template <class ArgumentPackage>
      static bool precall(ArgumentPackage const& arg)
      {
        std::cerr << "pre " << gil_data().state.size() << std::endl;
        gil_data().state.push_back(PyEval_SaveThread());
        std::cerr << "pushed" << std::endl;
        return boost::python::default_call_policies::precall(arg);
      }
      template <class ArgumentPackage>
      static PyObject* postcall(ArgumentPackage const& args, PyObject* result)
      {
        std::cerr << "post " << gil_data().state.size() << std::endl;
        if (gil_data().state.empty() && !gil_data().initialized)
        {
          gil_data().initialized = true;
          std::cerr << "initial call" << std::endl;
          return boost::python::default_call_policies::postcall(args, result);
        }
        ELLE_ASSERT(!gil_data().state.empty());
        PyEval_RestoreThread(gil_data().state.back());
        gil_data().state.pop_back();
        std::cerr << "poped" << std::endl;
        return boost::python::default_call_policies::postcall(args, result);
      }
    };
    class ReenterPython
    {
    public:
      ReenterPython()
      {
        //gstate = PyGILState_Ensure();

        std::cerr << "reenter start " << gil_data().state.size() << std::endl;
        if (gil_data().state.empty() && !gil_data().initialized)
        {
          gstate_mode = true;
          gstate = PyGILState_Ensure();
        }
        else
        {
          gstate_mode = false;
          ELLE_ASSERT(!gil_data().state.empty());
          PyEval_RestoreThread(gil_data().state.back());
          gil_data().state.pop_back();
        }
      }
      ~ReenterPython()
      {
        std::cerr << "reenter end " << gil_data().state.size() << std::endl;
        if (gstate_mode)
          PyGILState_Release(gstate);
        else
          gil_data().state.push_back(PyEval_SaveThread());

      }
      bool gstate_mode;
      PyGILState_STATE gstate;
    };
}}

#endif
