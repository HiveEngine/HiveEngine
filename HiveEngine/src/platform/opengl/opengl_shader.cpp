//
// Created by lapor on 7/26/2024.
//


#ifdef HIVE_BACKEND_OPENGL
#include "opengl_shader.h"

#include <fstream>
#include <sstream>


#include "glad/glad.h"

namespace hive
{
    std::string readFile(const std::string &path)
    {
        std::ifstream shader_file;
        shader_file.open(path);

        if (!shader_file.is_open())
        {
            Logger::log("Unable to open shader file at path: " + path, LogLevel::Error);
            return "";
        }

        std::stringstream string_stream;
        string_stream << shader_file.rdbuf();
        shader_file.close();

        return string_stream.str();
    }

    unsigned int compile_shader(const std::string &source, const ShaderType type)
    {
        unsigned int id = 0;

        switch (type)
        {
            case Vertex:
                id = glCreateShader(GL_VERTEX_SHADER);
                break;
            case Fragment:
                id = glCreateShader(GL_FRAGMENT_SHADER);
                break;
        }

        const char *source_c_str = source.c_str();
        glShaderSource(id, 1, &source_c_str, nullptr);
        glCompileShader(id);

        return id;
    }

    unsigned int compile_program(unsigned int vertex_id, unsigned int frament_id)
    {
        const unsigned int program_id = glCreateProgram();

        glAttachShader(program_id, vertex_id);
        glAttachShader(program_id, frament_id);
        glLinkProgram(program_id);

        return program_id;
    }

    bool get_shader_compile_status(unsigned int shader_id)
    {
        int success;
        glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);

        if (!success)
            return false;

        return true;
    }


    void print_shader_error(unsigned int shader_id)
    {
        char infoLog[1024];
        glGetShaderInfoLog(shader_id, 1024, nullptr, infoLog);

        std::string str(infoLog);
        Logger::log("Shader error log: " + str, LogLevel::Error);
    }


    OpenglShader::OpenglShader(const std::string &vertex_path, const std::string &fragment_path)
    {
        const std::string vertexCode = readFile(vertex_path);
        const std::string fragmentCode = readFile(fragment_path);

        const unsigned int v_shader_id = compile_shader(vertexCode, Vertex);
        if (!get_shader_compile_status(v_shader_id))
        {
            Logger::log("Error unable to compile vertex shader at path: " + vertex_path, LogLevel::Error);
            print_shader_error(v_shader_id);
        }

        const unsigned int f_shader_id = compile_shader(fragmentCode, Fragment);
        if (!get_shader_compile_status(f_shader_id))
        {
            Logger::log("Error unable to compile fragment shader at path: " + fragment_path, LogLevel::Error);
            print_shader_error(f_shader_id);
        }

        const unsigned int id = compile_program(v_shader_id, f_shader_id);

        if (id == 0)
            Logger::log("Error unable to link the program", LogLevel::Error);

        glDeleteShader(v_shader_id);
        glDeleteShader(f_shader_id);

        program_id = id;
    }


    void OpenglShader::bind() const
    {
        glUseProgram(program_id);
    }

    void OpenglShader::unbind() const
    {
        glUseProgram(0);
    }

    void OpenglShader::uploadUniformInt(const std::string& name, int value) const
    {
        GLint location = glGetUniformLocation(program_id, name.c_str());
        glUniform1i(location, value);
    }

    void OpenglShader::uploadUniformFloat(const std::string& name, float value) const
    {
        GLint location = glGetUniformLocation(program_id, name.c_str());
        glUniform1f(location, value);
    }

    void OpenglShader::uploadUniformMat4(const std::string& name, const glm::mat4& matrix) const
    {
        GLint location = glGetUniformLocation(program_id, name.c_str());
        glUniformMatrix4fv(location, 1, GL_FALSE, &matrix[0][0]);
    }

    void OpenglShader::uploadUniformFloat4(const std::string& name, glm::vec4 value) const
    {
        GLint location = glGetUniformLocation(program_id, name.c_str());
        glUniform4f(location, value.x, value.y, value.z, value.w);
    }

}

#endif