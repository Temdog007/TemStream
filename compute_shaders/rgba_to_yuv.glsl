#version 430
layout(local_size_x = 1, local_size_y  =1, local_size_z=1) in;

layout(rgba8,location = 0) readonly uniform image2D rgbaTexture;

layout(location=1) writeonly uniform image2D Y;
layout(location=2) writeonly uniform image2D U;
layout(location=3) writeonly uniform image2D V;


void main(){
    ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.xy);

    vec4 color = imageLoad(rgbaTexture, pixel_coords);

    imageStore(Y,pixel_coords, vec4((0.257 * color.r) + (0.504 * color.g) + (0.098 * color.b)));
    imageStore(U,pixel_coords, vec4(-(0.148 * color.r) - (0.291 * color.g) + (0.439 * color.b)));
    imageStore(V,pixel_coords, vec4((0.439 * color.r) - (0.368 * color.g) - (0.071 *color.b)));
}