#pragma once

#include <wax/containers/string.h>

#include <queen/hierarchy/hierarchy.h>
#include <queen/reflect/component_registry.h>
#include <queen/reflect/json_serializer.h>
#include <queen/world/world.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace queen
{
    /**
     * Result of a world serialization operation
     */
    struct WorldSerializeResult
    {
        bool m_success = false;
        size_t m_entitiesWritten = 0;
        size_t m_componentsWritten = 0;
    };

    namespace detail
    {
        template <typename Writer> class WorldSerializerCore
        {
        public:
            WorldSerializerCore() noexcept = default;

            template <size_t MaxComponents>
            WorldSerializeResult Serialize(World& world, const ComponentRegistry<MaxComponents>& registry) noexcept
            {
                WorldSerializeResult result{};
                m_writer.Reset();

                WriteRaw("{\"version\":1,\"entities\":[");

                bool firstEntity = true;
                DynamicJsonSerializer componentSerializer{};

                world.ForEachArchetype([&](Archetype<ComponentAllocator>& archetype) {
                    if (!m_writer.Success())
                    {
                        return;
                    }

                    const auto& types = archetype.GetComponentTypes();
                    for (uint32_t row = 0; row < archetype.EntityCount(); ++row)
                    {
                        if (!m_writer.Success())
                        {
                            return;
                        }

                        const Entity entity = archetype.GetEntity(row);
                        if (!firstEntity)
                        {
                            Put(',');
                        }
                        firstEntity = false;

                        WriteRaw("{\"id\":");
                        WriteUint64(entity.ToU64());

                        const Entity parent = world.GetParent(entity);
                        if (!parent.IsNull())
                        {
                            WriteRaw(",\"parent\":");
                            WriteUint64(parent.ToU64());
                        }

                        WriteRaw(",\"components\":{");

                        bool firstComp = true;
                        for (size_t c = 0; c < types.Size(); ++c)
                        {
                            const TypeId typeId = types[c];
                            if (typeId == TypeIdOf<Parent>() || typeId == TypeIdOf<Children>())
                            {
                                continue;
                            }

                            const RegisteredComponent* reg = registry.Find(typeId);
                            if (reg == nullptr || !reg->HasReflection())
                            {
                                continue;
                            }

                            if (!firstComp)
                            {
                                Put(',');
                            }
                            firstComp = false;

                            Put('"');
                            WriteRaw(reg->m_reflection.m_name);
                            Put('"');
                            Put(':');

                            const void* data = archetype.GetComponentRaw(row, typeId);
                            componentSerializer.SerializeComponent(data, reg->m_reflection);
                            WriteRaw(componentSerializer.CStr());

                            ++result.m_componentsWritten;
                        }

                        WriteRaw("}}");
                        ++result.m_entitiesWritten;
                    }
                });

                WriteRaw("]}");
                m_writer.Terminate();
                result.m_success = m_writer.Success();
                return result;
            }

            [[nodiscard]] const char* CStr() const noexcept
            {
                return m_writer.CStr();
            }

            [[nodiscard]] size_t Size() const noexcept
            {
                return m_writer.Size();
            }

            [[nodiscard]] bool Overflowed() const noexcept
            {
                return m_writer.Overflowed();
            }

        protected:
            [[nodiscard]] Writer& WriterRef() noexcept
            {
                return m_writer;
            }

            [[nodiscard]] const Writer& WriterRef() const noexcept
            {
                return m_writer;
            }

        private:
            void Put(char c) noexcept
            {
                m_writer.Put(c);
            }

            void WriteRaw(const char* s) noexcept
            {
                m_writer.WriteRaw(s);
            }

            void WriteUint64(uint64_t v) noexcept
            {
                char tmp[24];
                const int count = std::snprintf(tmp, sizeof(tmp), "%llu", static_cast<unsigned long long>(v));
                for (int i = 0; i < count; ++i)
                {
                    Put(tmp[i]);
                }
            }

            Writer m_writer{};
        };
    } // namespace detail

    /**
     * Serializes a World to JSON using reflection and a ComponentRegistry.
     *
     * This fixed-buffer variant remains useful for tests and explicit bounds
     * checking, but scene save paths should prefer DynamicWorldSerializer.
     */
    template <size_t BufSize = 65536>
    class WorldSerializer : public detail::WorldSerializerCore<detail::FixedTextWriter<BufSize>>
    {
    };

    class DynamicWorldSerializer : public detail::WorldSerializerCore<detail::DynamicTextWriter>
    {
    public:
        [[nodiscard]] const wax::String& String() const noexcept
        {
            return this->WriterRef().String();
        }

        [[nodiscard]] wax::String TakeString() noexcept
        {
            return this->WriterRef().TakeString();
        }
    };
} // namespace queen
