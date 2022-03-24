#version 430
layout(local_size_x = 1, local_size_y  =1, local_size_z=1) in;

layout(rgba8,location = 0) readonly uniform image2D rgbaTexture;

layout(location=1) writeonly uniform image2D Y;
layout(location=2) writeonly uniform image2D U;
layout(location=3) writeonly uniform image2D V;


void main(){
    ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.xy);

    vec4 color = imageLoad(rgbaTexture, pixel_coords);

    imageStore(Y,pixel_coords, vec4((0.2568 * color.r) + (0.5041 * color.g) + (0.0979 * color.b)) + (1.0 / 16.0));
    bool goodX = pixel_coords.x < (gl_NumWorkGroups.x+1) / 2;
    bool goodY =  pixel_coords.y < (gl_NumWorkGroups.y+1) / 2;

    vec4 avg;
    if(goodX){
        if(goodY){
            vec4 p1 = imageLoad(rgbaTexture, pixel_coords * ivec2(2,1) + ivec2(0,0));
            vec4 p2 = imageLoad(rgbaTexture, pixel_coords * ivec2(2,1) + ivec2(1,0));
            vec4 p3 = imageLoad(rgbaTexture, pixel_coords * ivec2(2,1) + ivec2(0,1));
            vec4 p4 = imageLoad(rgbaTexture, pixel_coords * ivec2(2,1) + ivec2(1,1));
            avg = (p1+p2+p3+p4) / 4.0;
        }
        else{
            vec4 p1 = imageLoad(rgbaTexture, pixel_coords * ivec2(2,1) + ivec2(0,0));
            vec4 p2 = imageLoad(rgbaTexture, pixel_coords * ivec2(2,1) + ivec2(1,0));
            avg = (p1+p2) / 2.0;
        }
    }
    else if(goodY) {
        vec4 p3 = imageLoad(rgbaTexture, pixel_coords * ivec2(2,1) + ivec2(0,1));
        vec4 p4 = imageLoad(rgbaTexture, pixel_coords * ivec2(2,1) + ivec2(1,1));
        avg = (p3+p4) / 2.0;
    }

    if(goodX || goodY){
        imageStore(U,pixel_coords, vec4((-0.1482 * avg.r) + (-0.2910 * avg.g) + (0.4392 * avg.b)) + 0.5);
        imageStore(V,pixel_coords, vec4((0.4392 * avg.r) + (-0.3678 * avg.g) + (-0.0714 *avg.b)) + 0.5);
    }
}