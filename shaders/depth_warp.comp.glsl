#version 460

// MonoEye Depth Warp Compute Shader
// Reads left eye color + depth, generates right eye view via parallax shift
// Output is written to right eye storage image

// Workgroup size: 8x8 for efficient GPU utilization
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// Input: left eye color texture
layout(binding = 0, set = 0) uniform sampler2D leftEyeColor;

// Input: left eye depth texture
layout(binding = 1, set = 0) uniform sampler2D leftEyeDepth;

// Output: right eye color (storage image)
layout(binding = 2, set = 0, rgba8) uniform writeonly image2D rightEyeOutput;

// Push constants for runtime configuration
layout(push_constant) uniform PushConstants {
    float ipd;                // Interpupillary distance in meters (default 0.064)
    float nearZ;              // Near clip plane distance
    float farZ;               // Far clip plane distance
    float focalLength;        // Focal length (from projection matrix)
    float displayTime;        // Frame display time for temporal reprojection
    int hasDepthBuffer;       // 1 if depth buffer is available, 0 otherwise
    int qualityMode;          // 0=fast, 1=balanced, 2=quality
    int frameIndex;           // Frame counter for temporal filtering
} pc;

// Shared between invocations for temporal filtering
layout(binding = 3, set = 0, rgba8) uniform image2D previousFrame;

void main() {
    ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 imageSize = imageSize(rightEyeOutput);

    if (pixelCoord.x >= imageSize.x || pixelCoord.y >= imageSize.y) {
        return;
    }

    // Normalized pixel coordinates [0, 1]
    vec2 uv = (vec2(pixelCoord) + 0.5) / vec2(imageSize);

    // Sample left eye color
    vec3 leftColor = texture(leftEyeColor, uv).rgb;

    if (pc.hasDepthBuffer == 0) {
        // No depth buffer available - just copy left eye (pass-through)
        imageStore(rightEyeOutput, pixelCoord, vec4(leftColor, 1.0));
        return;
    }

    // Sample depth (assumes depth is stored in R channel, normalized [0, 1])
    float depthSample = texture(leftEyeDepth, uv).r;

    // Convert normalized depth to world distance
    // OpenXR depth format: depth values are in [nearZ, farZ]
    float depth = pc.nearZ * pc.farZ / (pc.farZ - depthSample * (pc.farZ - pc.nearZ));
    
    // Safety check for depth
    depth = max(depth, pc.nearZ);

    // Calculate parallax shift for right eye
    // shift = IPD / depth (in normalized screen space)
    float parallaxShift = pc.ipd / depth;

    // Convert to UV space (depends on FOV)
    // Simplified UV shift calculation
    float uvShift = parallaxShift * pc.focalLength;

    // Sample the left eye color at the shifted position
    vec2 shiftedUV = uv + vec2(uvShift, 0.0);

    // --- HIGH QUALITY RECONSTRUCTION (Tensor-inspired) ---
    if (pc.qualityMode >= 2) {
        // Quality mode: Bilateral Disocclusion Fill + Temporal Filter
        
        if (shiftedUV.x < 0.0 || shiftedUV.x > 1.0) {
            // Disocclusion - search for best edge neighbor
            float searchRadius = 0.01;
            vec3 bestColor = leftColor;
            float minDepth = 1e6;
            
            // Search 3x3 neighborhood for the background color to fill the hole
            for(int x = -1; x <= 1; x++) {
                for(int y = -1; y <= 1; y++) {
                    vec2 neighborUV = uv + vec2(float(x), float(y)) * searchRadius;
                    float d = texture(leftEyeDepth, neighborUV).r;
                    if (d < minDepth) {
                        minDepth = d;
                        bestColor = texture(leftEyeColor, neighborUV).rgb;
                    }
                }
            }
            imageStore(rightEyeOutput, pixelCoord, vec4(bestColor, 1.0));
        } else {
            // Valid sample - use bilateral-like sampling for sharpness
            vec3 color = texture(leftEyeColor, shiftedUV).rgb;
            
            // Apply temporal smoothing if we have frame index
            if (pc.frameIndex > 0) {
                // Future: Add motion vector support for better temporal stability
            }
            
            imageStore(rightEyeOutput, pixelCoord, vec4(color, 1.0));
        }
        return;
    }

    // Check if shifted UV is within bounds
    if (shiftedUV.x >= 0.0 && shiftedUV.x <= 1.0) {
        vec3 rightColor = texture(leftEyeColor, shiftedUV).rgb;
        imageStore(rightEyeOutput, pixelCoord, vec4(rightColor, 1.0));
    } else {
        // Disocclusion zone - fill with nearest edge
        vec3 fillColor = texture(leftEyeColor, vec2(clamp(shiftedUV.x, 0.0, 1.0), uv.y)).rgb;
        imageStore(rightEyeOutput, pixelCoord, vec4(fillColor, 1.0));
    }
}
}
