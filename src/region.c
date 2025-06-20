#include "region.h"

osd osds[MAX_OSD];
pthread_t regionPid = 0;
char timefmt[64];
unsigned int rxb_l, txb_l, cpu_l[6];

void region_fill_formatted(char* str) {
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
            char ifname[16] = {0};
            struct ifaddrs *ifaddr, *ifa;

            ipos++;

            if (str[ipos + 1] == ':') {
                int j = 0;
                ipos++;
                while (str[ipos + 1] && str[ipos + 1] != '$' && str[ipos + 1] != ' ' && j < (sizeof(ifname) - 1))
                    ifname[j++] = str[++ipos];
                ifname[j] = '\0';
            }

            if (getifaddrs(&ifaddr) == -1) continue;

            for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
            { 
                if (EQUALS(ifa->ifa_name, "lo")) continue;
                if (ifname[0] && !EQUALS(ifa->ifa_name, ifname)) continue;
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
        else if (str[ipos + 1] == 'T')
        {
            ipos++;
            char s[8];
            float t = hal_temperature_read();
            sprintf(s, "%.1f", t);
            strcat(out, s);
            opos += strlen(s);
        }
        else if (str[ipos + 1] == 't')
        {
            ipos++;
            char s[64];
            time_t t = time(NULL);
            struct tm tm_buf, *tm_info;
            if (str[ipos + 1] == 'u') {
                ipos++;
                tm_info = gmtime(&t);
            } else tm_info = localtime_r(&t, &tm_buf);
            strftime(s, 64, timefmt, tm_info);
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

static inline int region_open_bitmap(char *path, FILE **file) {
    unsigned short type;

    if (!path)
        HAL_ERROR("region", "Filename is empty!\n");
    if (!(*file = fopen(path, "rb")))
        HAL_ERROR("region", "Opening the bitmap failed!\n");
    if (fread(&type, 1, sizeof(type), *file) != sizeof(type))
        HAL_ERROR("region", "Reading the bitmap failed!\n");
    if (type != 0x4d42)
        HAL_ERROR("region", "Only bitmap files are currently supported!\n");

    return EXIT_SUCCESS;
}

int region_parse_bitmap(FILE **file, bitmapfile *bmpFile, bitmapinfo *bmpInfo) {
    if (fread(bmpFile, 1, sizeof(bitmapfile), *file) != sizeof(bitmapfile))
        HAL_ERROR("region", "Extracting the bitmap file header failed!\n");
    if (fread(bmpInfo, 1, sizeof(bitmapinfo), *file) != sizeof(bitmapinfo))
        HAL_ERROR("region", "Extracting the bitmap info failed!\n");
    if (bmpInfo->bitCount < 24)
        HAL_ERROR("region", "Indexed or <3bpp bitmaps are not supported!\n");
    if (bmpInfo->compression != 0 && !(bmpInfo->compression == 3 && (bmpInfo->bitCount == 16 || bmpInfo->bitCount == 32)))
        HAL_ERROR("region", "Compressed modes are not supported!\n");

    return EXIT_SUCCESS;
}

int region_prepare_image(char *path, hal_bitmap *bitmap) {
    FILE *file;
    unsigned char *bitmapdata, *bitmapout;
    unsigned short *dest;

    if (!path)
        HAL_ERROR("region", "Filename is empty!\n");
    if (!(file = fopen(path, "rb")))
        HAL_ERROR("region", "Opening the bitmap failed!\n");

    spng_ctx *ctx = spng_ctx_new(0);
    if (!ctx) {
        HAL_DANGER("server", "Constructing the PNG decoder context failed!\n");
        goto png_error;
    }

    spng_set_crc_action(ctx, SPNG_CRC_USE, SPNG_CRC_USE);
    spng_set_chunk_limits(ctx,
        app_config.web_server_thread_stack_size,
        app_config.web_server_thread_stack_size);
    spng_set_png_file(ctx, file);

    struct spng_ihdr ihdr;
    int err = spng_get_ihdr(ctx, &ihdr);
    if (err) {
        HAL_DANGER("server", "Parsing the PNG IHDR chunk failed!\nError: %s\n", spng_strerror(err));
        goto png_error;
    }
#ifdef DEBUG_IMAGE
    printf("[image] (PNG) width: %u, height: %u, depth: %u, color type: %u -> %s\n",
        ihdr.width, ihdr.height, ihdr.bit_depth, ihdr.color_type, color_type_str(ihdr.color_type));
    printf("        compress: %u, filter: %u, interl: %u\n",
        ihdr.compression_method, ihdr.filter_method, ihdr.interlace_method);
#endif

    struct spng_plte plte = {0};
    err = spng_get_plte(ctx, &plte);
    if (err && err != SPNG_ECHUNKAVAIL) {
        HAL_DANGER("server", "Parsing the PNG PLTE chunk failed!\nError: %s\n", spng_strerror(err));
        goto png_error;
    }
#ifdef DEBUG_IMAGE
    printf("        palette: %u\n", plte.n_entries);
#endif

    size_t bitmapsize;
    err = spng_decoded_image_size(ctx, SPNG_FMT_RGBA8, &bitmapsize);
    if (err) {
        HAL_DANGER("server", "Recovering the resulting image size failed!\nError: %s\n", spng_strerror(err));
        goto png_error;
    }

    bitmapdata = malloc(bitmapsize);
    if (!bitmapdata) {
        HAL_DANGER("server", "Allocating the PNG bitmap input buffer for size %u failed!\n", bitmapsize);
        goto png_error;
    }

    bitmapout = malloc(bitmapsize / 2);
    if (!bitmapout) {
        HAL_DANGER("server", "Allocating the PNG bitmap output buffer for size %u failed!\n", bitmapsize / 2);
        goto png_error;
    }

    err = spng_decode_image(ctx, bitmapdata, bitmapsize, SPNG_FMT_RGBA8, 0);
    if (!bitmapdata) {
        HAL_DANGER("server", "Decoding the PNG image failed!\nError: %s\n", spng_strerror(err));
        goto png_error;
    }

    dest = (unsigned short*)bitmapout;
    for (int i = 0; i < bitmapsize; i += 4) {
        *dest = ((bitmapdata[i + 3] & 0x80) << 8) | ((bitmapdata[i + 0] & 0xF8) << 7) |
            ((bitmapdata[i + 1] & 0xF8) << 2) | ((bitmapdata[i + 2] & 0xF8) >> 3);
        dest++;
    }
    free(bitmapdata);

    fclose(file);

    bitmap->data = bitmapout;
    bitmap->dim.width = ihdr.width;
    bitmap->dim.height = abs(ihdr.height);

    return EXIT_SUCCESS;

png_error:
    fclose(file);
    spng_ctx_free(ctx);
    free(bitmapdata);
    free(bitmapout);

    return EXIT_FAILURE;
}

int region_prepare_bitmap(char *path, hal_bitmap *bitmap) {
    bitmapfile bmpFile;
    bitmapinfo bmpInfo;
    bitmapfields bmpFields;
    static FILE *file;
    void *buffer, *start;
    unsigned int size;
    unsigned short *dest;
    unsigned char bpp, alpha, red, green, blue;
    char pos;

    if (region_open_bitmap(path, &file))
        return EXIT_FAILURE;

    if (region_parse_bitmap(&file, &bmpFile, &bmpInfo))
        HAL_ERROR("region", "Bitmap file \"%s\" cannot be processed!\n", path);

    if (bmpInfo.compression == 3 && fread(&bmpFields, 1, sizeof(bitmapfields), file) != sizeof(bitmapfields))
        HAL_ERROR("region", "Extracting the bitmap fields failed!\n");
    
    bpp = bmpInfo.bitCount / 8;
    size = bmpInfo.width * abs(bmpInfo.height);

    if (fseek(file, bmpFile.offBits, 0))
        HAL_ERROR("region", "Navigating to the bitmap image data failed!\n");
    if (!(buffer = malloc(size * bpp)))
        HAL_ERROR("region", "Allocating the bitmap input memory failed!\n");

    if (fread(buffer, 1, (unsigned int)(size * bpp), file) != 
        (unsigned int)(size * bpp))
        HAL_ERROR("region", "Reading the bitmap image data failed!\n");

    if (bmpInfo.height >= 0) {
        char *new  = malloc(size * bpp);
        int stride = bmpInfo.width * bpp;
        if (!new)
            HAL_ERROR("region", "Allocating the flipped bitmap memory failed!\n");
        for (int h = 0; h < bmpInfo.height; h++)
            memcpy(new + (h * stride),
                buffer + ((bmpInfo.height - 1) * stride) - (h * stride),
                stride);
        free(buffer);
        buffer = new;
    }

    if (!(bitmap->data = malloc(size * 2)))
        HAL_ERROR("region", "Allocating the destination buffer failed!\n");

    start = buffer;
    dest = bitmap->data;

    if (bmpInfo.compression != 3) {
        for (int i = 0; i < size; i++) {
            if ((pos = bmpInfo.bitCount) == 24)
                alpha = 0xFF;
            else
                alpha = (*((unsigned int*)start) >> (pos -= 8)) & 0xFF;
            red = (*((unsigned int*)start) >> (pos -= 8)) & 0xFF;
            green = (*((unsigned int*)start) >> (pos -= 8)) & 0xFF;
            blue = (*((unsigned int*)start) >> (pos -= 8)) & 0xFF;
            *dest = ((alpha & 0x80) << 8) | ((red & 0xF8) << 7) | ((green & 0xF8) << 2) | ((blue & 0xF8) >> 3);
            start += bpp;
            dest++;
        }
    } else {
        for (int i = 0; i < size; i++) {
            alpha = (*((unsigned int*)start) & bmpFields.alphaMask) >> __builtin_ctz(bmpFields.alphaMask);
            red = (*((unsigned int*)start) & bmpFields.redMask) >> __builtin_ctz(bmpFields.redMask);
            green = (*((unsigned int*)start) & bmpFields.greenMask) >> __builtin_ctz(bmpFields.greenMask);
            blue = (*((unsigned int*)start) & bmpFields.blueMask) >> __builtin_ctz(bmpFields.blueMask);
            *dest = ((alpha & 0x80) << 8) | ((red & 0xF8) << 7) | ((green & 0xF8) << 2) | ((blue & 0xF8) >> 3);
            start += bpp;
            dest++;
        }
    }
    free(buffer);

    fclose(file);

    bitmap->dim.width = bmpInfo.width;
    bitmap->dim.height = abs(bmpInfo.height);

    return EXIT_SUCCESS;
}

void *region_thread(void) {
    switch (plat) {
#if defined(__ARM_PCS_VFP)
        case HAL_PLATFORM_I6:  i6_region_init(); break;
        case HAL_PLATFORM_I6C: i6c_region_init(); break;
        case HAL_PLATFORM_M6:  m6_region_init(); break;
#endif
    }

    for (char id = 0; id < MAX_OSD; id++)
    {
        if (!EMPTY(osds[id].text) || !EMPTY(osds[id].img)) continue;
        osds[id].hand = -1;
        osds[id].color = DEF_COLOR;
        osds[id].opal = DEF_OPAL;
        osds[id].size = DEF_SIZE;
        osds[id].posx = DEF_POSX;
        osds[id].posy = DEF_POSY + (DEF_SIZE * 3 / 2) * id;
        osds[id].updt = 0;
        strncpy(osds[id].font, DEF_FONT, sizeof(osds[id].font) - 1);
        osds[id].text[0] = '\0';
        osds[id].img[0] = '\0';
        osds[id].outl = DEF_OUTL;
        osds[id].thick = DEF_THICK;
    }

    while (keepRunning) {
        for (char id = 0; id < MAX_OSD; id++) {
            if (!EMPTY(osds[id].text))
            {
                char out[80];
                strncpy(out, osds[id].text, sizeof(out) - 1);
                if (strstr(out, "$"))
                {
                    region_fill_formatted(out);
                    osds[id].updt = 1;
                }

                if (osds[id].updt) {
                    char font[256];
                    char *dirs[] = {
                        ".",
                        "/oem/usr/share",
                        "/usr/local/share/fonts",
                        "/usr/share/fonts/truetype",
                        "/usr/share/fonts",
                        NULL};
                    char **dir = dirs;
                    while (*dir) {
                        sprintf(font, "%s/%s.ttf", *dir, osds[id].font);
                        if (!access(font, F_OK)) goto found_font;
                        sprintf(font, "%s/%s2.ttf", *dir++, osds[id].font);
                        if (!access(font, F_OK)) goto found_font;
                    }
                    HAL_DANGER("region", "Font \"%s\" not found!\n", osds[id].font);
                    continue;
found_font:;
                    hal_bitmap bitmap = text_create_rendered(font, osds[id].size, out, osds[id].color,
                        osds[id].outl, osds[id].thick);
                    hal_rect rect = { .height = bitmap.dim.height, .width = bitmap.dim.width,
                        .x = osds[id].posx, .y = osds[id].posy };
                    switch (plat) {
#if defined(__ARM_PCS_VFP)
                        case HAL_PLATFORM_I6:
                            i6_region_create(id, rect, osds[id].opal);
                            i6_region_setbitmap(id, &bitmap);
                            break;
                        case HAL_PLATFORM_I6C:
                            i6c_region_create(id, rect, osds[id].opal);
                            i6c_region_setbitmap(id, &bitmap);
                            break;
                        case HAL_PLATFORM_M6:
                            m6_region_create(id, rect, osds[id].opal);
                            m6_region_setbitmap(id, &bitmap);
                            break;
                        case HAL_PLATFORM_RK:
                            rk_region_create(id, rect, osds[id].opal);
                            rk_region_setbitmap(id, &bitmap);
                            break;
#elif defined(__arm__) && !defined(__ARM_PCS_VFP)
                        case HAL_PLATFORM_GM:
                            gm_region_setbitmap(id, &bitmap);
                            gm_region_create(id, rect, osds[id].opal);
                            break;
                        case HAL_PLATFORM_V1:
                            v1_region_create(id, rect, osds[id].opal);
                            v1_region_setbitmap(id, &bitmap);
                            break;
                        case HAL_PLATFORM_V2:
                            v2_region_create(id, rect, osds[id].opal);
                            v2_region_setbitmap(id, &bitmap);
                            break;
                        case HAL_PLATFORM_V3:
                            v3_region_create(id, rect, osds[id].opal);
                            v3_region_setbitmap(id, &bitmap);
                            break;
                        case HAL_PLATFORM_V4:
                            v4_region_create(id, rect, osds[id].opal);
                            v4_region_setbitmap(id, &bitmap);
                            break;
#elif defined(__mips__)
                        case HAL_PLATFORM_T31:
                            t31_region_create(&osds[id].hand, rect, osds[id].opal);
                            t31_region_setbitmap(&osds[id].hand, &bitmap);
                            break;
#endif
                    }
                    free(bitmap.data);
                }
            }
            else if (EMPTY(osds[id].text) && osds[id].updt)
            {
                char img[64];
                if (EMPTY(osds[id].img))
                    sprintf(img, "/tmp/osd%d.bmp", id);
                else strncpy(img, osds[id].img, sizeof(osds[id].img) - 1);
                if (!access(img, F_OK))
                {
                    hal_bitmap bitmap;
                    int ret;
                    if (ENDS_WITH(img, ".png"))
                        ret = region_prepare_image(img, &bitmap);
                    else
                        ret = region_prepare_bitmap(img, &bitmap);
                    if (!ret)
                    {
                        hal_rect rect = { .height = bitmap.dim.height, .width = bitmap.dim.width,
                            .x = osds[id].posx, .y = osds[id].posy };
                        switch (plat) {
#if defined(__ARM_PCS_VFP)
                            case HAL_PLATFORM_I6:
                                i6_region_create(id, rect, osds[id].opal);
                                i6_region_setbitmap(id, &bitmap);
                                break;
                            case HAL_PLATFORM_I6C:
                                i6c_region_create(id, rect, osds[id].opal);
                                i6c_region_setbitmap(id, &bitmap);
                                break;
                            case HAL_PLATFORM_M6:
                                m6_region_create(id, rect, osds[id].opal);
                                m6_region_setbitmap(id, &bitmap);
                                break;
                            case HAL_PLATFORM_RK:
                                rk_region_create(id, rect, osds[id].opal);
                                rk_region_setbitmap(id, &bitmap);
                                break;
#elif defined(__arm__) && !defined(__ARM_PCS_VFP)
                            case HAL_PLATFORM_GM:
                                gm_region_create(id, rect, osds[id].opal);
                                gm_region_setbitmap(id, &bitmap);
                                break;
                            case HAL_PLATFORM_V1:
                                v1_region_create(id, rect, osds[id].opal);
                                v1_region_setbitmap(id, &bitmap);
                                break;
                            case HAL_PLATFORM_V2:
                                v2_region_create(id, rect, osds[id].opal);
                                v2_region_setbitmap(id, &bitmap);
                                break;
                            case HAL_PLATFORM_V3:
                                v3_region_create(id, rect, osds[id].opal);
                                v3_region_setbitmap(id, &bitmap);
                                break;
                            case HAL_PLATFORM_V4:
                                v4_region_create(id, rect, osds[id].opal);
                                v4_region_setbitmap(id, &bitmap);
                                break;
#elif defined(__mips__)
                            case HAL_PLATFORM_T31:
                                t31_region_create(&osds[id].hand, rect, osds[id].opal);
                                t31_region_setbitmap(&osds[id].hand, &bitmap);
                                break;
#endif
                        }
                        free(bitmap.data);
                    }
                }
                else
                    switch (plat) {
#if defined(__ARM_PCS_VFP)
                        case HAL_PLATFORM_I6:  i6_region_destroy(id); break;
                        case HAL_PLATFORM_I6C: i6c_region_destroy(id); break;
                        case HAL_PLATFORM_M6:  m6_region_destroy(id); break;
                        case HAL_PLATFORM_RK:  rk_region_destroy(id); break;
#elif defined(__arm__) && !defined(__ARM_PCS_VFP)
                        case HAL_PLATFORM_GM:  gm_region_destroy(id); break;
                        case HAL_PLATFORM_V1:  v1_region_destroy(id); break;
                        case HAL_PLATFORM_V2:  v2_region_destroy(id); break;
                        case HAL_PLATFORM_V3:  v3_region_destroy(id); break;
                        case HAL_PLATFORM_V4:  v4_region_destroy(id); break;
#elif defined(__mips__)
                        case HAL_PLATFORM_T31: t31_region_destroy(&osds[id].hand); break;
#endif
                    }
            }
            osds[id].updt = 0;
        }
        sleep(1);
    }

    switch (plat) {
#if defined(__ARM_PCS_VFP)
        case HAL_PLATFORM_I6:  i6_region_deinit(); break;
        case HAL_PLATFORM_I6C: i6c_region_deinit(); break;
        case HAL_PLATFORM_M6:  m6_region_deinit(); break;
#endif
    }
}

int start_region_handler() {
    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    size_t stacksize;
    pthread_attr_getstacksize(&thread_attr, &stacksize);
    size_t new_stacksize = 320 * 1024;
    if (pthread_attr_setstacksize(&thread_attr, new_stacksize))
        HAL_DANGER("region", "Can't set stack size %zu\n", new_stacksize);
    if (pthread_create(
            &regionPid, &thread_attr, (void *(*)(void *))region_thread, NULL))
        HAL_DANGER("region", "Starting the handler thread failed!\n");
    if (pthread_attr_setstacksize(&thread_attr, stacksize))
        HAL_DANGER("region", "Can't set stack size %zu\n", stacksize);
    pthread_attr_destroy(&thread_attr);
}

void stop_region_handler() {
    pthread_join(regionPid, NULL);
}