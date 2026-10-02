#include "check.h"
#undef CHECK
#include "../src/rtsp/rtp.c"
#undef CHECK
#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            checkFailures++; \
        } \
    } while (0)

void request_idr(void) {}

/* The encoder can hand out a pack with no data and no length: nothing to send,
 * and its header must not be read */
static void test_empty_nal(void)
{
    struct list_head_t trans_list = {};

    CHECK(__transfer_nal_h26x(&trans_list, NULL, 0, 0) == SUCCESS);
    CHECK(__transfer_nal_h26x(&trans_list, NULL, 0, 1) == SUCCESS);
}

int main(void)
{
    test_empty_nal();
    CHECK_DONE();
}
