//
// Created by lapor on 7/19/2024.
//

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include "core/rendering/VertexBuffer.hpp"
#include "core/rendering/IndexBuffer.hpp"
#include "core/rendering/VertexArray.hpp"
#include "core/rendering/BufferUtils.h"
#include "platform/opengl/GLCheck.h"

unsigned int createBasicShader();
unsigned int createBlueShader();

int main(void)
{
    platform::WindowsWindow window = platform::WindowsWindow("Windows Window", 600, 700, core::WindowFlags::DEFAULT);
    Lypo::Shader *shader = new Lypo::OpenglShader("vertex.glsl", "fragment.glsl");

    float vertices[] = {
        -0.5f, -0.5f, 0.0f, // left
        0.5f, -0.5f, 0.0f, // right
        0.0f, 0.5f, 0.0f // top
    };

    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *) 0);
    glEnableVertexAttribArray(0);

    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress))
    {
        std::cout << "Error in glad load" << std::endl;
        return -1;
    }

    //from learnopengl.com
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    unsigned int shaderProgram = createBasicShader();
    unsigned int blueShader = createBlueShader();

    std::shared_ptr<Lypo::VertexArray> vertexArray;
    std::shared_ptr<Lypo::VertexArray> squareVA;

    vertexArray.reset(Lypo::VertexArray::Create());

    float vertices[3 * 7] = {
            -0.5f, -0.5f, 0.0f, 0.8f, 0.2f, 0.8f, 1.0f,
            0.5f, -0.5f, 0.0f, 0.2f, 0.3f, 0.8f, 1.0f,
            0.0f,  0.5f, 0.0f, 0.8f, 0.8f, 0.2f, 1.0f
    };

    std::shared_ptr<Lypo::VertexBuffer> vertexBuffer;
    vertexBuffer.reset(Lypo::VertexBuffer::Create(vertices, sizeof(vertices)));
    Lypo::BufferLayout layout = {
            { Lypo::ShaderDataType::Float3, "a_Position" },
            { Lypo::ShaderDataType::Float4, "a_Color" }
    };
    vertexBuffer->SetLayout(layout);

    vertexArray->AddVertexBuffer(vertexBuffer);

    uint32_t indices[3] = { 0, 1, 2 };
    std::shared_ptr<Lypo::IndexBuffer> indexBuffer;
    indexBuffer.reset(Lypo::IndexBuffer::Create(indices, sizeof(indices)));
    vertexArray->SetIndexBuffer(indexBuffer);

    squareVA.reset(Lypo::VertexArray::Create());

    float squareVertices[3 * 4] = {
            -0.75f, -0.75f, 0.0f,
            0.75f, -0.75f, 0.0f,
            0.75f,  0.75f, 0.0f,
            -0.75f,  0.75f, 0.0f
    };

    std::shared_ptr<Lypo::VertexBuffer> squareVB;
    squareVB.reset(Lypo::VertexBuffer::Create(squareVertices, sizeof(squareVertices)));
    squareVB->SetLayout({
                                { Lypo::ShaderDataType::Float3, "a_Position" }
                        });
    squareVA->AddVertexBuffer(squareVB);

    uint32_t squareIndices[6] = { 0, 1, 2, 2, 3, 0 };
    std::shared_ptr<Lypo::IndexBuffer> squareIB;
    squareIB.reset(Lypo::IndexBuffer::Create(squareIndices, sizeof(squareIndices)));
    squareVA->SetIndexBuffer(squareIB);


    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        /* Render here */
        //vertex buffer

        glClearColor(0.1f, 0.1f, 0.1f, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(blueShader);
        squareVA->Bind();
        glDrawElements(GL_TRIANGLES, squareVA->GetIndexBuffer()->GetCount(), GL_UNSIGNED_INT, nullptr);



        glUseProgram(shaderProgram);
        vertexArray->Bind();
        glDrawElements(GL_TRIANGLES, vertexArray->GetIndexBuffer()->GetCount(), GL_UNSIGNED_INT, nullptr);





        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

unsigned int createBasicShader()
{
    // Shader creation for test
    const char *vertexShaderSource = R"(
			#version 330 core

			layout(location = 0) in vec3 a_Position;
			layout(location = 1) in vec4 a_Color;

			out vec3 v_Position;
			out vec4 v_Color;

			void main()
			{
				v_Position = a_Position;
				v_Color = a_Color;
				gl_Position = vec4(a_Position, 1.0);
			}
		)";

    unsigned int vertexShader;
    vertexShader = glCreateShader(GL_VERTEX_SHADER);

    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    const char *fragmentShaderSource = R"(
			#version 330 core

			layout(location = 0) out vec4 color;

			in vec3 v_Position;
			in vec4 v_Color;

			void main()
			{
				color = vec4(v_Position * 0.5 + 0.5, 1.0);
				color = v_Color;
			}
		)";

    unsigned int fragmentShader;
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    unsigned int shaderProgram;
    shaderProgram = glCreateProgram();

    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glUseProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

unsigned int createBlueShader()
{
    // Shader creation for test
    const char *vertexShaderSource = R"(
			#version 330 core

			layout(location = 0) in vec3 a_Position;

			out vec3 v_Position;

			void main()
			{
				v_Position = a_Position;
				gl_Position = vec4(a_Position, 1.0);
			}
		)";

    unsigned int vertexShader;
    vertexShader = glCreateShader(GL_VERTEX_SHADER);

    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    const char *fragmentShaderSource = R"(
			#version 330 core

			layout(location = 0) out vec4 color;

			in vec3 v_Position;

			void main()
			{
				color = vec4(0.2, 0.3, 0.8, 1.0);
			}
		)";

    unsigned int fragmentShader;
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    unsigned int shaderProgram;
    shaderProgram = glCreateProgram();

    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glUseProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}
