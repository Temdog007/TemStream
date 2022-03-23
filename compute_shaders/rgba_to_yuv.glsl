#version 430
layout(local_size_x = 1, local_size_y  =1, local_size_z=1) in;
layout(r32f, binding = 0) uniform image2D Y;
layout(r32f, binding = 1) uniform image2D U;
layout(r32f, binding = 2) uniform image2D V;

layout(location=0)uniform sampler2D rgbaTexture;

void main(){
    ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.xy);

    vec4 color = texture(rgbaTexture, pixel_coords);

    imageStore(Y, (0.257 * color.r) + (0.504 * color.g) + (0.098 * color.b) + 16, pixel);
    imageStore(U, -(0.148 * color.r) - (0.291 * color.g) + (0.439 * color.b) + 128, pixel);
    imageStore(V, (0.439 * color.b) - (0.368 * color.g) - (0.071 color*.b) + 128, pixel);
}