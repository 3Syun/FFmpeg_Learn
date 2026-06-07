#include <stdio.h>
#include <string.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

int write_h264_to_mp4(const char *filename,
                      const uint8_t *sps_pps_data, int sps_pps_size,
                      int width, int height, int fps) {
    // 1. 创建“假”编码器上下文
    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);  // 注意 const
    AVCodecContext *enc_ctx = avcodec_alloc_context3(codec);
    if (!enc_ctx) {
        fprintf(stderr, "Failed to alloc codec context\n");
        return -1;
    }
    enc_ctx->width = width;
    enc_ctx->height = height;
    enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    enc_ctx->time_base = (AVRational){1, fps};

    // 2. 设置 extradata（SPS+PPS）
    enc_ctx->extradata = (uint8_t*)av_mallocz(sps_pps_size + AV_INPUT_BUFFER_PADDING_SIZE);
    if (!enc_ctx->extradata) {
        avcodec_free_context(&enc_ctx);
        return -1;
    }
    memcpy(enc_ctx->extradata, sps_pps_data, sps_pps_size);
    enc_ctx->extradata_size = sps_pps_size;

    // 3. 分配输出上下文
    AVFormatContext *fmt_ctx = nullptr;
    avformat_alloc_output_context2(&fmt_ctx, nullptr, nullptr, filename);
    if (!fmt_ctx) {
        avcodec_free_context(&enc_ctx);
        return -1;
    }

    // 4. 创建视频流
    AVStream *video_stream = avformat_new_stream(fmt_ctx, nullptr);
    if (!video_stream) {
        avformat_free_context(fmt_ctx);
        avcodec_free_context(&enc_ctx);
        return -1;
    }
    video_stream->id = fmt_ctx->nb_streams - 1;
    int stream_idx = video_stream->index;

    // 5. 拷贝编码参数
    avcodec_parameters_from_context(video_stream->codecpar, enc_ctx);
    video_stream->time_base = enc_ctx->time_base;

    // 6. 打开文件
    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&fmt_ctx->pb, filename, AVIO_FLAG_WRITE) < 0) {
            avformat_free_context(fmt_ctx);
            avcodec_free_context(&enc_ctx);
            return -1;
        }
    }

    // 7. 写文件头
    if (avformat_write_header(fmt_ctx, nullptr) < 0) {
        avio_closep(&fmt_ctx->pb);
        avformat_free_context(fmt_ctx);
        avcodec_free_context(&enc_ctx);
        return -1;
    }

    // 8. 写入演示帧（使用新版 packet API）
    AVPacket *pkt = av_packet_alloc();
    // 注意：这里为了演示，将 const 数据强制转为非 const，实际应传入非 const 帧数据
    pkt->data = (uint8_t*)sps_pps_data;   // 实际项目请替换为真实可写帧数据
    pkt->size = sps_pps_size;
    pkt->stream_index = stream_idx;
    pkt->pts = 0;
    pkt->dts = 0;
    pkt->flags = AV_PKT_FLAG_KEY;
    av_interleaved_write_frame(fmt_ctx, pkt);
    av_packet_free(&pkt);

    // 9. 写文件尾并释放资源
    av_write_trailer(fmt_ctx);
    avio_closep(&fmt_ctx->pb);
    avformat_free_context(fmt_ctx);
    avcodec_free_context(&enc_ctx);
    return 0;
}

int main() {
    // 模拟的 SPS+PPS 数据（通常从硬件编码器获取）
    uint8_t fake_sps_pps[] = {
        0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x0a,  // SPS
        0x00, 0x00, 0x00, 0x01, 0x68, 0xce, 0x38, 0x80   // PPS
    };
    int sps_pps_size = sizeof(fake_sps_pps);

    int ret = write_h264_to_mp4("output.mp4", fake_sps_pps, sps_pps_size,
                                 1920, 1080, 30);
    if (ret == 0) {
        printf("MP4 file written successfully.\n");
    } else {
        printf("Failed to write MP4.\n");
    }
    return ret;
}