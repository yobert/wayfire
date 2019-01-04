#version 100

attribute mediump vec3 position;
varying highp vec3 direction;

uniform mat4 cubeMapMatrix;

void main()
{
    gl_Position = cubeMapMatrix * vec4(position, 1.0);
    direction = position;
}
