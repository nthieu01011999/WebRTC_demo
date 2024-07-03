#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <CUnit/Basic.h>
#include "vvtk_video.h"

void testAssert_video_config_value(void)
{

}

void dump_video(char *data, int data_len, int stream_index, int frame_index)
{
    char filename[64];
    sprintf(filename, "/tmp/video_%d_%d.out", stream_index, frame_index);
    FILE *fp = fopen(filename, "wb");

    if(fp) 
    {
        fwrite(data, 1, data_len, fp);
        fclose(fp);
    }
}

void dump_video2(char *data, int data_len)
{
    FILE *fp = fopen("/tmp/video.out", "a+b");

    if(fp) 
    {
        fwrite(data, 1, data_len, fp);
        fclose(fp);
    }
}

#define DUMP_FILE 0

static int frame_index = 0;
static int stream_index;
VVTK_RET_CALLBACK video_callback(vvtk_video_frame_t *video_frame, const void *user_data)
{
    // printf("\nwidth:%d height:%d\n", video_frame->width, video_frame->height);
    // printf("timestamp:%llu\n", video_frame->timestamp);
    // printf("data:%p size:%d\n", video_frame->data, video_frame->size);
    // printf("sequence:%d\n", video_frame->sequence);
    // printf("type:%d\n", video_frame->type);
    // printf("index:%d\n", video_frame->index);

    CU_ASSERT(video_frame->size > 0);

#if DUMP_FILE
    dump_video(video_frame->data, video_frame->size, stream_index, frame_index);
    // dump_video2(video_frame->data, video_frame->size);
#endif

    if(frame_index++<15)
        return VVTK_RET_CALLBACK_CONTINUE;
    else
        return VVTK_RET_CALLBACK_STOP;
}

void test_vvtk_video_callback(void)
{
    frame_index = 0;
    sleep(2);
    CU_ASSERT(0==vvtk_set_video_callback(0, video_callback, NULL));
    sleep(5);
    frame_index = 0;
    stream_index++;
    CU_ASSERT(0==vvtk_set_video_callback(1, video_callback, NULL));
    sleep(5);
}

void test_vvtk_video_config(void)
{
    vvtk_video_config_t config;

    CU_ASSERT(0==vvtk_get_video_config(0, &config));

    config.width = 1280;
    config.height = 720;
    CU_ASSERT(0==vvtk_set_video_config(0, &config));
    sleep(2);

    CU_ASSERT(0==vvtk_get_video_config(0, &config));

    config.frame_rate = 20;
    CU_ASSERT(0==vvtk_set_video_config(0, &config));
    sleep(2);

    CU_ASSERT(0==vvtk_get_video_config(0, &config));

    config.encoding_mode = VVTK_VIDEO_ENCODING_MODE_CBR;
    config.bitrate_max = 2000000;
    CU_ASSERT(0==vvtk_set_video_config(0, &config));
    sleep(2);

    CU_ASSERT(0==vvtk_get_video_config(0, &config));

    config.encoding_mode = VVTK_VIDEO_ENCODING_MODE_VBR;
    config.bitrate_max = 2000000;
    config.bitrate_min = 1500000;
    CU_ASSERT(0==vvtk_set_video_config(0, &config));
    sleep(2);

    CU_ASSERT(0==vvtk_get_video_config(0, &config));


    config.codec = VVTK_VIDEO_CODEC_H264;
    CU_ASSERT(0==vvtk_set_video_config(0, &config));
    frame_index = 0;
    stream_index++;
    sleep(2);
    CU_ASSERT(0==vvtk_set_video_callback(0, video_callback, NULL));
    sleep(5);


    config.codec = VVTK_VIDEO_CODEC_H265;
    CU_ASSERT(0==vvtk_set_video_config(0, &config));
    frame_index = 0;
    stream_index++;
    sleep(2);
    CU_ASSERT(0==vvtk_set_video_callback(0, video_callback, NULL));
    sleep(5);


}

#define DUMP_JPEG_FILE 0
void dump_jpeg_frame(vvtk_jpeg_image_t *image)
{
	FILE *fp = NULL;
	char filename[20];

	sprintf(filename, "/tmp/frame.jpg");

	fp = fopen(filename, "wb");
	if(fp)
	{
		fwrite(image->data, 1, image->size, fp);
		fclose(fp);
	}
}

void test_vvtk_video_jpeg_image(void)
{
    vvtk_video_config_t video_config;
	vvtk_jpeg_config_t jpeg_config;
	vvtk_jpeg_image_t image = {0};

    CU_ASSERT(0==vvtk_get_video_config(0, &video_config));
	jpeg_config.width = video_config.width;
	jpeg_config.height = video_config.height;
	jpeg_config.quality = 100;
    CU_ASSERT(0==vvtk_get_jpeg_image(&jpeg_config, &image));
    CU_ASSERT(image.size>0);
#if DUMP_JPEG_FILE
	if(image.size)
		dump_jpeg_frame(&image);
#endif
    CU_ASSERT(0==vvtk_free_jpeg_image(&image));

    CU_ASSERT(0==vvtk_get_video_config(1, &video_config));
	jpeg_config.width = video_config.width;
	jpeg_config.height = video_config.height;
	jpeg_config.quality = 100;
    CU_ASSERT(0==vvtk_get_jpeg_image(&jpeg_config, &image));
    CU_ASSERT(image.size>0);
    CU_ASSERT(0==vvtk_free_jpeg_image(&image));
}