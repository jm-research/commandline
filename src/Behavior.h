#ifndef COMMANDLINE_BEHAVIOR_H
#define COMMANDLINE_BEHAVIOR_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"

#include "Option.h"

namespace Commandline {

class OptionCategory;
class SubCommand;

//===----------------------------------------------------------------------===//
// Command line option modifiers that can be used to modify the behavior of
// command line option parsers...
//

// Modifier to set the description shown in the -help output...
struct desc {
  llvm::StringRef Desc;

  desc(llvm::StringRef str) : Desc(str) {}

  void apply(Option& o) const { o.setDescription(Desc); }
};

// Modifier to set the value description shown in the -help output...
struct value_desc {
  llvm::StringRef Desc;

  value_desc(llvm::StringRef str) : Desc(str) {}

  void apply(Option& o) const { o.setValueStr(Desc); }
};

// Specify a default (initial) value for the command line argument, if the
// default constructor for the argument type does not give you what you want.
// This is only valid on "opt" arguments, not on "list" arguments.
template <class Ty>
struct Initializer {
  const Ty& Init;
  Initializer(const Ty& val) : Init(val) {}

  template <class Opt>
  void apply(Opt& o) const {
    o.setInitialValue(Init);
  }
};

template <class Ty>
struct ListInitializer {
  llvm::ArrayRef<Ty> Inits;
  ListInitializer(llvm::ArrayRef<Ty> vals) : Inits(vals) {}

  template <class Opt>
  void apply(Opt& o) const {
    o.setInitialValues(Inits);
  }
};

template <class Ty>
auto init(const Ty& val) -> Initializer<Ty> {
  return Initializer<Ty>(val);
}

template <class Ty>
auto list_init(llvm::ArrayRef<Ty> vals) -> ListInitializer<Ty> {
  return ListInitializer<Ty>(vals);
}

// Allow the user to specify which external variable they want to store the
// results of the command line argument processing into, if they don't want to
// store it in the option itself.
template <class Ty>
struct LocationClass {
  Ty& Loc;

  LocationClass(Ty& l) : Loc(l) {}

  template <class Opt>
  void apply(Opt& o) const {
    o.setLocation(o, Loc);
  }
};

template <class Ty>
auto location(Ty& l) -> LocationClass<Ty> {
  return LocationClass<Ty>(l);
}

// Specify the Option category for the command line argument to belong to.
struct cat {
  OptionCategory& Category;

  cat(OptionCategory& c) : Category(c) {}

  template <class Opt>
  void apply(Opt& o) const {
    o.addCategory(Category);
  }
};

// Specify the subcommand that this option belongs to.
struct sub {
  SubCommand& Sub;

  sub(SubCommand& s) : Sub(s) {}

  template <class Opt>
  void apply(Opt& o) const {
    o.addSubCommand(Sub);
  }
};

// Specify a callback function to be called when an option is seen.
// Can be used to set other options automatically.
template <typename R, typename Ty>
struct cb {
  std::function<R(Ty)> CB;

  cb(std::function<R(Ty)> cb) : CB(cb) {}

  template <typename Opt>
  void apply(Opt& o) const {
    o.setCallback(CB);
  }
};

namespace detail {
template <typename F>
struct callback_traits : public callback_traits<decltype(&F::operator())> {};

template <typename R, typename C, typename... Args>
struct callback_traits<R (C::*)(Args...) const> {
  using result_type = R;
  using arg_type = std::tuple_element_t<0, std::tuple<Args...>>;
  static_assert(sizeof...(Args) == 1,
                "callback function must have one and only one parameter");
  static_assert(std::is_same_v<result_type, void>,
                "callback return type must be void");
  static_assert(std::is_lvalue_reference_v<arg_type> &&
                    std::is_const_v<std::remove_reference_t<arg_type>>,
                "callback arg_type must be a const lvalue reference");
};
}  // namespace detail

template <typename F>
auto callback(F CB) -> cb<typename detail::callback_traits<F>::result_type,
                          typename detail::callback_traits<F>::arg_type> {
  using result_type = typename detail::callback_traits<F>::result_type;
  using arg_type = typename detail::callback_traits<F>::arg_type;
  return cb<result_type, arg_type>(CB);
}

}  // namespace Commandline

#endif  // COMMANDLINE_BEHAVIOR_H