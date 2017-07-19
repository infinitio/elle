#ifndef ELLE_CONTAINER_MAP_HH
# define ELLE_CONTAINER_MAP_HH

# include <iosfwd>
# include <map>
# include <unordered_map>

namespace std
{
  template <class K, class V, class ...OTHER>
  std::ostream&
  operator <<(ostream& out,
              unordered_map<K, V, OTHER...> const& m);

  template <class K, class V, class ...OTHER>
  std::ostream&
  operator <<(ostream& out,
              multimap<K, V, OTHER...> const& m);
}

# include <elle/container/map.hxx>

#endif