#pragma once

namespace hive
{
    template <class>
   struct max_sizeof;

   template <class... Ts>
   struct max_sizeof<std::tuple<Ts...>>
   {
      static constexpr auto value = std::max({sizeof(Ts)...});
   };

   template <class T>
   constexpr auto max_sizeof_v = max_sizeof<T>::value;

   template <class>
   struct max_alignof;

   template <class... Ts>
   struct max_alignof<std::tuple<Ts...>>
   {
      static constexpr auto value = std::max({alignof(Ts)...});
   };

   template <class T>
   constexpr auto max_alignof_v = max_alignof<T>::value;

   // Functor
   template <typename R, typename... Args>
   class Functor
   {
      // Interface polymorphique
      struct invoquable
      {
         virtual R invoquer(Args&&...) = 0;
         virtual invoquable* cloner(void*) const = 0;
         virtual ~invoquable() = default;
      };

      // Implémentation pour une fonction libre
      class fonction : public invoquable
      {
         R (*ptr)(Args...);

      public:
         fonction(R (*ptr)(Args...)) noexcept : ptr{ptr} {}

         R invoquer(Args&&... args) override
         {
            return ptr(std::forward<Args>(args)...);
         }

         fonction* cloner(void* p) const override
         {
            return new(p) fonction{*this};
         }
      };

      // Implémentation pour une méthode d'instance
      template <class T>
      class methode_instance : public invoquable
      {
         T* inst;
         R (T::*ptr)(Args...);

      public:
         methode_instance(T* inst, R (T::*ptr)(Args...)) noexcept : inst{inst}, ptr{ptr} {}

         R invoquer(Args&&... args) override
         {
            return (inst->*ptr)(std::forward<Args>(args)...);
         }

         methode_instance* cloner(void* p) const override
         {
            return new(p) methode_instance{*this};
         }
      };

      // Implémentation pour une méthode d'instance const
      template <class T>
      class methode_instance_const : public invoquable
      {
         T* inst;
         R (T::*ptr)(Args...) const;

      public:
         methode_instance_const(T* inst, R (T::*ptr)(Args...) const) noexcept : inst{inst}, ptr{ptr} {}

         R invoquer(Args&&... args) override
         {
            return (inst->*ptr)(std::forward<Args>(args)...);
         }

         methode_instance_const* cloner(void* p) const override
         {
            return new(p) methode_instance_const{*this};
         }
      };

      using types = std::tuple<
         fonction,
         methode_instance<Functor<R, Args...>>,
         methode_instance_const<Functor<R, Args...>>
      >;

      alignas(max_alignof_v<types>) char buf[max_sizeof_v<types>];
      invoquable* ptr{};

      void* internal_buffer() { return buf; }

   public:
      bool empty() const noexcept { return !ptr; }

      Functor() = default;

      Functor(R (*fct)(Args...))
         : ptr{new(internal_buffer()) fonction{fct}} {}

      template <class T>
      Functor(T* inst, R (T::*fct)(Args...))
         : ptr{new(internal_buffer()) methode_instance<T>{inst, fct}} {}

      template <class T>
      Functor(T* inst, R (T::*fct)(Args...) const)
         : ptr{new(internal_buffer()) methode_instance_const<T>{inst, fct}} {}

      Functor(const Functor& other)
         : ptr{other.empty() ? nullptr : other.ptr->cloner(internal_buffer())} {}

      Functor& operator=(const Functor& other)
      {
         if (this != &other)
         {
            if (ptr) ptr->~invoquable();
            ptr = other.empty() ? nullptr : other.ptr->cloner(internal_buffer());
         }
         return *this;
      }

      ~Functor()
      {
         if (ptr) ptr->~invoquable();
      }

      R operator()(Args... args)
      {
         return ptr->invoquer(std::forward<Args>(args)...);
      }
   };
}