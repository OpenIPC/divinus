#pragma once

#define RTSP_HDR_CONTENTLENGTH     "Content-Length"
#define RTSP_HDR_ACCEPT            "Accept"
#define RTSP_HDR_ALLOW             "Allow"
#define RTSP_HDR_BLOCKSIZE         "Blocksize"
#define RTSP_HDR_CONTENTTYPE       "Content-Type"
#define RTSP_HDR_DATE              "Date"
#define RTSP_HDR_REQUIRE           "Require"
#define RTSP_HDR_TRANSPORTREQUIRE  "Transport-Require"
#define RTSP_HDR_SEQUENCENO        "SequenceNo"
#define RTSP_HDR_CSEQ              "CSeq"
#define RTSP_HDR_STREAM            "Stream"
#define RTSP_HDR_SESSION           "Session"
#define RTSP_HDR_TRANSPORT         "Transport"
#define RTSP_HDR_RANGE             "Range"
#define RTSP_HDR_USER_AGENT        "User-Agent"

#define RTSP_METHOD_MAXLEN         15
#define RTSP_METHOD_DESCRIBE       "DESCRIBE"
#define RTSP_METHOD_ANNOUNCE       "ANNOUNCE"
#define RTSP_METHOD_GET_PARAMETERS "GET_PARAMETERS"
#define RTSP_METHOD_OPTIONS        "OPTIONS"
#define RTSP_METHOD_PAUSE          "PAUSE"
#define RTSP_METHOD_PLAY           "PLAY"
#define RTSP_METHOD_RECORD         "RECORD"
#define RTSP_METHOD_REDIRECT       "REDIRECT"
#define RTSP_METHOD_SETUP          "SETUP"
#define RTSP_METHOD_SET_PARAMETER  "SET_PARAMETER"
#define RTSP_METHOD_TEARDOWN       "TEARDOWN"


enum {
    RTSP_ERR_CONNECTION_CLOSE = -10,
    RTSP_ERR_FATAL,
    RTSP_ERR_EOF,
    RTSP_ERR_UNSUPPORTED_PT,
    RTSP_ERR_NOT_SD,
    RTSP_ERR_INPUT_PARAM,
    RTSP_ERR_ALLOC,
    RTSP_ERR_PARSE,
    RTSP_ERR_NOT_FOUND,
    RTSP_ERR_GENERIC,
    RTSP_ERR_NOERROR
};

enum {
    RTSP_ID_DESCRIBE,
    RTSP_ID_ANNOUNCE,
    RTSP_ID_GET_PARAMETERS,
    RTSP_ID_OPTIONS,
    RTSP_ID_PAUSE,
    RTSP_ID_PLAY,
    RTSP_ID_RECORD,
    RTSP_ID_REDIRECT,
    RTSP_ID_SETUP,
    RTSP_ID_SET_PARAMETER,
    RTSP_ID_TEARDOWN
};