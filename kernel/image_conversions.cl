__kernel void
rgba2Yuv(__global const uchar* rgba,
         __global uchar* Y,
         __global uchar* U,
         __global uchar* V,
         const uint width)
{
    size_t i = get_global_id(0);

    size_t x = i % width;
    size_t y = i / width;

    uchar r = rgba[i * 4];
    uchar g = rgba[i * 4 + 1];
    uchar b = rgba[i * 4 + 2];

    uint yValue = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
    Y[i] = (uchar)(clamp(yValue, 0u, 255u));

    uint vValue = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
    uint uValue = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;

    size_t halfWidth = width / 2;
    size_t index = halfWidth * (y / 2) + (x / 2) % halfWidth;
    if (x % 2 == 0) {
        if (y % 2 == 0) {
            U[index] = (uchar)(clamp(uValue, 0u, 255u));
        } else {
            V[index] = (uchar)(clamp(vValue, 0u, 255u));
        }
    }
}

void
scalePlane(__global const uchar* orig,
           const uint2 origSize,
           __global uchar* img,
           const uint2 size)
{
    size_t i = get_global_id(0);

    size_t x = i % size.x;
    size_t y = i / size.x;

    float xpercent = clamp((float)x / (float)size.x, 0.f, 1.f);
    float ypercent = clamp((float)y / (float)size.y, 0.f, 1.f);

    size_t ox = clamp((uint)(xpercent * origSize.x), 0u, origSize.x - 1u);
    size_t oy = clamp((uint)(ypercent * origSize.y), 0u, origSize.y - 1u);

    img[i] = orig[ox + oy * origSize.x];
}

__kernel void
scaleYUV(__global const uchar* inY,
         __global const uchar* inU,
         __global const uchar* inV,
         const uint2 inSize,
         __global uchar* outY,
         __global uchar* outU,
         __global uchar* outV,
         const uint2 outSize,
         const uint2 scale)
{
    scalePlane(inY, inSize, outY, outSize * scale.x / scale.y);
    scalePlane(inU, inSize, outU, (outSize / 4) * scale.x / scale.y);
    scalePlane(inV, inSize, outV, (outSize / 4) * scale.x / scale.y);
}