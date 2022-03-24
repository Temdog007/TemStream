#version 430
precision highp float;

layout(local_size_x = 1, local_size_y  =1, local_size_z=1) in;

layout(rgba8,location = 0) readonly uniform image2D rgbaTexture;

layout(location=1) writeonly uniform image2D U;
layout(location=2) writeonly uniform image2D V;

void main(){
    ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.xy);

    vec4 p1 = imageLoad(rgbaTexture, pixel_coords * ivec2(2, 1) + ivec2(0, 0));
    vec4 p2 = imageLoad(rgbaTexture, pixel_coords * ivec2(2, 1) + ivec2(1, 0));
    vec4 p3 = imageLoad(rgbaTexture, pixel_coords * ivec2(2, 1) + ivec2(0, 1));
    vec4 p4 = imageLoad(rgbaTexture, pixel_coords * ivec2(2, 1) + ivec2(1, 1));

    vec4 color = (p1+p2+p3+p4) * 0.25;

    float u = (-0.148 * color.r) + (-0.291 * color.g) + (0.439 * color.b) + 0.5;
    float v = (0.439 * color.r) + (-0.368 * color.g) + (-0.071 * color.b) + 0.5;

    imageStore(U,pixel_coords, vec4(clamp(u, 0.0, 1.0)));
    imageStore(V,pixel_coords, vec4(clamp(v, 0.0, 1.0)));
}