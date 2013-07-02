#ifndef INFINIT_REACTOR_STORAGE_HXX
# define INFINIT_REACTOR_STORAGE_HXX

# include <reactor/scheduler.hh>
# include <reactor/thread.hh>

# include <elle/assert.hh>

namespace reactor
{
  template <typename T>
  LocalStorage<T>::LocalStorage()
    : _content()
  {}

  template <typename T>
  LocalStorage<T>::operator T&()
  {
    return this->Get();
  }

  // XXX: factor with Get(T const& def)
  template <typename T>
  T&
  LocalStorage<T>::Get()
  {
    Scheduler* sched = Scheduler::scheduler();
    Thread* current = sched ? sched->current() : 0;

    std::lock_guard<std::mutex> lock(_mutex);
    typename Content::iterator it = this->_content.find(current);
    if (it == this->_content.end())
      {
        if (current != nullptr)
          current->destructed().connect(
              boost::bind(&Self::_Clean, this, current)
          );
        return this->_content[current];
      }
    else
      return it->second;
  }


  template <typename T>
  T&
  LocalStorage<T>::Get(T const& def)
  {
    Scheduler* sched = Scheduler::scheduler();
    Thread* current = sched ? sched->current() : 0;
    std::lock_guard<std::mutex> lock(_mutex);
    typename Content::iterator it = this->_content.find(current);
    if (it == this->_content.end())
      {
        this->_content[current] = def;
        if (current != nullptr)
          current->destructed().connect(
              boost::bind(&Self::_Clean, this, current)
          );
        return this->_content[current];
      }
    else
      return it->second;
  }

  template <typename T>
  void
  LocalStorage<T>::_Clean(Thread* current)
  {
    std::lock_guard<std::mutex> lock(_mutex);
    typename Content::iterator it = this->_content.find(current);
    ELLE_ASSERT_NEQ(it, this->_content.end());
    this->_content.erase(it);
  }
}

#endif
