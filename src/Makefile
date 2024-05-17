SRCS := hal/hisi/*_hal.c hal/sstar/*_hal.c hal/config.c hal/support.c hal/tools.c\
	 mp4/bitbuf.c mp4/moof.c mp4/moov.c mp4/mp4.c mp4/nal.c\
	 rtsp/ringfifo.c rtsp/rtputils.c rtsp/rtspservice.c rtsp/rtsputils.c\
	 app_config.c compat.c error.c gpio.c http_post.c jpeg.c main.c night.c server.c video.c
BUILD = $(CC) $(SRCS) -I. -ldl -lpthread -Os -s -o ../$(or $(TARGET),$@)

divinus-musl:
	$(BUILD)

divinus-muslhf:
	$(BUILD)

divinus-glibc:
	$(BUILD)