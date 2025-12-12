#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform mat4 invView;
uniform mat4 invProj;
uniform vec3 cameraPos;
uniform float r_s;
uniform float c;
uniform int numBodies;

struct Body {
    vec3 position;
    float radius;
    vec3 color;
};

uniform Body bodies[10];

const int MAX_STEPS = 500;

// Accretion disk parameters (in units of r_s)
const float DISK_INNER = 3.0;   // Inner edge at ISCO (innermost stable circular orbit)
const float DISK_OUTER = 8.0;   // Outer edge
const float DISK_HEIGHT = 0.1;  // Half-thickness

// Accretion disk color based on radius (darker orange near center, brighter orange at edges)
vec3 diskColor(float r) {
    float t = (r - DISK_INNER) / (DISK_OUTER - DISK_INNER);
    t = clamp(t, 0.0, 1.0);
    // #DB351B (dark orange inside) to #FA882C (bright orange outside)
    vec3 darkOrange = vec3(0.859, 0.208, 0.106);   // #DB351B
    vec3 brightOrange = vec3(0.98, 0.533, 0.173);  // #FA882C
    return mix(darkOrange, brightOrange, t);
}

void main() {
    // Ray generation from screen coordinates - FOR EVERY PIXEL/FRAGMENT
    vec4 clipPos = vec4(TexCoord, -1.0, 1.0);
    vec4 viewPos = invProj * clipPos;
    viewPos /= viewPos.w;
    vec3 worldDir = normalize((invView * vec4(viewPos.xyz, 0.0)).xyz);
    
    // Normalize to units of r_s for numerical stability
    float scale = r_s;
    
    // Ray position and velocity in Cartesian coordinates (normalized units)
    vec3 pos = cameraPos / scale;
    vec3 vel = worldDir;  // Unit direction
    
    // Adaptive base step
    float base_dt = 0.3;
    
    // RK4 integration using Cartesian geodesic equations
    // This avoids the polar singularity of spherical coordinates
    for (int i = 0; i < MAX_STEPS; i++) {
        float r = length(pos);
        
        // RAY HITS BLACK HOLE when it falls inside event horizon
        if (r < 1.0) {  // r_s normalized to 1
            FragColor = vec4(0.0, 0.0, 0.0, 1.0);
            return;
        }
        
        // Check if ray hits any body (convert back to world units)
        vec3 worldPos = pos * scale;
        for (int j = 0; j < numBodies && j < 10; ++j) {
            float distToBody = length(worldPos - bodies[j].position);
            if (distToBody < bodies[j].radius) {
                FragColor = vec4(bodies[j].color, 1.0);
                return;
            }
        }
        
        // Check if ray crosses the accretion disk (y=0 plane, between inner and outer radius)
        // Track previous y to detect crossing
        float prevY = pos.y;
        
        // Adaptive step size: smaller near the black hole
        float dt = base_dt * clamp((r - 1.0) * 0.5, 0.02, 2.0);
        
        // Cartesian geodesic acceleration for null rays in Schwarzschild spacetime
        // Using the formula: a = -1.5 * (h²/r⁵) * pos
        // where h = |pos × vel| is the specific angular momentum magnitude
        vec3 h_vec = cross(pos, vel);
        float h2 = dot(h_vec, h_vec);
        float r2 = r * r;
        float r5 = r2 * r2 * r;
        
        // Acceleration (geometric units, r_s = 1, so GM = 0.5)
        vec3 accel = -1.5 * h2 / r5 * pos;
        
        // RK4 integration
        // k1
        vec3 k1_pos = vel;
        vec3 k1_vel = accel;
        
        // k2
        vec3 pos2 = pos + 0.5 * dt * k1_pos;
        vec3 vel2 = vel + 0.5 * dt * k1_vel;
        float r2_k2 = length(pos2);
        vec3 h2_vec = cross(pos2, vel2);
        float h2_2 = dot(h2_vec, h2_vec);
        float r5_2 = r2_k2 * r2_k2 * r2_k2 * r2_k2 * r2_k2;
        vec3 accel2 = -1.5 * h2_2 / r5_2 * pos2;
        vec3 k2_pos = vel2;
        vec3 k2_vel = accel2;
        
        // k3
        vec3 pos3 = pos + 0.5 * dt * k2_pos;
        vec3 vel3 = vel + 0.5 * dt * k2_vel;
        float r2_k3 = length(pos3);
        vec3 h3_vec = cross(pos3, vel3);
        float h2_3 = dot(h3_vec, h3_vec);
        float r5_3 = r2_k3 * r2_k3 * r2_k3 * r2_k3 * r2_k3;
        vec3 accel3 = -1.5 * h2_3 / r5_3 * pos3;
        vec3 k3_pos = vel3;
        vec3 k3_vel = accel3;
        
        // k4
        vec3 pos4 = pos + dt * k3_pos;
        vec3 vel4 = vel + dt * k3_vel;
        float r2_k4 = length(pos4);
        vec3 h4_vec = cross(pos4, vel4);
        float h2_4 = dot(h4_vec, h4_vec);
        float r5_4 = r2_k4 * r2_k4 * r2_k4 * r2_k4 * r2_k4;
        vec3 accel4 = -1.5 * h2_4 / r5_4 * pos4;
        vec3 k4_pos = vel4;
        vec3 k4_vel = accel4;
        
        // Update position and velocity
        vec3 newPos = pos + dt * (k1_pos + 2.0*k2_pos + 2.0*k3_pos + k4_pos) / 6.0;
        vel += dt * (k1_vel + 2.0*k2_vel + 2.0*k3_vel + k4_vel) / 6.0;
        vel = normalize(vel);  // Keep velocity normalized for light rays
        
        // Check if ray crossed the disk plane (y=0)
        if ((prevY > 0.0 && newPos.y < 0.0) || (prevY < 0.0 && newPos.y > 0.0)) {
            // Find intersection point with y=0 plane
            float tPlane = -pos.y / (newPos.y - pos.y);
            vec3 diskHit = pos + tPlane * (newPos - pos);
            float diskR = length(vec2(diskHit.x, diskHit.z));
            
            // Check if within disk bounds
            if (diskR >= DISK_INNER && diskR <= DISK_OUTER) {
                vec3 color = diskColor(diskR);
                FragColor = vec4(color, 1.0);
                return;
            }
        }
        
        pos = newPos;
        
        // If ray escapes too far, stop tracing
        if (r > 100.0) {
            break;
        }
    }
    
    // Ray didn't hit anything, let grid show through
    discard;
}
