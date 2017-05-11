#pragma once

#include <memory>
#include <string>
#include <vector>

#include <elle/attribute.hh>
#include <elle/compiler.hh>
#include <elle/os/environ.hh>

namespace elle
{
  namespace system
  {
    /// A process spawner.
    class ELLE_API Process
    {
    /*------.
    | Types |
    `------*/
    public:
      using Self = Process;
      using Arguments = std::vector<std::string>;

    /*-------------.
    | Construction |
    `-------------*/
    public:
      /// Create a process.
      ///
      /// @param args List of arguments, starting with the executable, followed
      ///             by the arguments.
      /// @param set_uid Set real uid/gid to effective uid/gid before exec.
      /// @param env  Variable to pass in the children's environment.
      Process(Arguments args, bool set_uid = false,
              os::Environ env = {});
      /// Create a process.
      ///
      /// @param args List of arguments, starting with the executable, followed
      ///             by the arguments.
      /// @param set_uid Set real uid/gid to effective uid/gid before exec.
      /// @param env  Variable to pass in the children's environment.
      Process(std::initializer_list<std::string> args,
              bool set_uid = false,
              os::Environ env = {});
      ~Process();

    /*-----------.
    | Attributes |
    `-----------*/
    public:
      ELLE_ATTRIBUTE_R(Arguments, arguments);
      ELLE_ATTRIBUTE_R(bool, set_uid);
      ELLE_ATTRIBUTE_R(os::Environ, env);

    /*--------.
    | Control |
    `--------*/
    public:
      /// Wait for Process to return.
      int
      wait();
      /// Get Process ID.
      int
      pid();

    /*---------------.
    | Implementation |
    `---------------*/
    private:
      class Impl;
      ELLE_ATTRIBUTE(std::unique_ptr<Impl>, impl);
    };
  }
}
