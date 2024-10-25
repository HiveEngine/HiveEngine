#pragma once
#ifdef HIVE_BACKEND_OPENGL
#include <hvpch.h>

namespace hive {
	class IndexBuffer
	{
	public:
		virtual ~IndexBuffer() {}

		virtual void bind() const = 0;
		virtual void unbind() const = 0;

		virtual uint32_t getCount() const = 0;

		static IndexBuffer* create(uint32_t* indices, uint32_t count);
	};
}
#endif