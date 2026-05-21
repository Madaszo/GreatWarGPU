#version 460

struct Soldier {
    vec2 position;
    vec2 velocity;
    int team;
    int _pad;
};

layout(push_constant) uniform CameraPushConstants {
    vec2 center;
    vec2 scale;
    float pointSize;
} camera;

layout(std430, binding = 0) buffer SoldierBuffer {
    Soldier soldiers[];
};

layout(location = 0) out vec3 outColor;

void main() {
    Soldier soldier = soldiers[gl_VertexIndex];
    
    vec2 ndc = (soldier.position - camera.center) * camera.scale;

    gl_Position = vec4(ndc, 0.0, 1.0);
    gl_PointSize = clamp(camera.pointSize * 0.05, 1.0, 6.0);
    
    // Red team = vec3(1, 0, 0), Blue team = vec3(0, 0, 1)
    outColor = (soldier.team == 0) ? vec3(1.0, 0.2, 0.2) : vec3(0.2, 0.2, 1.0);
}
