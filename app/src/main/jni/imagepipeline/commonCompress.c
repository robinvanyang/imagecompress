/*
 * Copyright 2014 http://Bither.net
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include <android/bitmap.h>
#include <android/log.h>
#include <jni.h>
#include <stdio.h>
#include <setjmp.h>
#include <math.h>
#include <stdint.h>
#include <time.h>
#include "jpeglib.h"
#include "cdjpeg.h"
#include "jversion.h"
#include "logging.h"


#define true 1
#define false 0
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

typedef uint8_t  BYTE;

char *error;

struct my_error_mgr {
    struct jpeg_error_mgr pub;
    jmp_buf  setjmp_buffer;
};

typedef struct my_error_mgr *my_error_ptr;

METHODDEF(void)
my_error_exit (j_common_ptr cinfo)
{
    my_error_ptr myerr = (my_error_ptr) cinfo->err;
    (*cinfo->err->output_message) (cinfo);
    error=myerr->pub.jpeg_message_table[myerr->pub.msg_code];
    LOGE("jpeg_message_table[%d]:%s", myerr->pub.msg_code,myerr->pub.jpeg_message_table[myerr->pub.msg_code]);
    longjmp(myerr->setjmp_buffer, 1);
}

static int generateJPEG(BYTE* data, int w, int h, int quality,
                 const char* outfilename, jboolean optimize) {
    int nComponent = 3;

    struct jpeg_compress_struct jcs;

    struct my_error_mgr jem;

    jcs.err = jpeg_std_error(&jem.pub);
    jem.pub.error_exit = my_error_exit;
    if (setjmp(jem.setjmp_buffer)) {
        return 0;
    }
    jpeg_create_compress(&jcs);
    FILE* f = fopen(outfilename, "wb");
    if (f == NULL) {
        return 0;
    }
    jpeg_stdio_dest(&jcs, f);
    jcs.image_width = w;
    jcs.image_height = h;
    if (optimize) {
        LOGI("optimize==ture");
    } else {
        LOGI("optimize==false");
    }

    jcs.arith_code = false;
    jcs.input_components = nComponent;
    if (nComponent == 1)
        jcs.in_color_space = JCS_GRAYSCALE;
    else
        jcs.in_color_space = JCS_RGB;

    jpeg_set_defaults(&jcs);
    jcs.optimize_coding = optimize;
    jpeg_set_quality(&jcs, quality, true);

    jpeg_start_compress(&jcs, TRUE);

    JSAMPROW row_pointer[1];
    int row_stride;
    row_stride = jcs.image_width * nComponent;
    while (jcs.next_scanline < jcs.image_height) {
        row_pointer[0] = &data[jcs.next_scanline * row_stride];

        jpeg_write_scanlines(&jcs, row_pointer, 1);
    }

    if (jcs.optimize_coding) {
        LOGI("optimize==ture");
    } else {
        LOGI("optimize==false");
    }
    jpeg_finish_compress(&jcs);
    jpeg_destroy_compress(&jcs);
    fclose(f);

    return 1;
}

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb;

char* jstrinTostring(JNIEnv* env, jbyteArray barr) {
    char* rtn = NULL;
    jsize alen = (*env)->GetArrayLength(env, barr);
    jbyte* ba = (*env)->GetByteArrayElements(env, barr, 0);
    if (alen > 0) {
        rtn = (char*) malloc(alen + 1);
        memcpy(rtn, ba, alen);
        rtn[alen] = 0;
    }
    (*env)->ReleaseByteArrayElements(env, barr, ba, 0);
    return rtn;
}

static jbyteArray stoJstring(JNIEnv* env, const char* pat,int len) {
    jbyteArray bytes = (*env)->NewByteArray(env, len);
    (*env)->SetByteArrayRegion(env, bytes, 0, len,  pat);
    jsize alen = (*env)->GetArrayLength(env, bytes);
    return bytes;
}

static jstring Compressor_compressBitmap(
    JNIEnv* env,
    jclass thiz,
    jobject bitmapcolor,
    int w,
    int h,
    int quality,
    jbyteArray fileNameStr,
    jboolean optimize) {

    AndroidBitmapInfo infocolor;
    BYTE* pixelsColor;
    int ret;
    BYTE * data;
    BYTE *tmpdata;
    char * fileName = jstrinTostring(env, fileNameStr);
    if ((ret = AndroidBitmap_getInfo(env, bitmapcolor, &infocolor)) < 0) {
        LOGE("AndroidBitmap_getInfo() failed ! error=%d", ret);
        return (*env)->NewStringUTF(env, "0");;
    }
    int format = infocolor.format;
    LOGE("Bitmap format: %d", format);

    if ((ret = AndroidBitmap_lockPixels(env, bitmapcolor,(void**)&pixelsColor)) < 0) {
        LOGE("AndroidBitmap_lockPixels() failed ! error=%d", ret);
    }

    BYTE r, g, b;
    data = NULL;
    data = malloc(w * h * 3);
    tmpdata = data;
    int j = 0, i = 0;
    int color;
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            color = *((int *) pixelsColor);
            b = ((color & 0x00FF0000) >> 16);
            g = ((color & 0x0000FF00) >> 8);
            r = color & 0x000000FF;
            *data = r;
            *(data + 1) = g;
            *(data + 2) = b;
            data = data + 3;
            pixelsColor += 4;
        }
    }
    AndroidBitmap_unlockPixels(env, bitmapcolor);
    int resultCode= generateJPEG(tmpdata, w, h, quality, fileName, optimize);
    free(tmpdata);

    if(resultCode==0){
        jstring result=(*env)->NewStringUTF(env, error);
        error=NULL;
        return result;
    }
    return (*env)->NewStringUTF(env, "1"); //success
}

static JNINativeMethod gCompressorNativeMethods[] = {
  {
    "nativeCompressBitmap",
    "(Landroid/graphics/Bitmap;III[BZ)Ljava/lang/String;",
    (void*) Compressor_compressBitmap
  },
};

jint registerCompressorNativeMethods(JNIEnv* env) {
  jclass compressor_class = (*env)->FindClass(
    env,
    "imagecompress/Compressor");

  if(!compressor_class) {
    return JNI_ERR;
  }

  jint result = (*env)->RegisterNatives(
    env,
    compressor_class,
    gCompressorNativeMethods,
    ARRAY_SIZE(gCompressorNativeMethods));
  if(result != JNI_OK) {
    return JNI_ERR;
  }

  return JNI_VERSION_1_6;
}
