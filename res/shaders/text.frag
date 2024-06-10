#version 450

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec4 outColor;

vec4 quadratic_ps(vec2 p, vec4 color, bool is_concave) {
    vec2 px = dFdx(p);
    vec2 py = dFdy(p);

    float fx = (2 * p.x) * px.x - px.y;
    float fy = (2 * p.x) * py.x - py.y;

    float sd = (p.x * p.x - p.y) / sqrt(fx * fx + fy * fy);

    float alpha = 0.5 - sd;
    if (is_concave) {
        alpha *= -1;
    }
    if (alpha > 1) {
        color.a = 1;
    } else if (alpha < 0) {
        discard;
    } else {
        color.a = alpha;
    }
    return color;
}

void main() {
    outColor = quadratic_ps(inUV, vec4(inColor, 1.0), false);
}
