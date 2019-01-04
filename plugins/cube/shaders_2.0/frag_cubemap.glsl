#version 100

varying highp vec3 direction;
uniform samplerCube smp;

void main()
{
    gl_FragColor = vec4(textureCube(smp, direction).xyz, 1);
}
