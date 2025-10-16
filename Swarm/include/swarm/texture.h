#pragma once

namespace swarm
{
    class Device;

    enum class TextureType
    {
        COLOR, DEPTH,
    };

    struct TextureDescription
    {
        unsigned int width, height;
        TextureType type;
    };

    class Texture
    {
    public:
        explicit Texture(Device& device, TextureDescription desc);
        ~Texture();

    protected:
        Device& m_Device;
    private:
        #include <swarm/texturebackend.inl>
    };
}