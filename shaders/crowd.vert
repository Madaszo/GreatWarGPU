#version 460

struct Soldier {
    vec2 position;
    vec2 velocity;
    int team;
    int _pad;
};

layout(std430, binding = 0) buffer SoldierBuffer {
    Soldier soldiers[];
};

layout(location = 0) out vec3 outColor;

void main() {
    Soldier soldier = soldiers[gl_VertexIndex];
    
    gl_Position = vec4(soldier.position, 0.0, 1.0);
    gl_PointSize = 5.0;
    
    // Red team = vec3(1, 0, 0), Blue team = vec3(0, 0, 1)
    outColor = (soldier.team == 0) ? vec3(1.0, 0.2, 0.2) : vec3(0.2, 0.2, 1.0);
}
