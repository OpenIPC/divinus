#include "region.h"

#define tag "[region] "

osd osds[MAX_OSD];
pthread_t regionPid = 0;
char timefmt[32] = DEF_TIMEFMT;

void region_fill_formatted(char* str)
{
    unsigned int rxb_l, txb_l, cpu_l[6];
    char out[80] = "";
    char param = 0;
    int ipos = 0, opos = 0;

    while(str[ipos] != 0)
    {
        if (str[ipos] != '$')
        {
            strncat(out, str + ipos, 1);
            opos++;
        }
        else if (str[ipos + 1] == 'B')
        {
            ipos++;
            struct ifaddrs *ifaddr, *ifa;
            if (getifaddrs(&ifaddr) == -1) continue;

            for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
            { 
                if (equals(ifa->ifa_name, "lo")) continue;
                if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_PACKET) continue;
                if (!ifa->ifa_data) continue;

                struct rtnl_link_stats *stats = ifa->ifa_data;
                char b[32];
                sprintf(b, "R:%dKbps S:%dKbps", 
                    (stats->rx_bytes - rxb_l) / 1024, (stats->tx_bytes - txb_l) / 1024);
                strcat(out, b);
                opos += strlen(b);
                rxb_l = stats->rx_bytes;
                txb_l = stats->tx_bytes;
                break;
            }
            
            freeifaddrs(ifaddr);
        }
        else if (str[ipos + 1] == 'C')
        {
            ipos++;
            char tmp[6];
            unsigned int cpu[6];
            FILE *stat = fopen("/proc/stat", "r");
            fscanf(stat, "%s %u %u %u %u %u %u",
                tmp, &cpu[0], &cpu[1], &cpu[2], &cpu[3], &cpu[4], &cpu[5]);
            fclose(stat);

            char c[5];
            char avg = 100 - (cpu[3] - cpu_l[3]) / sysconf(_SC_NPROCESSORS_ONLN);
            sprintf(c, "%d%%", avg);
            strcat(out, c);
            opos += strlen(c);
            for (int i = 0; i < sizeof(cpu) / sizeof(cpu[0]); i++)
                cpu_l[i] = cpu[i];
        }
        else if (str[ipos + 1] == 'M')
        {
            ipos++;
            struct sysinfo si;
            sysinfo(&si);

            char m[16];
            short used = (si.freeram + si.bufferram) / 1024 / 1024;
            short total = si.totalram / 1024 / 1024;
            sprintf(m, "%d/%dMB", used, total);
            strcat(out, m);
            opos += strlen(m);
        }
        else if (str[ipos + 1] == 't')
        {
            ipos++;
            char s[64];
            time_t t = time(NULL);
            struct tm *tm = gmtime(&t);
            strftime(s, 64, timefmt, tm);
            strcat(out, s);
            opos += strlen(s);
        }
        else if (str[ipos + 1] == '$') {
            ipos++;
            strcat(out, "$");
            opos++;
        }
        ipos++; 
    }
    strncpy(str, out, 80);
}

static inline int region_open_bitmap(char *path, FILE **file)
{
    unsigned short type;

    if (!path)
        REGION_ERROR("Filename is empty!\n");
    if (!(*file = fopen(path, "rb")))
        REGION_ERROR("Opening the bitmap failed!\n");
    if (fread(&type, 1, sizeof(type), *file) != sizeof(type))
        REGION_ERROR("Reading the bitmap failed!\n");
    if (type != 0x4d42)
        REGION_ERROR("Only bitmap files are currently supported!\n");

    return EXIT_SUCCESS;
}

int region_parse_bitmap(FILE **file, bitmapfile *bmpFile, bitmapinfo *bmpInfo)
{
    if (fread(bmpFile, 1, sizeof(bitmapfile), *file) != sizeof(bitmapfile))
        REGION_ERROR("Extracting the bitmap file header failed!\n");
    if (fread(bmpInfo, 1, sizeof(bitmapinfo), *file) != sizeof(bitmapinfo))
        REGION_ERROR("Extracting the bitmap info failed!\n");

    if (bmpInfo->bitCount / 8 < 2)
        REGION_ERROR("Indexed or <4bpp bitmaps are not supported!\n");
    if (bmpInfo->height < 0)
        REGION_ERROR("Flipped bitmaps are not supported!\n");

    return EXIT_SUCCESS;
}

int region_prepare_bitmap(char *path, hal_bitmap *bitmap)
{
    bitmapfile bmpFile;
    bitmapinfo bmpInfo;
    static FILE *file;
    void *buffer, *start;
    unsigned int stride;
    unsigned short *dest;
    unsigned char alpha, alphaLen, red, redLen, 
        green, greenLen, blue, blueLen, pos;

    if (region_open_bitmap(path, &file))
        return EXIT_FAILURE;

    if (region_parse_bitmap(&file, &bmpFile, &bmpInfo))
        REGION_ERROR("Bitmap file \"%s\" cannot be processed!\n", path);

    if (fseek(file, bmpFile.offBits, 0))
        REGION_ERROR("Navigating to the bitmap image data failed!\n");

    if (!(bitmap->data = malloc(2 * bmpInfo.width * bmpInfo.height)))
        REGION_ERROR("Allocating the destination buffer failed!\n");

    printf("fmt: %#x\n", bmpInfo.format);
    stride = bmpInfo.width * bmpInfo.bitCount / 8;
    alphaLen = (bmpInfo.format >> 24) & 0xFF;
    redLen = (bmpInfo.format >> 16) & 0xFF;
    greenLen = (bmpInfo.format >> 8) & 0xFF;
    blueLen = bmpInfo.format & 0xFF;

    if (!(buffer = malloc(bmpInfo.height * stride)))
        REGION_ERROR("Allocating the bitmap input memory failed!\n");

    if (fread(buffer, 1, (unsigned int)(bmpInfo.height * stride), file) != 
        (unsigned int)(bmpInfo.height * stride))
        REGION_ERROR("Reading the bitmap image data failed!\n");

    start = buffer;
    dest = bitmap->data;
    for (int i = 0; i < bmpInfo.height * bmpInfo.height; i++) {
        blue = *((unsigned int*)start) & blueLen;
        pos += blueLen;
        green = (*((unsigned int*)start) >> pos) & greenLen;
        pos += greenLen;
        red = (*((unsigned int*)start) >> pos) & redLen;
        pos += redLen;
        alpha = (*((unsigned int*)start) >> pos) & alphaLen;
        *dest = ((alpha & 1) << 15) | ((red & 31) << 10) | ((green & 31) << 5) | (blue & 31);
        start += stride;
        dest++;
    }
    free(buffer);

    fclose(file);
}

void *region_thread(void)
{
    switch (plat) {
        case HAL_PLATFORM_I6:  i6_region_init(); break;
        case HAL_PLATFORM_I6C: i6c_region_init(); break;
        case HAL_PLATFORM_I6F: i6f_region_init(); break;
    }

    for (char id = 0; id < MAX_OSD; id++)
    {
        osds[id].hand = -1;
        osds[id].color = DEF_COLOR;
        osds[id].opal = DEF_OPAL;
        osds[id].size = DEF_SIZE;
        osds[id].posx = DEF_POSX;
        osds[id].posy = DEF_POSY + (DEF_SIZE * 3 / 2) * id;
        osds[id].updt = 0;
        strcpy(osds[id].font, DEF_FONT);
        osds[id].text[0] = '\0';
    }

    while (keepRunning) {
        for (char id = 0; id < MAX_OSD; id++) {
            if (!empty(osds[id].text))
            {
                char out[80];
                strcpy(out, osds[id].text);
                if (strstr(out, "$"))
                {
                    region_fill_formatted(out);
                    osds[id].updt = 1;
                }

                if (osds[id].updt) {
                    char *font;
                    asprintf(&font, "/usr/share/fonts/truetype/%s.ttf", osds[id].font);
                    if (!access(font, F_OK)) {
                        hal_bitmap bitmap = text_create_rendered(font, osds[id].size, out, osds[id].color);
                        hal_rect rect = { .height = bitmap.dim.height, .width = bitmap.dim.width,
                            .x = osds[id].posx, .y = osds[id].posy };
                        switch (plat) {
                            case HAL_PLATFORM_I6:
                                i6_region_create(id, rect, osds[id].opal);
                                i6_region_setbitmap(id, &bitmap);
                                break;
                            case HAL_PLATFORM_I6C:
                                i6c_region_create(id, rect, osds[id].opal);
                                i6c_region_setbitmap(id, &bitmap);
                                break;
                            case HAL_PLATFORM_I6F:
                                i6f_region_create(id, rect, osds[id].opal);
                                i6f_region_setbitmap(id, &bitmap);
                                break;
                            case HAL_PLATFORM_V3:
                                v3_region_create(id, rect, osds[id].opal);
                                v3_region_setbitmap(id, &bitmap);
                                break;
                            case HAL_PLATFORM_V4:
                                v4_region_create(id, rect, osds[id].opal);
                                v4_region_setbitmap(id, &bitmap);
                                break;
                            case HAL_PLATFORM_T31:
                                t31_region_create(&osds[id].hand, rect, osds[id].opal);
                                t31_region_setbitmap(&osds[id].hand, &bitmap);
                                break;
                        }
                        free(bitmap.data);
                    }
                }
            }
            else if (empty(osds[id].text) && osds[id].updt)
            {
                char img[32];
                sprintf(img, "/tmp/osd%d.bmp", id);
                if (!access(img, F_OK))
                {
                    hal_bitmap bitmap;
                    if (!(region_prepare_bitmap(img, &bitmap)))
                    {
                        hal_rect rect = { .height = bitmap.dim.height, .width = bitmap.dim.width,
                            .x = osds[id].posx, .y = osds[id].posy };
                        switch (plat) {
                            case HAL_PLATFORM_I6:
                                i6_region_create(id, rect, osds[id].opal);
                                i6_region_setbitmap(id, &bitmap);
                                break;
                            case HAL_PLATFORM_I6C:
                                i6c_region_create(id, rect, osds[id].opal);
                                i6c_region_setbitmap(id, &bitmap);
                                break;
                            case HAL_PLATFORM_I6F:
                                i6f_region_create(id, rect, osds[id].opal);
                                i6f_region_setbitmap(id, &bitmap);
                                break;
                            case HAL_PLATFORM_V3:
                                v3_region_create(id, rect, osds[id].opal);
                                v3_region_setbitmap(id, &bitmap);
                                break;
                            case HAL_PLATFORM_V4:
                                v4_region_create(id, rect, osds[id].opal);
                                v4_region_setbitmap(id, &bitmap);
                                break;
                            case HAL_PLATFORM_T31:
                                t31_region_create(&osds[id].hand, rect, osds[id].opal);
                                t31_region_setbitmap(&osds[id].hand, &bitmap);
                                break;
                        }
                        free(bitmap.data);
                    }
                }
                else
                    switch (plat) {
                        case HAL_PLATFORM_I6:  i6_region_destroy(id); break;
                        case HAL_PLATFORM_I6C: i6c_region_destroy(id); break;
                        case HAL_PLATFORM_I6F: i6f_region_destroy(id); break;
                        case HAL_PLATFORM_V3:  v3_region_destroy(id); break;
                        case HAL_PLATFORM_V4:  v4_region_destroy(id); break;
                        case HAL_PLATFORM_T31: t31_region_destroy(&osds[id].hand); break;
                    }
                osds[id].updt = 0;
            }
        }
        sleep(1);
    }

    switch (plat) {
        case HAL_PLATFORM_I6:  i6_region_deinit(); break;
        case HAL_PLATFORM_I6C: i6c_region_deinit(); break;
        case HAL_PLATFORM_I6F: i6f_region_deinit(); break;
    }
}

int start_region_handler() {
    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    size_t stacksize;
    pthread_attr_getstacksize(&thread_attr, &stacksize);
    size_t new_stacksize = 320 * 1024;
    if (pthread_attr_setstacksize(&thread_attr, new_stacksize)) {
        printf(tag "Can't set stack size %zu\n", new_stacksize);
    }
    pthread_create(&regionPid, &thread_attr, (void *(*)(void *))region_thread, NULL);
    if (pthread_attr_setstacksize(&thread_attr, stacksize)) {
        printf(tag "Error:  Can't set stack size %zu\n", stacksize);
    }
    pthread_attr_destroy(&thread_attr);
}

void stop_region_handler() {
    pthread_join(regionPid, NULL);
}