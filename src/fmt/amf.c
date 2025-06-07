#include "amf.h"

static double s_double = 1.0; // 3ff0 0000 0000 0000

static enum BufError AMFWriteInt16(struct BitBuf* buf, uint16_t value) {
    return put_u16_be(buf, value);
}

static enum BufError AMFWriteInt32(struct BitBuf* buf, uint32_t value) {
    return put_u32_be(buf, value);
}

static enum BufError AMFWriteString16(struct BitBuf* buf, const char* string, size_t length) {
    enum BufError err;
    err = put_u16_be(buf, (uint16_t)length);
    chk_err;

    return put(buf, string, length);
}

static enum BufError AMFWriteString32(struct BitBuf* buf, const char* string, size_t length) {
    enum BufError err;
    err = put_u32_be(buf, (uint32_t)length);
    chk_err;

    return put(buf, string, length);
}

enum BufError AMFWriteNull(struct BitBuf* buf) {
    return put_u8(buf, AMF_NULL);
}

enum BufError AMFWriteUndefined(struct BitBuf* buf) {
    return put_u8(buf, AMF_UNDEFINED);
}

enum BufError AMFWriteObject(struct BitBuf* buf) {
    return put_u8(buf, AMF_OBJECT);
}

enum BufError AMFWriteObjectEnd(struct BitBuf* buf) {
    enum BufError err;
    err = put_u8(buf, 0);
    chk_err;

    err = put_u8(buf, 0);
    chk_err;

    return put_u8(buf, AMF_OBJECT_END);
}

enum BufError AMFWriteTypedObject(struct BitBuf* buf) {
    return put_u8(buf, AMF_TYPED_OBJECT);
}

enum BufError AMFWriteECMAArray(struct BitBuf* buf) {
    enum BufError err;
    err = put_u8(buf, AMF_ECMA_ARRAY);
    chk_err;

    return put_u32_be(buf, 0); // U32 associative-count
}

enum BufError AMFWriteBoolean(struct BitBuf* buf, uint8_t value) {
    enum BufError err;
    err = put_u8(buf, AMF_BOOLEAN);
    chk_err;

    return put_u8(buf, value ? 1 : 0);
}

enum BufError AMFWriteDouble(struct BitBuf* buf, double value) {
    enum BufError err;
    uint8_t bytes[8];

    err = put_u8(buf, AMF_NUMBER);
    chk_err;

    // Little-Endian
    if (0x00 == *(char*)&s_double) {
        uint8_t* v = (uint8_t*)&value;
        for (int i = 0; i < 8; ++i)
            bytes[i] = v[7 - i];
    } else {
        memcpy(bytes, &value, 8);
    }

    return put(buf, (const char*)bytes, 8);
}

enum BufError AMFWriteString(struct BitBuf* buf, const char* string, size_t length) {
    enum BufError err;

    if (length < 65536) {
        err = put_u8(buf, AMF_STRING);
        chk_err;

        err = AMFWriteString16(buf, string, length);
    } else {
        err = put_u8(buf, AMF_LONG_STRING);
        chk_err;

        err = AMFWriteString32(buf, string, length);
    }

    return err;
}

enum BufError AMFWriteDate(struct BitBuf *buf, double milliseconds, int16_t timezone) {
    enum BufError err;
    uint8_t bytes[8];

    err = put_u8(buf, AMF_DATE);
    chk_err;

    // Little-Endian
    if (0x00 == *(char*)&s_double) {
        uint8_t* v = (uint8_t*)&milliseconds;
        for (int i = 0; i < 8; ++i)
            bytes[i] = v[7 - i];
    } else {
        memcpy(bytes, &milliseconds, 8);
    }

    err = put(buf, (const char*)bytes, 8);
    chk_err;

    return put_u16_be(buf, (uint16_t)timezone);
}

enum BufError AMFWriteNamed(struct BitBuf *buf, const char* name, size_t length) {
    return AMFWriteString16(buf, name, length);
}

enum BufError AMFWriteNamedBoolean(struct BitBuf *buf, const char* name, size_t length, uint8_t value) {
    enum BufError err;
    err = AMFWriteString16(buf, name, length);
    chk_err;

    return AMFWriteBoolean(buf, value);
}

enum BufError AMFWriteNamedDouble(struct BitBuf *buf, const char* name, size_t length, double value) {
    enum BufError err;
    err = AMFWriteString16(buf, name, length);
    chk_err;

    return AMFWriteDouble(buf, value);
}

enum BufError AMFWriteNamedString(struct BitBuf *buf, const char* name, size_t length, const char* value, size_t length2) {
    enum BufError err;
    err = AMFWriteString16(buf, name, length);
    chk_err;

    return AMFWriteString(buf, value, length2);
}

static enum BufError AMFReadInt16(struct BitBuf* buf, uint32_t* value) {
    if (!buf || buf->offset + 2 > buf->size)
        return BUF_ENDOFBUF_ERROR;

    if (value)
        *value = ((uint32_t)buf->buf[buf->offset] << 8) | buf->buf[buf->offset + 1];

    buf->offset += 2;
    return BUF_OK;
}

static enum BufError AMFReadInt32(struct BitBuf* buf, uint32_t* value) {
    if (!buf || buf->offset + 4 > buf->size)
        return BUF_ENDOFBUF_ERROR;

    if (value)
        *value = ((uint32_t)buf->buf[buf->offset] << 24) |
                 ((uint32_t)buf->buf[buf->offset + 1] << 16) |
                 ((uint32_t)buf->buf[buf->offset + 2] << 8) |
                 buf->buf[buf->offset + 3];

    buf->offset += 4;
    return BUF_OK;
}

enum BufError AMFReadNull(struct BitBuf *buf) {
    if (!buf || buf->offset > buf->size)
        return BUF_ENDOFBUF_ERROR;

    buf->offset += 1;
    return BUF_OK;
}

enum BufError AMFReadUndefined(struct BitBuf *buf) {
    if (!buf || buf->offset > buf->size)
        return BUF_ENDOFBUF_ERROR;

    buf->offset += 1;
    return BUF_OK;
}

enum BufError AMFReadBoolean(struct BitBuf *buf, uint8_t* value) {
    if (!buf || buf->offset + 1 > buf->size)
        return BUF_ENDOFBUF_ERROR;

    if (value)
        *value = buf->buf[buf->offset];

    buf->offset += 1;
    return BUF_OK;
}

enum BufError AMFReadDouble(struct BitBuf *buf, double* value) {
    if (!buf || buf->offset + 8 > buf->size)
        return BUF_ENDOFBUF_ERROR;

    if (value) {
        if (0x00 == *(char*)&s_double) {
            // Little-Endian
            uint8_t* p = (uint8_t*)value;
            for (int i = 0; i < 8; ++i)
                p[i] = buf->buf[buf->offset + 7 - i];
        } else {
            memcpy(value, buf->buf + buf->offset, 8);
        }
    }

    buf->offset += 8;
    return BUF_OK;
}

enum BufError AMFReadString(struct BitBuf *buf, int isLongString, char* string, size_t length) { 
    uint32_t len = 0;
    enum BufError err;

    if (isLongString)
        err = AMFReadInt32(buf, &len);
    else
        err = AMFReadInt16(buf, &len);
    chk_err;

    if (string && length > len) {
        memcpy(string, buf->buf + buf->offset, len);
        string[len] = 0;
    }

    buf->offset += len;
    return BUF_OK;
}

enum BufError AMFReadDate(struct BitBuf *buf, double *milliseconds, int16_t *timezone) {
    enum BufError err;
    uint32_t v = 0;

    err = AMFReadDouble(buf, milliseconds);
    chk_err;

    err = AMFReadInt16(buf, &v);
    chk_err;

    if (timezone)
        *timezone = (int16_t)v;

    return BUF_OK;
}

enum BufError AMF3ReadNull(struct BitBuf *buf) {
    if (!buf || buf->offset > buf->size)
        return BUF_ENDOFBUF_ERROR;

    buf->offset += 1;
    return BUF_OK;
}

enum BufError AMF3ReadBoolean(struct BitBuf *buf, uint8_t *value) {
    if (!buf || buf->offset + 1 > buf->size)
        return BUF_ENDOFBUF_ERROR;

    if (value)
        *value = buf->buf[buf->offset];

    buf->offset += 1;
    return BUF_OK;
}

enum BufError AMF3ReadInteger(struct BitBuf *buf, int32_t* value) {
    uint8_t b;
    int i;
    int32_t v = 0;

    for (i = 0; i < 3; i++) {
        if (buf->offset >= buf->size)
            return BUF_ENDOFBUF_ERROR;

        b = buf->buf[buf->offset++];
        v <<= 7;
        if (!(b & 0x80)) {
            v |= b;
            if (value) *value = v;
            return BUF_OK;
        }
        v |= (b & 0x7F);
    }

    if (buf->offset >= buf->size)
        return BUF_ENDOFBUF_ERROR;

    b = buf->buf[buf->offset++];
    v <<= 8;
    v |= b;
    if (v >= (1 << 28))
        v -= (1 << 29);
    if (value) *value = v;

    return BUF_OK;
}

enum BufError AMF3ReadDouble(struct BitBuf *buf, double* value) {
    if (!buf || buf->offset + 8 > buf->size)
        return BUF_ENDOFBUF_ERROR;

    if (value) {
        if (0x00 == *(char*)&s_double) {
            // Little-Endian
            uint8_t* p = (uint8_t*)value;
            for (int i = 0; i < 8; ++i)
                p[i] = buf->buf[buf->offset + 7 - i];
        } else {
            memcpy(value, buf->buf + buf->offset, 8);
        }
    }

    buf->offset += 8;
    return BUF_OK;
}

enum BufError AMF3ReadString(struct BitBuf *buf, char* string, uint32_t* length) {
    enum BufError err;
    int32_t v = 0;

    err = AMF3ReadInteger(buf, (int32_t*)&v);
    chk_err;

    if (v & 0x01) {
        // reference
        if (length) *length = 0;
        if (string) string[0] = 0;
        return BUF_OK;
    } else {
        uint32_t len = (uint32_t)(v >> 1);

        if (length) *length = len;
        if (buf->offset + len > buf->size)
            return BUF_ENDOFBUF_ERROR;

        if (string) {
            memcpy(string, buf->buf + buf->offset, len);
            string[len] = 0;
        }

        buf->offset += len;
        return BUF_OK;
    }
}

static enum BufError amf_read_object(struct BitBuf* buf, struct amf_object_item_t* items, size_t n);
static enum BufError amf_read_ecma_array(struct BitBuf* buf, struct amf_object_item_t* items, size_t n);
static enum BufError amf_read_strict_array(struct BitBuf* buf, struct amf_object_item_t* items, size_t n);

static enum BufError amf_read_item(struct BitBuf* buf, enum AMFDataType type, struct amf_object_item_t* item) {
    switch (type) {
		case AMF_BOOLEAN:
			return AMFReadBoolean(buf, (uint8_t*)(item ? item->value : NULL));
		case AMF_NUMBER:
			return AMFReadDouble(buf, (double*)(item ? item->value : NULL));
		case AMF_STRING:
			return AMFReadString(buf, 0, (char*)(item ? item->value : NULL), item ? item->size : 0);
		case AMF_LONG_STRING:
			return AMFReadString(buf, 1, (char*)(item ? item->value : NULL), item ? item->size : 0);
		case AMF_DATE:
			return AMFReadDate(buf, (double*)(item ? item->value : NULL), (int16_t*)(item ? (char*)item->value + 8 : NULL));
		case AMF_OBJECT:
			return amf_read_object(buf, (struct amf_object_item_t*)(item ? item->value : NULL), item ? item->size : 0);
		case AMF_NULL:
			return AMFReadNull(buf);
		case AMF_UNDEFINED:
			return AMFReadUndefined(buf);
		case AMF_ECMA_ARRAY:
			return amf_read_ecma_array(buf, (struct amf_object_item_t*)(item ? item->value : NULL), item ? item->size : 0);
		case AMF_STRICT_ARRAY:
			return amf_read_strict_array(buf, (struct amf_object_item_t*)(item ? item->value : NULL), item ? item->size : 0);
		default:
			return BUF_INCORRECT;
    }
}

static inline int amf_read_item_type_check(uint8_t type0, uint8_t itemtype) {
    // decode AMF_ECMA_ARRAY as AMF_OBJECT
    return (type0 == itemtype || (AMF_OBJECT == itemtype && (AMF_ECMA_ARRAY == type0 || AMF_NULL == type0))) ? 1 : 0;
}

static enum BufError amf_read_strict_array(struct BitBuf* buf, struct amf_object_item_t* items, size_t n) {
    enum BufError err;
    uint32_t count, i;

    err = AMFReadInt32(buf, &count); // U32 array-count
    chk_err;

    for (i = 0; i < count && buf && buf->offset < buf->size; i++) {
        if (buf->offset >= buf->size)
            return BUF_ENDOFBUF_ERROR;
    
        uint8_t type = buf->buf[buf->offset++];
        err = amf_read_item(buf, type, (i < n && amf_read_item_type_check(type, items[i].type)) ? &items[i] : NULL);
        chk_err;
    }

    return BUF_OK;
}

static enum BufError amf_read_ecma_array(struct BitBuf* buf, struct amf_object_item_t* items, size_t n) {
    if (!buf || buf->offset + 4 > buf->size)
        return BUF_ENDOFBUF_ERROR;

    buf->offset += 4; // U32 associative-count
    return amf_read_object(buf, items, n);
}

static enum BufError amf_read_object(struct BitBuf* buf, struct amf_object_item_t* items, size_t n) {
    enum BufError err;
    uint32_t len;
    size_t i;

    while (buf && buf->offset + 2 <= buf->size)
    {
        err = AMFReadInt16(buf, &len);
        chk_err;

        if (len == 0) break; // last item

        if (buf->offset + len + 1 > buf->size)
            return BUF_ENDOFBUF_ERROR;

        for (i = 0; i < n; i++) {
            if (strlen(items[i].name) == len &&
                0 == memcmp(items[i].name, buf->buf + buf->offset, len) &&
                amf_read_item_type_check(buf->buf[buf->offset + len], items[i].type))
                break;
        }

        buf->offset += len; // skip name string
        uint8_t type = buf->buf[buf->offset++]; // value type
        err = amf_read_item(buf, type, i < n ? &items[i] : NULL);
        chk_err;
    }

    if (buf && buf->offset < buf->size && AMF_OBJECT_END == buf->buf[buf->offset]) {
        buf->offset += 1;
        return BUF_OK;
    }

    return BUF_ENDOFBUF_ERROR; // invalid object
}

enum BufError amf_read_items(struct BitBuf* buf, struct amf_object_item_t* items, size_t count) {
    enum BufError err;
    size_t i;

    for (i = 0; i < count && buf && buf->offset < buf->size; i++) {
        if (buf->offset >= buf->size)
            return BUF_ENDOFBUF_ERROR;

        uint8_t type = buf->buf[buf->offset++];
        if (!amf_read_item_type_check(type, items[i].type))
            return BUF_INCORRECT;

        err = amf_read_item(buf, type, &items[i]);
        chk_err;
    }

    return BUF_OK;
}