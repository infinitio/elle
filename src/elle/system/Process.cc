#include <elle/system/Process.hh>

#include <cerrno>
#include <cstdlib> // environ

#include <elle/assert.hh>
#include <elle/err.hh>
#include <elle/log.hh>
#include <elle/make-vector.hh>
#include <elle/system/unistd.hh>

#if defined INFINIT_WINDOWS
# include <elle/windows.h>
#elif defined INFINIT_MACOSX
# include <crt_externs.h>
#else
# include <sys/types.h>
# include <sys/wait.h>
#endif

ELLE_LOG_COMPONENT("elle.system.Process");

namespace
{
  int execvpe(const char* program, char** argv, char** envp)
  {
#ifdef INFINIT_MACOSX
    char** saved = *(_NSGetEnviron());
    *(_NSGetEnviron()) = envp;
#else
    char** saved = ::environ;
    ::environ = envp;
#endif
    auto res = execvp(program, argv);
#ifdef INFINIT_MACOSX
    *(_NSGetEnviron()) = saved;
#else
    ::environ = saved;
#endif
    return res;
  }

  auto to_c(std::vector<std::string> const& args)
  {
    auto res = std::unique_ptr<char const*[]>(new char const*[args.size() + 1]);
    int i = 0;
    for (auto const& arg: args)
      res[i++] = arg.c_str();
    res[i] = nullptr;
    return res;
  }

  auto flatten(elle::os::Environ const& env)
  {
    auto res = std::vector<std::string>{};
    for (auto p: env)
      res.emplace_back(p.first + "=" + p.second);
    return res;
  }
}

namespace elle
{
  namespace system
  {
#ifndef INFINIT_WINDOWS
    class Process::Impl
    {
    public:
      Impl(Process& owner)
        : _owner(owner)
        , _pid(0)
        , _status(0)
        , _done(false)
      {
        this->_pid = fork();
        if (this->_pid == 0)
        {
          auto argv = to_c(this->_owner.arguments());
          auto vars = flatten(this->_owner.env());
          auto env = to_c(vars);
          try
          {
            if (_owner.set_uid())
            {
              auto tgt = geteuid();
              elle::seteuid(getuid());
              elle::setgid(getegid());
              elle::setuid(tgt);
            }
            execvpe(argv[0], const_cast<char**>(argv.get()), const_cast<char**>(env.get()));
            ELLE_ERR("execvpe(%s) error: %s", argv[0], strerror(errno));
            ::exit(1);
          }
          catch (...)
          {
            ELLE_ERR("execvpe(%s) error: %s", argv[0], strerror(errno));
            ::exit(1);
          }
        }
      }

      void
      wait()
      {
        pid_t waited;
        do
        {
          waited = ::waitpid(this->_pid, &this->_status, 0);
        }
        while (waited == -1 && errno == EINTR);
        if (waited == -1)
          elle::err("unable to wait process: %s", strerror(errno));
        assert(waited == this->_pid);
        this->_done = true;
      }

    private:
      ELLE_ATTRIBUTE(Process&, owner);
      ELLE_ATTRIBUTE_R(pid_t, pid);
      ELLE_ATTRIBUTE_R(int, status);
      ELLE_ATTRIBUTE_R(bool, done);
    };

#else

    class Process::Impl
    {
    public:
      Impl(Process& owner)
        : _owner(owner)
        , _process_info()
        , _status(0)
        , _done(false)
      {
        STARTUPINFO startup_info = {sizeof(STARTUPINFO)};
        std::string executable = this->_owner.arguments()[0];
        std::string command_line;
        int i = 0;
        for (auto const& arg: this->_owner.arguments())
        {
          if (i++ == 0)
            continue;
          if (i > 1)
            command_line += " ";
          command_line += arg;
        }
        auto env = std::string{};
        for (auto const& p: this->_owner.env())
        {
          env += p.first + " " + p.second;
          env += '\0';
        }
        env += '\0';
        auto envp =
          this->_owner.env().empty()) ? nullptr : env.data();
        if (!::CreateProcess(executable.c_str(),
                             strdup(command_line.c_str()),
                             nullptr, // ProcessAttributes.
                             nullptr, // ThreadAttributes.
                             true,    // InheritHandles.
                             0,       // CreationFlags.
                             envp,    // Environment.
                             nullptr, // CurrentDirectory.
                             &startup_info,
                             &this->_process_info))
          elle::err("unable to start %s: %s", executable, ::GetLastError());
      }

      void
      wait()
      {
        ::WaitForSingleObject(this->_process_info.hProcess, INFINITE);
        unsigned long status = 0;
        ::GetExitCodeProcess(this->_process_info.hProcess, &status);
        this->_status = status;
        this->_done = true;
      }

      int
      pid()
      {
        return this->_process_info.dwProcessId;
      }

    private:
      ELLE_ATTRIBUTE(Process&, owner);
      ELLE_ATTRIBUTE_R(PROCESS_INFORMATION, process_info)
      ELLE_ATTRIBUTE_R(int, status);
      ELLE_ATTRIBUTE_R(bool, done);
    };
#endif

    Process::Process(std::vector<std::string> args, bool set_uid,
                     os::Environ env)
      : _arguments(std::move(args))
      , _set_uid(set_uid)
      , _env(std::move(env))
      , _impl(new Process::Impl(*this))
    {
      ELLE_TRACE_SCOPE("%s: start", *this);
    }

    Process::Process(std::initializer_list<std::string> args, bool set_uid,
                     os::Environ env)
      : Self{make_vector(args), set_uid, env}
    {}

    Process::~Process()
    {}

    int
    Process::wait()
    {
      if (!this->_impl->done())
        this->_impl->wait();
      ELLE_ASSERT(this->_impl->done());
      return this->_impl->status();
    }

    int
    Process::pid()
    {
      return this->_impl->pid();
    }
  }
}
