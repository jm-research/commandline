#ifndef COMMANDLINE_MANAGEDSTATIC_H
#define COMMANDLINE_MANAGEDSTATIC_H

#include <atomic>
#include <cstddef>

namespace Commandline {

#define COMMANDLINE_ENABLE_THREADS 1

/// object_creator - Helper method for ManagedStatic.
template <class C>
struct ObjectCreator {
  static auto call() -> void* { return new C(); }
};

/// object_deleter - Helper method for ManagedStatic.
template <typename T>
struct ObjectDeleter {
  static void call(void* ptr) { delete (T*)ptr; }
};
template <typename T, size_t N>
struct ObjectDeleter<T[N]> {
  static void call(void* ptr) { delete[] (T*)ptr; }
};

/// ManagedStaticBase - Common base class for ManagedStatic instances.
class ManagedStaticBase {
 protected:
  mutable std::atomic<void*> Ptr{};
  mutable void (*DeleterFn)(void*) = nullptr;
  mutable const ManagedStaticBase* Next = nullptr;

  void RegisterManagedStatic(void* (*creator)(), void (*deleter)(void*)) const;

 public:
  constexpr ManagedStaticBase() = default;

  /// isConstructed - Return true if this object has not been created yet.
  auto isConstructed() const -> bool { return Ptr != nullptr; }

  void destroy() const;
};

/// ManagedStatic - This transparently changes the behavior of global statics to
/// be lazily constructed on demand and for making destruction be
/// explicit through the commandline_shutdown() function call.
template <class C, class Creator = ObjectCreator<C>,
          class Deleter = ObjectDeleter<C>>
class ManagedStatic : public ManagedStaticBase {
 public:
  // Accessors.
  auto operator*() -> C& {
    void* tmp = Ptr.load(std::memory_order_acquire);
    if (!tmp) {
      RegisterManagedStatic(Creator::call, Deleter::call);
    }

    return *static_cast<C*>(Ptr.load(std::memory_order_relaxed));
  }

  auto operator*() const -> const C& {
    void* tmp = Ptr.load(std::memory_order_acquire);
    if (!tmp) {
      RegisterManagedStatic(Creator::call, Deleter::call);
    }

    return *static_cast<C*>(Ptr.load(std::memory_order_relaxed));
  }

  auto operator->() -> C* { return &**this; }

  auto operator->() const -> const C* { return &**this; }

  // Extract the instance, leaving the ManagedStatic uninitialized. The
  // user is then responsible for the lifetime of the returned instance.
  auto claim() -> C* { return static_cast<C*>(Ptr.exchange(nullptr)); }
};

constexpr auto commandline_is_multithreaded() -> bool {
  return COMMANDLINE_ENABLE_THREADS;
}

void commandline_shutdown();

struct CommandlineShutdownObj {
  CommandlineShutdownObj() = default;
  ~CommandlineShutdownObj() { commandline_shutdown(); }
};

}  // namespace Commandline

#endif  // COMMANDLINE_MANAGEDSTATIC_H