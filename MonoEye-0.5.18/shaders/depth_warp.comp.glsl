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
    float ipd;                // Interpupillary distance in meters
    float nearZ;              // Near clip plane distance
    float farZ;               // Far clip plane distance
    float focalLength;        // Focal length
    uint hasDepthBuffer;      // 1 if depth buffer is available
    uint hasMotionBuffer;     // 1 if motion vectors are available
    uint qualityMode;         // 0=fast, 1=balanced, 2=quality
    uint showIndicator;       // 1 to show active indicator
    uint tensorEnabled;       // 1 to use tensor core logic
    uint specularRejection;   // 1 to remove flickering fireflies
    uint edgeSmoothing;       // 1 for depth-guided edge AA
    float upscaleFactor;      // Input resolution scale
    uint frameIndex;          // Frame counter
    uint hasUIOverlay;        // 1 if UI is active
} pc;

// Shared between invocations for temporal filtering
layout(binding = 3, set = 0, rgba8) uniform image2D previousFrame;
layout(binding = 4, set = 0) uniform sampler2D motionVectors;

// REQUIREMENT 6: Custom Overlay (Visual UI)
layout(binding = 5, set = 0) uniform sampler2D uiOverlay;

// High-quality Sharpening (RCAS inspired)
vec3 applySharpening(vec2 uv, vec3 centerColor, float depth) {
    float texelX = 1.0 / float(imageSize(rightEyeOutput).x);
    float texelY = 1.0 / float(imageSize(rightEyeOutput).y);
    
    vec3 n1 = texture(leftEyeColor, uv + vec2(texelX, 0)).rgb;
    vec3 n2 = texture(leftEyeColor, uv + vec2(-texelX, 0)).rgb;
    vec3 n3 = texture(leftEyeColor, uv + vec2(0, texelY)).rgb;
    vec3 n4 = texture(leftEyeColor, uv + vec2(0, -texelY)).rgb;
    
    // Contrast adaptive sharpening - Distance Aware
    // Distant objects (track) get higher sharpening, near objects (cockpit) stay natural
    float sharpness = 0.4 + (depth * 0.8); 
    
    vec3 avg = (n1 + n2 + n3 + n4) * 0.25;
    return mix(centerColor, centerColor + (centerColor - avg), sharpness);
}

// REQUIREMENT 5: Depth-Guided Bilateral Filter
vec3 bilateralSample(vec2 uv, vec3 baseColor, float baseDepth) {
    vec2 texelSize = 1.0 / vec2(imageSize(rightEyeOutput));
    vec3 result = baseColor;
    float totalWeight = 1.0;
    
    const int radius = 1;
    for(int x = -radius; x <= radius; x++) {
        for(int y = -radius; y <= radius; y++) {
            if(x == 0 && y == 0) continue;
            
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            vec3 neighborColor = texture(leftEyeColor, uv + offset).rgb;
            float neighborDepth = texture(leftEyeDepth, uv + offset).r;
            
            // Spatial weight (Gaussian)
            float d2 = float(x*x + y*y);
            float w_s = exp(-d2 / 2.0);
            
            // Range weight (Depth difference)
            // Hard edges (large depth diff) get low weight to prevent blurring across boundaries
            float w_r = exp(-abs(baseDepth - neighborDepth) * 50.0);
            
            float w = w_s * w_r;
            result += neighborColor * w;
            totalWeight += w;
        }
    }
    
    return result / totalWeight;
}

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
    float depthRaw = texture(leftEyeDepth, uv).r;
    
    // Apply sharpening if upscaling is active
    if (pc.upscaleFactor < 0.99) {
        leftColor = applySharpening(uv, leftColor, depthRaw);
    }

    // --- SPECULAR REJECTION (v3) ---
    if (pc.specularRejection == 1) {
        // Sample 4 neighbors to find high-frequency noise
        float texelX = 1.0 / float(imageSize.x);
        float texelY = 1.0 / float(imageSize.y);
        vec3 n1 = texture(leftEyeColor, uv + vec2(texelX, 0)).rgb;
        vec3 n2 = texture(leftEyeColor, uv + vec2(-texelX, 0)).rgb;
        vec3 n3 = texture(leftEyeColor, uv + vec2(0, texelY)).rgb;
        vec3 n4 = texture(leftEyeColor, uv + vec2(0, -texelY)).rgb;
        vec3 avg = (n1 + n2 + n3 + n4) * 0.25;
        
        // If current pixel is significantly brighter than average, clamp it
        float lum = dot(leftColor, vec3(0.299, 0.587, 0.114));
        float avgLum = dot(avg, vec3(0.299, 0.587, 0.114));
        if (lum > avgLum * 1.5) {
            leftColor = mix(leftColor, avg, 0.5);
        }
    }

    if (pc.hasDepthBuffer == 0) {
        // No depth buffer available - just copy left eye (pass-through)
        imageStore(rightEyeOutput, pixelCoord, vec4(leftColor, 1.0));
        return;
    }

    // Sample depth (assumes depth is stored in R channel, normalized [0, 1])
    float depthSample = texture(leftEyeDepth, uv).r;

    // Convert normalized depth to world distance
    // OpenXR depth format: depth values are in [nearZ, farZ]
    float depthWorld = pc.nearZ * pc.farZ / (pc.farZ - depthSample * (pc.farZ - pc.nearZ));
    
    // Safety check for depth
    depthWorld = max(depthWorld, pc.nearZ);
    
    // Calculate parallax shift for right eye
    // shift = IPD / depth (in normalized screen space)
    float parallaxShift = pc.ipd / depthWorld;

    // Convert to UV space (depends on FOV)
    // Simplified UV shift calculation
    float uvShift = parallaxShift * pc.focalLength;

    // Sample the left eye color at the shifted position
    vec2 shiftedUV = uv + vec2(uvShift, 0.0);

    // --- HIGH QUALITY RECONSTRUCTION (Tensor-inspired) ---
    vec3 finalColor;
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
            finalColor = bestColor;
        } else {
            // Valid sample - use bilateral filter for edge preservation
            finalColor = bilateralSample(shiftedUV, texture(leftEyeColor, shiftedUV).rgb, texture(leftEyeDepth, shiftedUV).r);
            
            if (pc.upscaleFactor < 0.99) {
                finalColor = applySharpening(shiftedUV, finalColor, texture(leftEyeDepth, shiftedUV).r);
            }
            
            // --- FSR 3.1 TEMPORAL ACCUMULATION ---
            if (pc.frameIndex > 0) {
                vec3 accumulatedColor;
                float motionWeight = 1.0;
                
                if (pc.hasMotionBuffer == 1) {
                    vec2 motion = texture(motionVectors, shiftedUV).rg;
                    ivec2 prevPixelCoord = pixelCoord - ivec2(motion * vec2(imageSize));
                    accumulatedColor = imageLoad(previousFrame, prevPixelCoord).rgb;
                    
                    // Motion-aware weight
                    float motionLen = length(motion);
                    motionWeight = clamp(1.0 - motionLen * 100.0, 0.1, 0.9);
                } else {
                    accumulatedColor = imageLoad(previousFrame, pixelCoord).rgb;
                    motionWeight = 0.5;
                }
                
                // Clipping history to current neighborhood
                vec3 m1 = vec3(0.0);
                vec3 m2 = vec3(0.0);
                for (int y = -1; y <= 1; y++) {
                    for (int x = -1; x <= 1; x++) {
                        vec3 c = texture(leftEyeColor, shiftedUV + vec2(x, y) / vec2(imageSize)).rgb;
                        m1 += c;
                        m2 += c * c;
                    }
                }
                vec3 mean = m1 / 9.0;
                vec3 stddev = sqrt(max(vec3(0.0), m2 / 9.0 - mean * mean));
                vec3 minColor = mean - stddev * 1.5;
                vec3 maxColor = mean + stddev * 1.5;
                accumulatedColor = clamp(accumulatedColor, minColor, maxColor);
                
                finalColor = mix(accumulatedColor, finalColor, 1.0 - motionWeight);
            }
        }
    } else {
        // Default fast/balanced modes
        if (shiftedUV.x >= 0.0 && shiftedUV.x <= 1.0) {
            finalColor = texture(leftEyeColor, shiftedUV).rgb;
        } else {
            finalColor = texture(leftEyeColor, vec2(clamp(shiftedUV.x, 0.0, 1.0), uv.y)).rgb;
        }

        if (pc.frameIndex > 0) {
            vec3 prevColor = imageLoad(previousFrame, pixelCoord).rgb;
            finalColor = mix(finalColor, prevColor, 0.7);
        }
    }

    // --- REQUIREMENT 6: UI COMPOSITING ---
    if (pc.hasUIOverlay == 1) {
        vec4 uiColor = texture(uiOverlay, uv);
        finalColor = mix(finalColor, uiColor.rgb, uiColor.a);
    }

    // Draw indicator
    if (pc.showIndicator == 1) {
        float dist = distance(vec2(pixelCoord), vec2(40.0, float(imageSize.y) - 40.0));
        if (dist < 8.0) {
            finalColor = vec3(0.0, 1.0, 0.0);
        }
    }

    imageStore(rightEyeOutput, pixelCoord, vec4(finalColor, 1.0));
    imageStore(previousFrame, pixelCoord, vec4(finalColor, 1.0));
}
