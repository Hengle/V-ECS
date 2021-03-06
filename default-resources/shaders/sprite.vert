#version 450

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec4 outTint;

layout(push_constant) uniform PushConsts {
    mat4 M;
    vec4 tint;
} pushConsts;

void main() {
    outUV = inUV;
    outTint = pushConsts.tint;
    gl_Position = pushConsts.M * vec4(inPos, 0, 1);
}
