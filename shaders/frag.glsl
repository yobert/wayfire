#version 100

varying highp vec2 uvpos;

uniform sampler2D smp;
uniform mediump vec4      color;

void main()
{
    mediump vec4 tex_color = texture2D(smp, uvpos);
    tex_color.rgb = tex_color.rgb * color.a;
    gl_FragColor = tex_color * color;
}
