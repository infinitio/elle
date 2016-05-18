#include <boost/context/all.hpp>

#ifdef VALGRIND
# include <valgrind/valgrind.h>
#endif

#include <elle/Backtrace.hh>
#include <elle/assert.hh>
#include <elle/log.hh>

#include <reactor/backend/boost_context/backend.hh>
#include <reactor/exception.hh>

ELLE_LOG_COMPONENT("reactor.backend");

namespace reactor
{
  namespace backend
  {
    namespace boost_context
    {
      /*----------------.
      | Stack Allocator |
      `----------------*/
      template <std::size_t Max, std::size_t Default, std::size_t Min>
      class TemplatedStackAllocator
      {
      public:
        static
        std::size_t
        maximum_stack_size()
        {
          return Max;
        }
        static
        std::size_t
        default_stack_size()
        {
          return Default;
        }
        static
        std::size_t
        minimum_stack_size()
        {
          return Min;
        }

        void*
        allocate(std::size_t size) const
        {
          ELLE_ASSERT(minimum_stack_size() <= size);
          ELLE_ASSERT(maximum_stack_size() >= size);

          void* limit = std::malloc(size);
          if (!limit)
            throw std::bad_alloc();

          return static_cast<char*>(limit) + size;
        }

        void
        deallocate(void* vp, std::size_t size) const
        {
          ELLE_ASSERT(vp);
          ELLE_ASSERT(minimum_stack_size() <= size);
          ELLE_ASSERT(maximum_stack_size() >= size);

          void* limit = static_cast<char*>(vp) - size;
          std::free(limit);
        }
      };

      /*-------.
      | Thread |
      `-------*/
      /// Default allocator type.
      typedef TemplatedStackAllocator<
        8 * 1024 * 1024,  // Max: 8 MiB
        4 * 128 * 1024,       // Default: 128 kiB
        8 * 1024          // Min: 8 kiB
        > StackAllocator;
      /// Type of context pointer used.
      typedef boost::context::execution_context<void> Context;

      /// Allocator.
      static StackAllocator stack_allocator;

      class Backend::Thread:
        public backend::Thread
      {
      /*---------.
      | Typedefs |
      `---------*/
      public:
        typedef Thread Self;
        typedef backend::Thread Super;
      /*-------------.
      | Construction |
      `-------------*/
      public:
        Thread(Backend& backend,
               const std::string& name,
               Action action)
          : Super(name, std::move(action))
          , _backend(backend)
          , _stack_size(StackAllocator::default_stack_size())
          , _stack_pointer(stack_allocator.allocate(this->_stack_size))
          , _context(
            [this] (boost::context::execution_context<void> caller)
            {
              this->_caller->_context = std::move(caller);
              return this->_run();
            })
          , _caller(nullptr)
          , _root(false)
        {
          #ifdef VALGRIND
          this->_valgrind_stack =
            VALGRIND_STACK_REGISTER(
              reinterpret_cast<char*>(this->_stack_pointer) - this->_stack_size,
              this->_stack_pointer);
          #endif
        }

        ~Thread()
        {
          ELLE_ASSERT(this->status() == Status::done ||
                      this->status() == Status::starting ||
                      this->_root);
          ELLE_TRACE("%s: die", *this);
          #ifdef VALGRIND
          VALGRIND_STACK_DEREGISTER(this->_valgrind_stack);
          #endif
        }

      private:
        Thread(Backend& backend)
          : Thread(backend, "<root>", Action())
        {
          this->_root = true;
          this->status(Status::running);
        }

      /*----------.
      | Switching |
      `----------*/
      public:
        virtual
        void
        step() override
        {
          // go from current to this
          ELLE_ASSERT(this->_caller == nullptr);
          bool starting = this->status() == Status::starting;
          Thread* current = this->_backend._current;
          this->_caller = current;
          this->_backend._current = this;
          current->_unwinding = std::uncaught_exception();
          if (current->_unwinding)
            ELLE_DUMP("step %s with in-flight exception", *current);
          if (starting)
          {
            this->status(Status::running);
            ELLE_TRACE("%s: start %s", *current , *this);
            this->_context = this->_context();
            ELLE_TRACE("%s: back from %s", *current, *this);
          }
          else
          {
            ELLE_ASSERT_EQ(this->status(), Status::waiting);
            this->status(Status::running);
            ELLE_TRACE("%s: step from %s", *this, *_caller);
            this->_context = this->_context();
          }
          // It is unclear whether an uncaught_exception mismatch has any
          // consequence if the code does not explicitly depend on its result.
          // Warn about it just in case.
          if (this->_backend._current->_unwinding != std::uncaught_exception())
          {
            ELLE_TRACE("step %s: unwind mismatch, expect %s, got %s",
              *this->_backend._current, this->_backend._current->_unwinding,
              std::uncaught_exception());
          }
        }


        virtual
        void
        yield() override
        {
          // go from this to caller
          ELLE_ASSERT_EQ(this->_backend._current, this);
          ELLE_ASSERT_EQ(this->status(), Status::running);
          this->status(Status::waiting);
          // Store current exception and stack unwinding state
          this->_unwinding = std::uncaught_exception();
          this->_exception = std::current_exception();
          auto caller = this->_caller;
          this->_backend._current = caller;
          ELLE_TRACE("%s: yield back to %s", *this, *this->_backend._current);
          this->_caller = nullptr;
          if (this->_unwinding)
            ELLE_DUMP("yielding %s with in-flight exception", *this);
          caller->_context = caller->_context();
          if (this->_backend._current->_unwinding != std::uncaught_exception())
          {
            ELLE_TRACE("yield %s: unwind mismatch, expect %s, got %s",
              *this->_backend._current, this->_backend._current->_unwinding,
              std::uncaught_exception());
          }
        }


      /*--------.
      | Details |
      `--------*/
      private:
        /// Let the backend use our private constructor.
        friend class Backend;
        /// Main routine of this thread of execution.
        boost::context::execution_context<void>
        _run()
        {
          this->status(Status::running);
          try
          {
            this->action()();
          }
          catch (reactor::Exception const& e)
          {
            std::cerr << "Thread " << this->name()
                      << " killed by exception "
                      << elle::demangle(typeid(e).name()) << ": "
                      << e.what() << "." << std::endl;
            std::cerr << e.backtrace() << std::endl;
            std::abort();
          }
          catch (const std::exception& e)
          {
            std::cerr << "Thread " << this->name()
                      << " killed by exception "
                      << elle::demangle(typeid(e).name()) << ": "
                      << e.what() << "." << std::endl;
            std::abort();
          }
          catch (...)
          {
            std::cerr << "Thread " << this->name()
                      << " killed by unknown exception."
                      << std::endl;
            std::abort();
          }
          Thread* caller = this->_caller;
          this->_caller = nullptr;
          this->status(Status::done);
          this->_backend._current = caller;
          ELLE_TRACE("%s: done", *this);
          return caller->_context();
        }

        /// Owning backend.
        Backend& _backend;
        /// Context stack size.
        std::size_t _stack_size;
        /// Context stack pointer.
        void* _stack_pointer;
        /// Underlying IO context.
        Context _context;
        /// The thread that stepped us.
        Thread* _caller;
        ELLE_ATTRIBUTE(bool, root);
        #ifdef VALGRIND
        ELLE_ATTRIBUTE(unsigned int, valgrind_stack);
        #endif
      };

      /*--------.
      | Backend |
      `--------*/

      Backend::Backend():
        _self(new Thread(*this)),
        _current(this->_self.get())
      {}

      Backend::~Backend()
      {}

      std::unique_ptr<backend::Thread>
      Backend::make_thread(const std::string& name, Action action)
      {
        return std::unique_ptr<backend::Thread>(
          new Thread(*this, name, std::move(action)));
      }

      Thread*
      Backend::current() const
      {
        return this->_current;
      }
    }
  }
}
