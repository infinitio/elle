#ifndef ELLE_SFINAE_HH
# define ELLE_SFINAE_HH

# include <cstddef>

# include <boost/preprocessor/cat.hpp>

namespace elle
{
  namespace sfinae
  {
    template<size_t>
    struct Helper
    {};
  }
}

# define ELLE_SFINAE_IF_WORKS(Expr)                             \
  int, ::elle::sfinae::Helper<sizeof(Expr, 0)>* = 0

# define ELLE_SFINAE_IF_POSSIBLE()                             \
  int

# define ELLE_SFINAE_OTHERWISE()                \
  unsigned int

# define ELLE_SFINAE_TRY()                      \
  42

# define ELLE_SFINAE_INSTANCE(Type)             \
  (*reinterpret_cast<typename std::remove_reference<Type>::type*>((void*)(0)))

# define ELLE_STATIC_PREDICATE(Name, Test, ...)                         \
  template <typename T, ## __VA_ARGS__>                                 \
  inline constexpr                                                      \
  typename std::enable_if_exists<ELLE_ATTRIBUTE_STRIP_PARENS(Test),     \
                                 bool>::type                            \
  BOOST_PP_CAT(_, Name)(int)                                            \
  {                                                                     \
    return true;                                                        \
  }                                                                     \
                                                                        \
  template <typename T, ## __VA_ARGS__>                                 \
  inline constexpr                                                      \
  bool                                                                  \
  BOOST_PP_CAT(_, Name)(...)                                            \
  {                                                                     \
    return false;                                                       \
  }                                                                     \
                                                                        \
  template <typename T, ## __VA_ARGS__>                                 \
  inline constexpr                                                      \
  bool                                                                  \
  Name()                                                                \
  {                                                                     \
    return BOOST_PP_CAT(_, Name)<T>(42);                                \
  }                                                                     \

#endif
