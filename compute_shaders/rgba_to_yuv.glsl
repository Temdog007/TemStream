#version 430
precision highp float;

layout(local_size_x = 1, local_size_y  =1, local_size_z=1) in;

layout(rgba8,location = 0) readonly uniform image2D rgbaTexture;

layout(location=1) writeonly uniform image2D Y;
layout(location=2) writeonly uniform image2D U;
layout(location=3) writeonly uniform image2D V;

void main(){
    ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.xy);

    vec4 color = imageLoad(rgbaTexture, pixel_coords);

    float y = (0.257 * color.r) + (0.504 * color.g) + (0.098 * color.b) + (16.0 / 256.0);
    imageStore(Y,pixel_coords, vec4(clamp(y, 0.0, 1.0)));

    // vec4 p1 = imageLoad(rgbaTexture, pixel_coords * ivec2(2, 1) + ivec2(0, 0));
    // vec4 p2 = imageLoad(rgbaTexture, pixel_coords * ivec2(2, 1) + ivec2(1, 0));
    // vec4 p3 = imageLoad(rgbaTexture, pixel_coords * ivec2(2, 1) + ivec2(0, 1));
    // vec4 p4 = imageLoad(rgbaTexture, pixel_coords * ivec2(2, 1) + ivec2(1, 1));

    // color = (p1+p2+p3+p4) * 0.25;

    float u = (-0.148 * color.r) + (-0.291 * color.g) + (0.439 * color.b) + 0.5;
    u = clamp(u, 0.0, 1.0);
    float v = (0.439 * color.r) + (-0.368 * color.g) + (-0.071 * color.b) + 0.5;
    v = clamp(v, 0.0, 1.0);

    ivec2 halfDim = ivec2(gl_NumWorkGroups.xy) / 2;
    int index = halfDim.x * (pixel_coords.y/2) + (pixel_coords.x/2) % halfDim.x;
    ivec2 fv = ivec2(index % halfDim.x, index / halfDim.x);
    if(pixel_coords.x % 2 == 0)
    {
        if(pixel_coords.y % 2 == 0)
        {
            imageStore(U, fv, vec4(v));
        }
        else
        {
            imageStore(V, fv, vec4(u));
        }
    }
}