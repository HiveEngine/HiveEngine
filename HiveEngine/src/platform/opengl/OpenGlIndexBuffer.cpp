
#ifdef HIVE_BACKEND_OPENGL
#include "OpenGlIndexBuffer.hpp"
#include "glad/glad.h"

namespace hive {
    OpenGlIndexBuffer::OpenGlIndexBuffer(uint32_t *indices, uint32_t count)
        :count_(count) {
        glGenBuffers(1, &bufferID_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufferID_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, count, indices, GL_STATIC_DRAW);
    }

    OpenGlIndexBuffer::~OpenGlIndexBuffer() {
        glDeleteBuffers(1, &bufferID_);
    }

    void OpenGlIndexBuffer::bind() const {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufferID_);
    }

    void OpenGlIndexBuffer::unbind() const {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
}
#endif
