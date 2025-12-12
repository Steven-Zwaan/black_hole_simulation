#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform mat4 invView;
uniform mat4 invProj;
uniform vec3 cameraPos;
uniform float r_s;

// Smooth "rubber sheet" depression - like a cloth sagging under weight
// Combines wide gentle slope with steep drop near the black hole
float clothHeight(float r) {
    float depth = 6.0 * r_s;
    // Wide gentle depression for overall sag
    float wideWidth = 8.0 * r_s;
    float wideGauss = exp(-r * r / (2.0 * wideWidth * wideWidth));
    // Sharp steep drop right near the black hole
    float steepWidth = 1.5 * r_s;
    float steepDepth = 3.0 * r_s;
    float steepGauss = exp(-r * r / (2.0 * steepWidth * steepWidth));
    return -depth * wideGauss - steepDepth * steepGauss;
}

// Derivative for surface normal
float clothDerivative(float r) {
    float depth = 6.0 * r_s;
    float wideWidth = 8.0 * r_s;
    float wideGauss = exp(-r * r / (2.0 * wideWidth * wideWidth));
    float steepWidth = 1.5 * r_s;
    float steepDepth = 3.0 * r_s;
    float steepGauss = exp(-r * r / (2.0 * steepWidth * steepWidth));
    return depth * r / (wideWidth * wideWidth) * wideGauss 
         + steepDepth * r / (steepWidth * steepWidth) * steepGauss;
}

void main() {
    // Ray generation from screen coordinates
    vec4 clipPos = vec4(TexCoord, -1.0, 1.0);
    vec4 viewPos = invProj * clipPos;
    viewPos /= viewPos.w;
    vec3 worldDir = normalize((invView * vec4(viewPos.xyz, 0.0)).xyz);
    
    vec3 rayOrigin = cameraPos;
    vec3 rayDir = worldDir;
    
    // Grid parameters - 5 black hole diameters wide (10 * r_s on each side)
    float gridSize = 1.0 * r_s;
    float gridLimit = 10.0 * r_s;
    
    // Ray-march to find grid LINE intersection (not just surface)
    float t = 0.0;
    float maxT = 300.0 * r_s;
    float stepSize = 0.2 * r_s;
    
    vec3 hitPos = rayOrigin;
    bool hit = false;
    float prevDiff = 0.0;
    bool first = true;
    
    for (int i = 0; i < 1500; i++) {
        t += stepSize;
        if (t > maxT) break;
        
        vec3 pos = rayOrigin + rayDir * t;
        float r = sqrt(pos.x * pos.x + pos.z * pos.z);
        
        // Skip if outside grid bounds
        if (abs(pos.x) > gridLimit || abs(pos.z) > gridLimit) {
            prevDiff = 0.0;
            first = true;
            continue;
        }
        
        float surfaceY = clothHeight(r);
        float diff = pos.y - surfaceY;
        
        // Check if ray crossed the surface (from either direction)
        if (!first && ((prevDiff > 0.0 && diff < 0.0) || (prevDiff < 0.0 && diff > 0.0))) {
            // Binary search for precise intersection
            float tLow = t - stepSize;
            float tHigh = t;
            for (int j = 0; j < 12; j++) {
                float tMid = (tLow + tHigh) * 0.5;
                vec3 midPos = rayOrigin + rayDir * tMid;
                float midR = sqrt(midPos.x * midPos.x + midPos.z * midPos.z);
                float midSurfaceY = clothHeight(midR);
                float midDiff = midPos.y - midSurfaceY;
                if ((prevDiff > 0.0 && midDiff < 0.0) || (prevDiff < 0.0 && midDiff > 0.0)) {
                    tHigh = tMid;
                } else {
                    tLow = tMid;
                }
            }
            vec3 candidateHit = rayOrigin + rayDir * (tLow + tHigh) * 0.5;
            
            // Check if this intersection is on a grid line
            float hx = candidateHit.x;
            float hz = candidateHit.z;
            float gridWidth = gridSize * 0.04;
            bool onGridX = mod(abs(hx), gridSize) < gridWidth;
            bool onGridZ = mod(abs(hz), gridSize) < gridWidth;
            
            if (onGridX || onGridZ) {
                // Found a grid line - use this hit
                hitPos = candidateHit;
                hit = true;
                break;
            }
            // Not on a grid line - continue marching to find next intersection
        }
        
        prevDiff = diff;
        first = false;
    }
    
    if (!hit) {
        discard;
    }
    
    float x = hitPos.x;
    float z = hitPos.z;
    float r = sqrt(x * x + z * z);
    
    // Calculate surface normal for lighting (flip if viewing from below)
    float deriv = clothDerivative(r);
    float dydx = deriv * (x / (r + 0.001));
    float dydz = deriv * (z / (r + 0.001));
    vec3 normal = normalize(vec3(-dydx, 1.0, -dydz));
    
    // Flip normal if ray hit from below
    if (dot(normal, rayDir) > 0.0) {
        normal = -normal;
    }
    
    // Lighting (works from both sides)
    vec3 lightDir = normalize(vec3(0.2, 1.0, 0.3));
    float diffuse = abs(dot(normal, lightDir));
    float ambient = 0.3;
    float lighting = ambient + 0.7 * diffuse;
    
    // Color: subtle gradient based on depth
    float depth = -hitPos.y / (9.0 * r_s);
    vec3 gridColor = mix(vec3(0.4, 0.4, 0.6), vec3(0.6, 0.25, 0.35), clamp(depth, 0.0, 1.0));
    
    FragColor = vec4(gridColor * lighting, 1.0);
}
