#include <iostream>

#ifdef  __cplusplus 
extern "C" {
#endif //  __cplusplus

#include <libavformat\avformat.h> 
#include <libavcodec\avcodec.h> 
#include <libavutil\time.h>

#ifdef __cplusplus 
}
#endif

using namespace std;

AVFormatContext* inputContext = NULL;
AVFormatContext* outputContext = NULL;

int64_t lastReadPacktTime = 0;

static int interrupt_cb(void *ctx)
{
	int  timeout = 10;  //10秒超时
	if (av_gettime() - lastReadPacktTime > timeout * 1000 * 1000)
	{
		cout << "timeout" << endl;
		return -1;
	}
	return 0;
}

int openInput(char* url) {
	inputContext = avformat_alloc_context();
	//AVDictionary* options = NULL;
	//av_dict_set(&options, "rtsp_transport", "udp", 0);
	lastReadPacktTime = av_gettime();
	inputContext->interrupt_callback.callback = interrupt_cb;

	int ret = avformat_open_input(&inputContext, url, 0, 0);
	if (ret != 0) {
		char buf[1024] = { 0 };
		av_strerror(ret, buf, sizeof(buf));
		printf("FFDemux avformat_open_input %s , failed!，%s");
		return ret;
	}

	//读取文件信息
	ret = avformat_find_stream_info(inputContext, 0);
	if (ret != 0) {
		char buf[1024] = { 0 };
		av_strerror(ret, buf, sizeof(buf));
		printf("FFDemux avformat_find_stream_info %s , failed!", buf);
		return ret;
	}

	printf("open success\n");

	return ret;

}

int openOutput(char* url) {
	// flv rtmp流 ，mpegts裸流
	int ret = avformat_alloc_output_context2(&outputContext, NULL, "mpegts", url);
	//int ret = avformat_alloc_output_context2(&outputContext,NULL,"flv",dstFilePath);

	if (ret < 0) {
		char buf[1024] = { 0 };
		av_strerror(ret, buf, sizeof(buf));
		printf("FFDemux avformat_alloc_output_context2 %s , failed!", buf);
		return ret;
	}
	ret = avio_open2(&outputContext->pb, url, AVIO_FLAG_WRITE, 0, 0);
	if (ret < 0) {
		printf("avio_open2 failed!");
		return ret;
	}

	for (int i = 0; i<inputContext->nb_streams; i++) {
		AVStream* stream = avformat_new_stream(outputContext, NULL);
		if (!stream) {
			printf("avformat_new_stream failed");
		}
		ret = avcodec_parameters_copy(outputContext->streams[i]->codecpar, inputContext->streams[i]->codecpar);
		if (ret < 0)
		{
			printf("copy coddec context failed");
			goto Error;
		}
	}

	ret = avformat_write_header(outputContext, nullptr);
	if (ret < 0)
	{
		printf("format write header failed");
		goto Error;
	}

	printf(" Open output file success %s", url);
	return ret;
Error:
	if (outputContext)
	{
		avformat_close_input(&outputContext);
	}
	return ret;
}

void closeInput() {
	if (inputContext != NULL) {
		avformat_close_input(&inputContext);
	}
}

void closeOutput() {
	if (outputContext != NULL)
	{
		avformat_close_input(&outputContext);
	}
}

AVPacket* readPacketFromSource() {
	AVPacket *pkt = av_packet_alloc();
	lastReadPacktTime = av_gettime();
	int ret = av_read_frame(inputContext, pkt);
	if (ret >= 0) {
		return pkt;
	}
	else {
		return NULL;
	}
}

int writePacket(AVPacket* packet) {
	auto inputStream = inputContext->streams[packet->stream_index];
	auto outputStream = outputContext->streams[packet->stream_index];
	av_packet_rescale_ts(packet, inputStream->time_base, outputStream->time_base);
	return av_interleaved_write_frame(outputContext, packet);
}

int main() {

	av_register_all(); //注册封装器
	avcodec_register_all(); //注册解码器
	avformat_network_init();  //初始化网络

	//int ret = openInput("F://play.mp4");
	int ret = openInput("rtmp://live.hkstv.hk.lxdns.com/live/hks");
	if (ret >= 0) {
		//ret = openOutput("F://play1.ts");
		ret = openOutput("udp://127.0.0.1:1234");  //推流到udp
	}

	if (ret < 0) {
		closeInput();
		closeOutput();
	}

	while (true)
	{
		AVPacket* packet = readPacketFromSource();
		if (packet) {
			ret = writePacket(packet);
			if (ret >= 0) {
				av_packet_unref(packet);
				printf("writePacket Success!\n");
			}
			else {
				printf("writePacket failed!\n");
			}
		}
		else {
			printf("结束");
			av_packet_free(&packet);
			closeInput();
			closeOutput();
			break;
		}

	}

	system("pause");
	return 0;
}