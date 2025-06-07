#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "bitbuf.h"

enum AMFDataType
{
    AMF_NUMBER = 0x00,
    AMF_BOOLEAN,
    AMF_STRING,
    AMF_OBJECT,
    AMF_MOVIECLIP,
    AMF_NULL,
    AMF_UNDEFINED,
    AMF_REFERENCE,
    AMF_ECMA_ARRAY,
    AMF_OBJECT_END,
    AMF_STRICT_ARRAY,
    AMF_DATE,
    AMF_LONG_STRING,
    AMF_UNSUPPORTED,
    AMF_RECORDSET,
    AMF_XML_DOCUMENT,
    AMF_TYPED_OBJECT,
    AMF_AVMPLUS_OBJECT,
};

enum AMF3DataType
{
    AMF3_UNDEFINED = 0x00,
    AMF3_NULL,
    AMF3_FALSE,
    AMF3_TRUE,
    AMF3_INTEGER,
    AMF3_DOUBLE,
    AMF3_STRING,
    AMF3_XML_DOCUMENT,
    AMF3_DATE,
    AMF3_ARRAY,
    AMF3_OBJECT,
    AMF3_XML,
    AMF3_BYTE_ARRAY,
    AMF3_VECTOR_INT,
    AMF3_VECTOR_UINT,
    AMF3_VECTOR_DOUBLE,
    AMF3_VECTOR_OBJECT,
    AMF3_DICTIONARY,
};

enum BufError AMFWriteNull(struct BitBuf* buf);
enum BufError AMFWriteUndefined(struct BitBuf* buf);
enum BufError AMFWriteObject(struct BitBuf* buf);
enum BufError AMFWriteObjectEnd(struct BitBuf* buf);
enum BufError AMFWriteTypedObject(struct BitBuf* buf);
enum BufError AMFWriteECMAArray(struct BitBuf* buf);

enum BufError AMFWriteBoolean(struct BitBuf* buf, uint8_t value);
enum BufError AMFWriteDouble(struct BitBuf* buf, double value);
enum BufError AMFWriteString(struct BitBuf* buf, const char* string, size_t length);
enum BufError AMFWriteDate(struct BitBuf *buf, double milliseconds, int16_t timezone);

enum BufError AMFWriteNamed(struct BitBuf *buf, const char* name, size_t length);
enum BufError AMFWriteNamedString(struct BitBuf *buf, const char* name, size_t length, const char* value, size_t length2);
enum BufError AMFWriteNamedDouble(struct BitBuf *buf, const char* name, size_t length, double value);
enum BufError AMFWriteNamedBoolean(struct BitBuf *buf, const char* name, size_t length, uint8_t value);

enum BufError AMFReadNull(struct BitBuf *buf);
enum BufError AMFReadUndefined(struct BitBuf *buf);
enum BufError AMFReadBoolean(struct BitBuf *buf, uint8_t* value);
enum BufError AMFReadDouble(struct BitBuf *buf, double* value);
enum BufError AMFReadString(struct BitBuf *buf, int isLongString, char* string, size_t length);
enum BufError AMFReadDate(struct BitBuf *buf, double *milliseconds, int16_t *timezone);

enum BufError AMF3ReadNull(struct BitBuf *buf);
enum BufError AMF3ReadBoolean(struct BitBuf *buf, uint8_t *value);
enum BufError AMF3ReadInteger(struct BitBuf *buf, int32_t* value);
enum BufError AMF3ReadDouble(struct BitBuf *buf, double* value);
enum BufError AMF3ReadString(struct BitBuf *buf, char* string, uint32_t* length);

struct amf_object_item_t
{
    enum AMFDataType type;
    const char* name;
    void* value;
    size_t size;
};
static enum BufError amf_read_item(struct BitBuf* buf, enum AMFDataType type, struct amf_object_item_t* item);