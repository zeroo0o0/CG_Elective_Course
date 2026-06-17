#version 430

layout (location = 0) in vec3 position;
layout (location = 1) in vec2 uv;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 bitangent;

out vec2 UV;
out vec3 Normal;
out vec3 FragPos;
out vec3 Tangent;
out vec3 Bitangent;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    FragPos = vec3(model * vec4(position, 1.0));
    Normal = mat3(transpose(inverse(model))) * normal;
    Tangent = mat3(transpose(inverse(model))) * tangent;
    Bitangent = mat3(transpose(inverse(model))) * bitangent;
    UV = uv;

    gl_Position = projection * view * vec4(FragPos, 1.0);
}
