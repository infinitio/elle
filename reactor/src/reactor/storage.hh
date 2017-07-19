#ifndef INFINIT_REACTOR_STORAGE_HH
# define INFINIT_REACTOR_STORAGE_HH

# include <mutex>
# include <unordered_map>

# include <reactor/fwd.hh>

namespace reactor
{
  template <typename T>
  class LocalStorage
  {
  public:
    typedef LocalStorage<T> Self;
    LocalStorage();
    operator T&();
    T& Get(T const& def);
    T& Get();

  private:
    void _Clean(Thread* t);
    typedef std::unordered_map<void*, T> Content;
    Content _content;
    std::mutex _mutex;
  };
}

# include <reactor/storage.hxx>

#endif