__kernel void
rgba2Yuv(__global const uchar* rgba,
         __global uchar* Y,
         __global uchar* U,
         __global uchar* V)
{
    int i = get_global_id(0);
    int width = get_global_size(0);

    int x = i % width;
    int y = i / width;

    uchar r = rgba[i * 4];
    uchar g = rgba[i * 4 + 1];
    uchar b = rgba[i * 4 + 2];

    uint yValue = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
    Y[i] = (uchar)(clamp(yValue, 0u, 255u));

    uint uValue = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
    uint vValue = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;

    int halfWidth = width / 2;
    int index = halfWidth * (y / 2) + (x / 2) % halfWidth;
    if (x % 2 == 0) {
        if (y % 2 == 0) {
            U[index] = (uchar)(clamp(uValue, 0u, 255u));
        } else {
            V[index] = (uchar)(clamp(vValue, 0u, 255u));
        }
    }
}