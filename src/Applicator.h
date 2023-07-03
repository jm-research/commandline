#ifndef COMMANDLINE_APPLICATOR_H
#define COMMANDLINE_APPLICATOR_H

#include "Option.h"
#include "OptionEnum.h"
#include "llvm/ADT/StringRef.h"

namespace Commandline {

//===----------------------------------------------------------------------===//
// This class is used because we must use partial specialization to handle
// literal string arguments specially (const char* does not correctly respond to
// the apply method). Because the syntax to use this is a pain, we have the
// 'apply' method below to handle the nastiness...
//
template <class Mod>
struct applicator {
  template <class Opt>
  static void opt(const Mod& M, Opt& O) {
    M.apply(O);
  }
};

// Handle const char* as a special case...
template <unsigned n>
struct applicator<char[n]> {
  template <class Opt>
  static void opt(llvm::StringRef Str, Opt& O) {
    O.setArgStr(Str);
  }
};
template <unsigned n>
struct applicator<const char[n]> {
  template <class Opt>
  static void opt(llvm::StringRef Str, Opt& O) {
    O.setArgStr(Str);
  }
};
template <>
struct applicator<llvm::StringRef> {
  template <class Opt>
  static void opt(llvm::StringRef Str, Opt& O) {
    O.setArgStr(Str);
  }
};

template <>
struct applicator<NumOccurrencesFlag> {
  static void opt(NumOccurrencesFlag N, Option& O) {
    O.setNumOccurrencesFlag(N);
  }
};

template <>
struct applicator<ValueExpected> {
  static void opt(ValueExpected VE, Option& O) { O.setValueExpectedFlag(VE); }
};

template <>
struct applicator<OptionHidden> {
  static void opt(OptionHidden OH, Option& O) { O.setHiddenFlag(OH); }
};

template <>
struct applicator<FormattingFlags> {
  static void opt(FormattingFlags FF, Option& O) { O.setFormattingFlag(FF); }
};

template <>
struct applicator<MiscFlags> {
  static void opt(MiscFlags MF, Option& O) {
    assert((MF != Grouping || O.ArgStr.size() == 1) &&
           "cl::Grouping can only apply to single character Options.");
    O.setMiscFlag(MF);
  }
};

// Apply modifiers to an option in a type safe way.
template <class Opt, class Mod, class... Mods>
void apply(Opt* O, const Mod& M, const Mods&... Ms) {
  applicator<Mod>::opt(M, *O);
  apply(O, Ms...);
}

template <class Opt, class Mod>
void apply(Opt* O, const Mod& M) {
  applicator<Mod>::opt(M, *O);
}

}  // namespace Commandline

#endif  // COMMANDLINE_APPLICATOR_H