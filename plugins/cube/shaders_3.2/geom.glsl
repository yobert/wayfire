#version 320 es

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

in vec2 tesuv[3];
in vec3 tePosition[3];

uniform int  light;

out vec2 guv;
out vec3 colorFactor;

#define AL 0.3    // ambient lighting
#define DL (1.0-AL) // diffuse lighting

void main() {

    const vec3 lightSource = vec3(0, 0, 2);
    const vec3 lightNormal = normalize(lightSource);

    if(light == 1) {
        vec3 A = tePosition[2] - tePosition[0];
        vec3 B = tePosition[1] - tePosition[0];
        vec3 N = normalize(cross(A, B));

        vec3 center = (tePosition[0] + tePosition[1] + tePosition[2]) / 3.0;

        float d = distance(center, lightSource);
        float ambient_coeff = pow(clamp(2.0 / d, 0.0, 1.0), 10.0);

        float value = clamp(pow(abs(dot(N, lightNormal)), 1.5), 0.0, 1.0);

        float df = AL * ambient_coeff + DL * value;
        colorFactor = vec3(df, df, df);
    }
    else
        colorFactor = vec3(1.0, 1.0, 1.0);

    gl_Position = gl_in[0].gl_Position;
    guv = tesuv[0];
    EmitVertex();

    gl_Position = gl_in[1].gl_Position;
    guv = tesuv[1];
    EmitVertex();

    gl_Position = gl_in[2].gl_Position;
    guv = tesuv[2];
    EmitVertex();
}
