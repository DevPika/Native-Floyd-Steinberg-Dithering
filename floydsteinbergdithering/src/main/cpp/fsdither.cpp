//#include <jni.h>
//#include <string>
//
//extern "C"
//jstring
//Java_com_example_jeffreyliu_testcpp_MainActivity_stringFromJNI(
//        JNIEnv* env,
//        jobject /* this */) {
//    std::string hello = "Hello from C++";
//    return env->NewStringUTF(hello.c_str());
//}




#include <jni.h>

#include <android/log.h>
#include <android/bitmap.h>
#include <cmath>
//#include <cstdint>
//#include <vector>
//#include <iostream>
//#include <sstream>
//#include <string>


#define  LOG_TAG    "libimageprocessing"

#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

static uint32_t find_closest_palette_color(int value) {
    if (value < 128) {
        return 0xFF000000;
    }
    return 0xFFFFFFFF;
}

static void floydSteinberg(AndroidBitmapInfo *info, void *pixels) {
    int x, y, w = info->width, h = info->height;
    uint32_t *line;

    void *pixels2 = pixels;

    auto **d = new uint32_t *[info->height];
    for (int i = 0; i < info->height; ++i) {
        d[i] = new uint32_t[info->width];
    }

    for (y = 0; y < info->height; y++) {
        line = (uint32_t *) pixels2;
        for (x = 0; x < info->width; x++) {

            //extract the RGB values from the pixel
            d[y][x] = 0xFF000000 | (line[x] & 0x00FFFFFFu);
        }

        pixels2 = (char *) pixels2 + info->stride;
    }


    for (y = 0; y < info->height; y++) {
        line = (uint32_t *) pixels;
        for (x = 0; x < info->width; x++) {

            uint32_t oldpixel = d[y][x];
            uint32_t newpixel = find_closest_palette_color((int) oldpixel);

            // set the new pixel back in
            line[x] = newpixel;

            int err = (int) (oldpixel - newpixel);

            if (x + 1 < w)
                d[y][x + 1] = d[y][x + 1] + (int) (err * (7. / 16));
            if (x - 1 >= 0 && y + 1 < h)
                d[y + 1][x - 1] = d[y + 1][x - 1] + (int) (err * (3. / 16));
            if (y + 1 < h)
                d[y + 1][x] = d[y + 1][x] + (int) (err * (5. / 16));
            if (x + 1 < w && y + 1 < h)
                d[y + 1][x + 1] = d[y + 1][x + 1] + (int) (err * (1. / 16));
        }

        pixels = (char *) pixels + info->stride;
    }

    //Free each sub-array
    for (int i = 0; i < info->height; ++i) {
        delete[] d[i];
    }
    //Free the array of pointers
    delete[] d;
}

static void global_mono(AndroidBitmapInfo *info, void *pixels) {
    int x, y;
    uint32_t *line;
    for (y = 0; y < info->height; y++) {
        line = (uint32_t *) pixels;
        for (x = 0; x < info->width; x++) {
            line[x] = find_closest_palette_color((int) (line[x] & 0xFFu));
        }

        pixels = (char *) pixels + info->stride;
    }
}

// Helper function to calculate the squared difference between two values
static uint32_t sqDiff(uint8_t a, uint8_t b) {
    int16_t diff = static_cast<int16_t>(static_cast<int16_t>(a)-static_cast<int16_t>(b));
    return static_cast<uint32_t>(diff * diff);
}

static uint32_t find_closest_palette_color_bwr(uint32_t value) {
//    Grayscale 4 levels
//    if (value < 64) {
//        return 0xFF444444;
//    } else if (value < 128) {
//        return 0xFF888888;
//    } else if (value < 192) {
//        return 0xFFCCCCCC;
//    } else {
//        return 0xFFFFFFFF;
//    }

    uint8_t palette[3][3] = {{0, 0, 0}, {0xFF, 0xFF, 0xFF}, {0, 0, 0xFF}};
    uint32_t paletteFinal[3] = {0xFF000000, 0xFFFFFFFF, 0xFF0000FF};
    uint32_t color = 0;
    uint32_t best = UINT32_MAX;
    uint8_t b = (value & 0x00FF0000) >> 16;
    uint8_t g = (value & 0x0000FF00) >> 8;
    uint8_t r = (value & 0x000000FF);
    for (int i = 0; i < 3; ++i) {
        // Euclidean distance, but the square root part is removed
        uint64_t sum = sqDiff(b, palette[i][0]) +
                       sqDiff(g, palette[i][1]) +
                       sqDiff(r, palette[i][2]);
        sum = (sum >  UINT32_MAX) ? UINT32_MAX : sum;
        uint32_t dist = static_cast<uint32_t>(sum);
        if (dist < best) {
            if (dist == 0) {
                return paletteFinal[i];
            }
            color = paletteFinal[i];
            best = dist;
        }
    }
    return color;
}

static uint8_t clampedAdd(int64_t err0, uint8_t a) {
    int16_t err = 0;
    if (err0 >= 0)  err = (err0 <=  UINT8_MAX) ? err0 :  UINT8_MAX;
    else           err = (err0 >= - UINT8_MAX) ? err0 : - UINT8_MAX;
    if (err >= 0)  a = (a <=  UINT8_MAX - err) ? a + err :  UINT8_MAX;
    else           a = (a >= - err) ? a + err : 0x00;
    return static_cast<uint8_t>(a);
}

static uint32_t getChangedColor(uint32_t origColor, int16_t errB, int16_t errG, int16_t errR, float_t errFactor) {
    uint8_t b = static_cast<uint8_t>((origColor & 0x00FF0000) >> 16);
    uint8_t g = static_cast<uint8_t>((origColor & 0x0000FF00) >> 8);
    uint8_t r = static_cast<uint8_t>(origColor & 0x000000FF);
    uint8_t newB = clampedAdd((int64_t)(errB * errFactor), b);
    uint8_t newG = clampedAdd((int64_t)(errG * errFactor), g);
    uint8_t newR = clampedAdd((int64_t)(errR * errFactor), r);
    return static_cast<uint32_t>((0xFF << 24) | (newB << 16) | (newG << 8) | newR);
}

static uint8_t getB(uint32_t c) {
    return static_cast<uint8_t>((c & 0x00FF0000) >> 16);
}

static uint8_t getG(uint32_t c) {
    return static_cast<uint8_t>((c & 0x0000FF00) >> 8);
}

static uint8_t getR(uint32_t c) {
    return static_cast<uint8_t>(c & 0x000000FF);
}

static void floydSteinbergBWR(AndroidBitmapInfo *info, void *pixels) {
    int x, y, w = info->width, h = info->height;
    uint32_t *line;

    void *pixels2 = pixels;

    auto **d = new uint32_t *[info->height];
    for (int i = 0; i < info->height; ++i) {
        d[i] = new uint32_t[info->width];
    }

    for (y = 0; y < info->height; y++) {
        line = (uint32_t *) pixels2;
        for (x = 0; x < info->width; x++) {

            //extract the RGB values from the pixel
            d[y][x] = 0xFF000000 | (line[x] & 0x00FFFFFFu);
        }

        pixels2 = (char *) pixels2 + info->stride;
    }

    bool first = true;

    for (y = 0; y < info->height; y++) {
        line = (uint32_t *) pixels;
        for (x = 0; x < info->width; x++) {

            uint32_t oldpixel = d[y][x];
            uint32_t newpixel = find_closest_palette_color_bwr( oldpixel);

            // set the new pixel back in
            line[x] = newpixel;

            int16_t errB = getB(oldpixel) - getB(newpixel);
            int16_t errG = getG(oldpixel) - getG(newpixel);
            int16_t errR = getR(oldpixel) - getR(newpixel);

//            if (errB == 0 && errG == 0 && errR == 0) {
//                continue;
//            } else {
//                if (first) {
//                    first = false;
//                    LOGE("%d %d %d %d %d", errB, errG, errR, x, y);
//
//                    std::stringstream stream2;
//                    stream2 << std::hex << (int64_t)(errG * (7. / 16));
//                    LOGE("%s", stream2.str().c_str());
//
//                    std::stringstream stream;
//                    stream << std::hex << d[y][x + 1];
//                    LOGE("%s", stream.str().c_str());
//                }
//            }

            if (x + 1 < w)
                d[y][x + 1] = getChangedColor(d[y][x + 1], errB, errG, errR, 7. / 16);
            if (x - 1 >= 0 && y + 1 < h)
                d[y + 1][x - 1] = getChangedColor(d[y + 1][x - 1], errB, errG, errR, 3. / 16);
            if (y + 1 < h)
                d[y + 1][x] = getChangedColor(d[y + 1][x], errB, errG, errR, 5. / 16);
            if (x + 1 < w && y + 1 < h)
                d[y + 1][x + 1] = getChangedColor(d[y + 1][x + 1], errB, errG, errR, 1. / 16);
        }

        pixels = (char *) pixels + info->stride;
    }

    //Free each sub-array
    for (int i = 0; i < info->height; ++i) {
        delete[] d[i];
    }
    //Free the array of pointers
    delete[] d;
}

static void global_mono_bwr(AndroidBitmapInfo *info, void *pixels) {
    int x, y;
    uint32_t *line;
    for (y = 0; y < info->height; y++) {
        line = (uint32_t *) pixels;
        for (x = 0; x < info->width; x++) {
            line[x] = find_closest_palette_color_bwr(line[x] | 0xFF000000);
        }

        pixels = (char *) pixels + info->stride;
    }
}


#ifdef __cplusplus
extern "C" {
#endif

void Java_com_askjeffreyliu_floydsteinbergdithering_Utils_binaryBlackAndWhiteNative(JNIEnv *env,
                                                                                    jclass obj,
                                                                                    jobject bitmap) {
    AndroidBitmapInfo info;
    int ret;
    void *pixels;

    if ((ret = AndroidBitmap_getInfo(env, bitmap, &info)) < 0) {
        LOGE("AndroidBitmap_getInfo() failed ! error=%d", ret);
        return;
    }
    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        LOGE("Bitmap format is not RGBA_8888 !");
        return;
    }

    if ((ret = AndroidBitmap_lockPixels(env, bitmap, &pixels)) < 0) {
        LOGE("AndroidBitmap_lockPixels() failed ! error=%d", ret);
    }

    global_mono(&info, pixels);

    AndroidBitmap_unlockPixels(env, bitmap);
}

void Java_com_askjeffreyliu_floydsteinbergdithering_Utils_binaryBlackWhiteRedNative(JNIEnv *env,
                                                                                    jclass obj,
                                                                                    jobject bitmap) {
    AndroidBitmapInfo info;
    int ret;
    void *pixels;

    if ((ret = AndroidBitmap_getInfo(env, bitmap, &info)) < 0) {
        LOGE("AndroidBitmap_getInfo() failed ! error=%d", ret);
        return;
    }
    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        LOGE("Bitmap format is not RGBA_8888 !");
        return;
    }

    if ((ret = AndroidBitmap_lockPixels(env, bitmap, &pixels)) < 0) {
        LOGE("AndroidBitmap_lockPixels() failed ! error=%d", ret);
    }

    global_mono_bwr(&info, pixels);

    AndroidBitmap_unlockPixels(env, bitmap);
}

void Java_com_askjeffreyliu_floydsteinbergdithering_Utils_floydSteinbergNative(JNIEnv *env,
                                                                               jclass obj,
                                                                               jobject bitmap) {
    AndroidBitmapInfo info;
    int ret;
    void *pixels;

    if ((ret = AndroidBitmap_getInfo(env, bitmap, &info)) < 0) {
        LOGE("AndroidBitmap_getInfo() failed ! error=%d", ret);
        return;
    }
    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        LOGE("Bitmap format is not RGBA_8888 !");
        return;
    }

    if ((ret = AndroidBitmap_lockPixels(env, bitmap, &pixels)) < 0) {
        LOGE("AndroidBitmap_lockPixels() failed ! error=%d", ret);
    }

    floydSteinberg(&info, pixels);

    AndroidBitmap_unlockPixels(env, bitmap);
}

void Java_com_askjeffreyliu_floydsteinbergdithering_Utils_floydSteinbergBWRNative(JNIEnv *env,
                                                                               jclass obj,
                                                                               jobject bitmap) {
    AndroidBitmapInfo info;
    int ret;
    void *pixels;

    if ((ret = AndroidBitmap_getInfo(env, bitmap, &info)) < 0) {
        LOGE("AndroidBitmap_getInfo() failed ! error=%d", ret);
        return;
    }
    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        LOGE("Bitmap format is not RGBA_8888 !");
        return;
    }

    if ((ret = AndroidBitmap_lockPixels(env, bitmap, &pixels)) < 0) {
        LOGE("AndroidBitmap_lockPixels() failed ! error=%d", ret);
    }

    floydSteinbergBWR(&info, pixels);

    AndroidBitmap_unlockPixels(env, bitmap);
}

#ifdef __cplusplus
}
#endif

