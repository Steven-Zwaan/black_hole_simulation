#version 330 core
out vec4 FragColor;

in vec3 WorldPos;
in vec3 Normal;

uniform vec3 color;

void main() {
    vec3 N = normalize(Normal);
    vec3 L = normalize(-WorldPos); // black hole at origin
    float diff = max(dot(N, L), 0.0);
    vec3 shaded = color * (0.1 + diff);
    FragColor = vec4(shaded, 1.0);
}
