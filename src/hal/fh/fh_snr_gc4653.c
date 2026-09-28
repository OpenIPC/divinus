#if defined(__arm__) && !defined(__ARM_PCS_VFP) && __ARM_ARCH == 6

/*
 * GalaxyCore GC4653 (2560x1440, RAW10, 2-lane MIPI) driver for the Fullhan
 * V100 ISP sensor callback interface. Registers are programmed through the
 * standard Linux i2c-dev interface on /dev/i2c-0 (16-bit address, 8-bit data).
 */
#include "fh_snr.h"
#include "fh_snr_gc4653_regs.h"

#include <fcntl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define GC4653_CHIP_ID   0x4653
#define GC4653_REG_ID_H  0x03f0
#define GC4653_REG_ID_L  0x03f1
#define GC4653_REG_FLIP  0x0101
#define GC4653_REG_EXP_H 0x0202
#define GC4653_REG_EXP_L 0x0203
#define GC4653_REG_DIG_H 0x020e
#define GC4653_REG_DIG_L 0x020f
#define GC4653_REG_VTS_H 0x0340
#define GC4653_REG_VTS_L 0x0341

/* Fullhan sensor format codes understood by this driver */
#define GC4653_FMT_15FPS 0x601
#define GC4653_FMT_20FPS 0x602
#define GC4653_FMT_25FPS 0x603
#define GC4653_FMT_30FPS 0x604

#define GC4653_GAIN_ROWS (sizeof(gc4653_gain_level) / sizeof(*gc4653_gain_level))

static const unsigned char gc4653_i2c_addrs[] = { 0x29, 0x10 };
static const unsigned short gc4653_gain_reg_addr[7] =
    { 0x02b3, 0x02b4, 0x02b8, 0x02b9, 0x0515, 0x0519, 0x02d9 };

static fh_isp_impl *_gc4653_isp;
static int _gc4653_fd = -1;
static unsigned char _gc4653_addr;
static int _gc4653_fmt = GC4653_FMT_25FPS;
static unsigned int _gc4653_gain = 64, _gc4653_intt = 0x6f;
static char _gc4653_ready;

static int gc4653_read(unsigned short reg)
{
    unsigned char wbuf[2] = { reg >> 8, reg & 0xff }, rbuf[1] = { 0 };
    struct i2c_msg msgs[2] = {
        { .addr = _gc4653_addr, .flags = 0, .len = 2, .buf = wbuf },
        { .addr = _gc4653_addr, .flags = I2C_M_RD, .len = 1, .buf = rbuf },
    };
    struct i2c_rdwr_ioctl_data data = { .msgs = msgs, .nmsgs = 2 };
    if (ioctl(_gc4653_fd, I2C_RDWR, &data) < 0)
        return -1;
    return rbuf[0];
}

static int gc4653_write(unsigned short reg, unsigned char val)
{
    unsigned char wbuf[3] = { reg >> 8, reg & 0xff, val };
    struct i2c_msg msg = { .addr = _gc4653_addr, .flags = 0, .len = 3, .buf = wbuf };
    struct i2c_rdwr_ioctl_data data = { .msgs = &msg, .nmsgs = 1 };
    if (ioctl(_gc4653_fd, I2C_RDWR, &data) < 0) {
        HAL_WARNING("fh_snr", "GC4653 write of register %#x failed!\n", reg);
        return -1;
    }
    return 0;
}

static void gc4653_write_table(const gc_reg *table, int count)
{
    for (int i = 0; i < count; i++)
        gc4653_write(table[i].reg, table[i].val);
}

static int gc4653_open(unsigned char addr)
{
    if (_gc4653_fd >= 0)
        close(_gc4653_fd);
    if ((_gc4653_fd = open("/dev/i2c-0", O_RDWR)) < 0)
        return -1;
    _gc4653_addr = addr;
    ioctl(_gc4653_fd, I2C_TENBIT, 0);
    ioctl(_gc4653_fd, I2C_SLAVE, addr);
    return 0;
}

static int gc4653_probe(void)
{
    for (unsigned int i = 0; i < sizeof(gc4653_i2c_addrs); i++) {
        if (gc4653_open(gc4653_i2c_addrs[i]))
            return -1;
        int high = gc4653_read(GC4653_REG_ID_H), low = gc4653_read(GC4653_REG_ID_L);
        if (high >= 0 && low >= 0 && ((high << 8) | low) == GC4653_CHIP_ID)
            return 0;
    }
    close(_gc4653_fd);
    _gc4653_fd = -1;
    return -1;
}

static int gc4653_set_gain(unsigned int gain)
{
    unsigned int row = 0;
    if (gain < gc4653_gain_level[0])
        gain = gc4653_gain_level[0];
    while (row + 1 < GC4653_GAIN_ROWS && gain >= gc4653_gain_level[row + 1])
        row++;
    _gc4653_gain = gain;
    for (int i = 0; i < 7; i++)
        gc4653_write(gc4653_gain_reg_addr[i], gc4653_gain_regs[row][i]);
    unsigned int digital = (gain << 6) / gc4653_gain_level[row];   /* 6.6 fixed point */
    gc4653_write(GC4653_REG_DIG_H, digital >> 6);
    gc4653_write(GC4653_REG_DIG_L, (digital & 0x3f) << 2);
    return 0;
}

static int gc4653_get_gain(unsigned int *gain)
{
    *gain = _gc4653_gain;
    return 0;
}

static int gc4653_get_input_attr(fh_isp_viattr *attr)
{
    if (!attr)
        return -0xbba;
    memset(attr, 0, sizeof(*attr));
    switch (_gc4653_fmt) {
        case GC4653_FMT_15FPS: attr->vts = 1500; attr->hts = 9600; break;
        case GC4653_FMT_20FPS: attr->vts = 2250; attr->hts = 4800; break;
        case GC4653_FMT_25FPS: attr->vts = 1800; attr->hts = 4800; break;
        case GC4653_FMT_30FPS: attr->vts = 1500; attr->hts = 4800; break;
        default: return -0xbbd;
    }
    attr->width = attr->width2 = 2560;
    attr->height = attr->height2 = 1440;
    attr->format = 1;
    return 0;
}

static int gc4653_set_integration(unsigned int lines)
{
    _gc4653_intt = lines;
    gc4653_write(GC4653_REG_EXP_H, (lines >> 8) & 0xff);
    gc4653_write(GC4653_REG_EXP_L, lines & 0xff);
    return 0;
}

static int gc4653_get_integration(unsigned int *lines)
{
    *lines = _gc4653_intt;
    return 0;
}

static int gc4653_set_frame_height(int multiplier)
{
    fh_isp_viattr attr;
    gc4653_get_input_attr(&attr);
    unsigned int vts = attr.vts * multiplier;
    gc4653_write(GC4653_REG_VTS_H, (vts >> 8) & 0xff);
    gc4653_write(GC4653_REG_VTS_L, vts & 0xff);
    return 0;
}

/* ISP passes bit 0 = flip, bit 1 = mirror; the sensor register has bit 0 = mirror, bit 1 = flip */
static int gc4653_set_flip_mirror(unsigned int bits)
{
    int val = gc4653_read(GC4653_REG_FLIP);
    if (val < 0) return -1;
    return gc4653_write(GC4653_REG_FLIP, (val & 0xfc) | ((bits >> 1) & 1) | ((bits & 1) << 1));
}

static int gc4653_get_flip_mirror(unsigned int *bits)
{
    int val = gc4653_read(GC4653_REG_FLIP);
    if (val < 0) return -1;
    *bits = ((val >> 1) & 1) | ((val & 1) << 1);
    return 0;
}

static int gc4653_init(void)
{
    if (gc4653_probe())
        return -1;
    _gc4653_gain = 64;
    _gc4653_intt = 0x6f;
    _gc4653_ready = 1;
    return 0;
}

static int gc4653_reset(void) { return 0; }

static int gc4653_deinit(void)
{
    if (_gc4653_fd >= 0) close(_gc4653_fd);
    _gc4653_fd = -1;
    _gc4653_ready = 0;
    return 0;
}

static int gc4653_set_format(int format)
{
    fh_isp_mipi mipi = { .freqRange = 8, .sensorMode = 0, .rawType = 0,
        .longFrameVc = 0xff, .shortFrameVc = 0, .laneNum = 2 };
    _gc4653_fmt = format;
    _gc4653_isp->fnMipiInit(&mipi);
    if (!_gc4653_ready)
        return 0;
    switch (format) {
        case GC4653_FMT_15FPS: gc4653_write_table(gc4653_init_15fps, sizeof(gc4653_init_15fps) / sizeof(gc_reg)); break;
        case GC4653_FMT_20FPS: gc4653_write_table(gc4653_init_20fps, sizeof(gc4653_init_20fps) / sizeof(gc_reg)); break;
        case GC4653_FMT_25FPS: gc4653_write_table(gc4653_init_25fps, sizeof(gc4653_init_25fps) / sizeof(gc_reg)); break;
        case GC4653_FMT_30FPS: gc4653_write_table(gc4653_init_30fps, sizeof(gc4653_init_30fps) / sizeof(gc_reg)); break;
        default: return -1;
    }
    gc4653_set_integration(500);
    gc4653_set_gain(64);
    return 0;
}

static int gc4653_set_register(unsigned int reg, unsigned int val)
{
    return gc4653_write(reg, val);
}

static int gc4653_get_register(unsigned int reg, unsigned short *val)
{
    int ret = gc4653_read(reg);
    if (ret < 0) return -1;
    *val = ret;
    return 0;
}

static int gc4653_set_chip_id(unsigned int id) { (void)id; return 0; }

static fh_isp_snrops gc4653_ops = {
    .name = "gc4653_mipi",
    .fnSetGain = gc4653_set_gain,
    .fnGetInputAttr = gc4653_get_input_attr,
    .fnGetGain = gc4653_get_gain,
    .fnSetIntegration = gc4653_set_integration,
    .fnSetFrameHeight = gc4653_set_frame_height,
    .fnGetIntegration = gc4653_get_integration,
    .fnSetFlipMirror = gc4653_set_flip_mirror,
    .fnGetFlipMirror = gc4653_get_flip_mirror,
    .fnInit = gc4653_init,
    .fnReset = gc4653_reset,
    .fnDeinit = gc4653_deinit,
    .fnSetFormat = gc4653_set_format,
    .fnSetRegister = gc4653_set_register,
    .fnSetChipId = gc4653_set_chip_id,
    .fnGetRegister = gc4653_get_register,
};

static int gc4653_format_for_framerate(int framerate)
{
    if (framerate <= 15) return GC4653_FMT_15FPS;
    if (framerate <= 20) return GC4653_FMT_20FPS;
    if (framerate <= 25) return GC4653_FMT_25FPS;
    return GC4653_FMT_30FPS;
}

static int gc4653_framerate_for_format(int format)
{
    switch (format) {
        case GC4653_FMT_15FPS: return 15;
        case GC4653_FMT_20FPS: return 20;
        case GC4653_FMT_25FPS: return 25;
        default: return 30;
    }
}

static int gc4653_probe_once(void)
{
    int ret = gc4653_probe();
    gc4653_deinit();
    return ret;
}

static fh_isp_snrops *gc4653_create(fh_isp_impl *isp)
{
    _gc4653_isp = isp;
    return &gc4653_ops;
}

fh_snr_driver fh_snr_gc4653 = {
    .name = "gc4653_mipi",
    .dim = { .width = 2560, .height = 1440 },
    .bayer = 1,
    .fnProbe = gc4653_probe_once,
    .fnFormatForFramerate = gc4653_format_for_framerate,
    .fnFramerateForFormat = gc4653_framerate_for_format,
    .fnCreate = gc4653_create,
};

#endif
