//===- SideEffects.h - SideEffect in MLIR -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains traits, interfaces, and utilities for defining and
// querying the side effects of an operation.
//
//===----------------------------------------------------------------------===//

#ifndef MLIR_INTERFACES_SIDEEFFECTS_H
#define MLIR_INTERFACES_SIDEEFFECTS_H

#include "mlir/IR/OpDefinition.h"

namespace mlir {
namespace SideEffects {
//===----------------------------------------------------------------------===//
// Effects
//===----------------------------------------------------------------------===//

/// This class represents a base class for a specific effect type.
class Effect {
public:
  /// This base class is used for derived effects that are non-parametric.
  template <typename DerivedEffect, typename BaseEffect = Effect>
  class Base : public BaseEffect {
  public:
    using BaseT = Base<DerivedEffect>;

    /// Return the unique identifier for the base effects class.
    static ClassID *getEffectID() { return ClassID::getID<DerivedEffect>(); }

    /// 'classof' used to support llvm style cast functionality.
    static bool classof(const ::mlir::SideEffects::Effect *effect) {
      return effect->getEffectID() == BaseT::getEffectID();
    }

    /// Returns a unique instance for the derived effect class.
    static DerivedEffect *get() {
      return BaseEffect::template get<DerivedEffect>();
    }
    using BaseEffect::get;

  protected:
    Base() : BaseEffect(BaseT::getEffectID()){};
  };

  /// Return the unique identifier for the base effects class.
  ClassID *getEffectID() const { return id; }

  /// Returns a unique instance for the given effect class.
  template <typename DerivedEffect> static DerivedEffect *get() {
    static_assert(std::is_base_of<Effect, DerivedEffect>::value,
                  "expected DerivedEffect to inherit from Effect");

    static DerivedEffect instance;
    return &instance;
  }

protected:
  Effect(ClassID *id) : id(id) {}

private:
  /// The id of the derived effect class.
  ClassID *id;
};

//===----------------------------------------------------------------------===//
// Resources
//===----------------------------------------------------------------------===//

/// This class represents a specific resource that an effect applies to. This
/// class represents an abstract interface for a given resource.
class Resource {
public:
  virtual ~Resource() {}

  /// This base class is used for derived effects that are non-parametric.
  template <typename DerivedResource, typename BaseResource = Resource>
  class Base : public BaseResource {
  public:
    using BaseT = Base<DerivedResource>;

    /// Returns a unique instance for the given effect class.
    static DerivedResource *get() {
      static DerivedResource instance;
      return &instance;
    }

    /// Return the unique identifier for the base resource class.
    static ClassID *getResourceID() {
      return ClassID::getID<DerivedResource>();
    }

    /// 'classof' used to support llvm style cast functionality.
    static bool classof(const Resource *resource) {
      return resource->getResourceID() == BaseT::getResourceID();
    }

  protected:
    Base() : BaseResource(BaseT::getResourceID()){};
  };

  /// Return the unique identifier for the base resource class.
  ClassID *getResourceID() const { return id; }

  /// Return a string name of the resource.
  virtual StringRef getName() = 0;

protected:
  Resource(ClassID *id) : id(id) {}

private:
  /// The id of the derived resource class.
  ClassID *id;
};

/// A conservative default resource kind.
struct DefaultResource : public Resource::Base<DefaultResource> {
  StringRef getName() final { return "<Default>"; }
};

/// This class represents a specific instance of an effect. It contains the
/// effect being applied, a resource that corresponds to where the effect is
/// applied, and an optional value(either operand, result, or region entry
/// argument) that the effect is applied to.
template <typename EffectT> class EffectInstance {
public:
  EffectInstance(EffectT *effect, Resource *resource = DefaultResource::get())
      : effect(effect), resource(resource) {}
  EffectInstance(EffectT *effect, Value value,
                 Resource *resource = DefaultResource::get())
      : effect(effect), resource(resource), value(value) {}

  /// Return the effect being applied.
  EffectT *getEffect() const { return effect; }

  /// Return the value the effect is applied on, or nullptr if there isn't a
  /// known value being affected.
  Value getValue() const { return value; }

  /// Return the resource that the effect applies to.
  Resource *getResource() const { return resource; }

private:
  /// The specific effect being applied.
  EffectT *effect;

  /// The resource that the given value resides in.
  Resource *resource;

  /// The value that the effect applies to. This is optionally null.
  Value value;
};
} // namespace SideEffects

//===----------------------------------------------------------------------===//
// SideEffect Traits
//===----------------------------------------------------------------------===//

namespace OpTrait {
/// This trait indicates that the side effects of an operation includes the
/// effects of operations nested within its regions. If the operation has no
/// derived effects interfaces, the operation itself can be assumed to have no
/// side effects.
template <typename ConcreteType>
class HasRecursiveSideEffects
    : public TraitBase<ConcreteType, HasRecursiveSideEffects> {};
} // namespace OpTrait

//===----------------------------------------------------------------------===//
// Operation Memory-Effect Modeling
//===----------------------------------------------------------------------===//

namespace MemoryEffects {
/// This class represents the base class used for memory effects.
struct Effect : public SideEffects::Effect {
  using SideEffects::Effect::Effect;

  /// A base class for memory effects that provides helper utilities.
  template <typename DerivedEffect>
  using Base = SideEffects::Effect::Base<DerivedEffect, Effect>;

  static bool classof(const SideEffects::Effect *effect);
};
using EffectInstance = SideEffects::EffectInstance<Effect>;

/// The following effect indicates that the operation allocates from some
/// resource. An 'allocate' effect implies only allocation of the resource, and
/// not any visible mutation or dereference.
struct Allocate : public Effect::Base<Allocate> {};

/// The following effect indicates that the operation frees some resource that
/// has been allocated. An 'allocate' effect implies only de-allocation of the
/// resource, and not any visible allocation, mutation or dereference.
struct Free : public Effect::Base<Free> {};

/// The following effect indicates that the operation reads from some resource.
/// A 'read' effect implies only dereferencing of the resource, and not any
/// visible mutation.
struct Read : public Effect::Base<Read> {};

/// The following effect indicates that the operation writes to some resource. A
/// 'write' effect implies only mutating a resource, and not any visible
/// dereference or read.
struct Write : public Effect::Base<Write> {};
} // namespace MemoryEffects

//===----------------------------------------------------------------------===//
// SideEffect Interfaces
//===----------------------------------------------------------------------===//

/// Include the definitions of the side effect interfaces.
#include "mlir/Interfaces/SideEffectInterfaces.h.inc"

//===----------------------------------------------------------------------===//
// SideEffect Utilities
//===----------------------------------------------------------------------===//

/// Return true if the given operation is unused, and has no side effects on
/// memory that prevent erasing.
bool isOpTriviallyDead(Operation *op);

/// Return true if the given operation would be dead if unused, and has no side
/// effects on memory that would prevent erasing. This is equivalent to checking
/// `isOpTriviallyDead` if `op` was unused.
bool wouldOpBeTriviallyDead(Operation *op);

} // end namespace mlir

#endif // MLIR_INTERFACES_SIDEEFFECTS_H
