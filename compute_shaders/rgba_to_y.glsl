#version 430
precision highp float;

layout(local_size_x = 1, local_size_y  =1, local_size_z=1) in;

layout(rgba8,location = 0) readonly uniform image2D rgbaTexture;

layout(location=1) writeonly uniform image2D Y;

void main(){
    ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.xy);

    vec4 color = imageLoad(rgbaTexture, pixel_coords);

    float y = (0.257 * color.r) + (0.504 * color.g) + (0.098 * color.b) + (16.0 / 256.0);
    imageStore(Y,pixel_coords, vec4(clamp(y, 0.0, 1.0)));
}