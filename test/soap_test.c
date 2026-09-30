#include "check.h"
#include "hal/tools.h"
#include "soap.h"

int main(void) {
    char text[64];

    CHECK(soap_action("<s:Envelope><s:Body><tds:GetCapabilities/></s:Body></s:Envelope>",
        text, sizeof(text)));
    CHECK(!strcmp(text, "GetCapabilities"));
    CHECK(soap_action("<Envelope><Body>\n  <GetStreamUri xmlns=\"x\"><a/></GetStreamUri></Body></Envelope>",
        text, sizeof(text)));
    CHECK(!strcmp(text, "GetStreamUri"));
    CHECK(!soap_action("<Envelope><Header/></Envelope>", text, sizeof(text)));

    CHECK(soap_tag_text("<wsse:Username>admin</wsse:Username>", "Username", text, sizeof(text)));
    CHECK(!strcmp(text, "admin"));
    CHECK(soap_tag_text("<Username>admin</Username>", "Username", text, sizeof(text)));
    CHECK(!strcmp(text, "admin"));
    CHECK(!soap_tag_text("<UsernameToken><x/></UsernameToken>", "Username", text, sizeof(text)));
    CHECK(!soap_tag_text("<a:Username>0123456789</a:Username>", "Username", text, 8));
    CHECK(soap_tag_text("<tev:Timeout>PT30S</tev:Timeout>", "Timeout", text, sizeof(text)));
    CHECK(!strcmp(text, "PT30S"));
    CHECK(soap_tag_text("<a><wsnt:Filter/></a>", "Filter", text, sizeof(text)));
    CHECK(!strcmp(text, ""));

    CHECK(soap_tag_attr_has("<wsse:Password Type=\"p#PasswordDigest\">x</wsse:Password>",
        "Password", "PasswordDigest"));
    CHECK(!soap_tag_attr_has("<Password Type=\"p#PasswordText\">PasswordDigest</Password>",
        "Password", "PasswordDigest"));

    CHECK(soap_duration("PT30S") == 30);
    CHECK(soap_duration("PT24H") == 86400);
    CHECK(soap_duration("PT1H30M") == 5400);
    CHECK(soap_duration("P1DT1S") == 86401);
    CHECK(soap_duration("PT0.5S") == 0);
    CHECK(soap_duration("30") == -1);
    CHECK(soap_duration("PT5X") == -1);
    CHECK(soap_duration("P5H") == -1);
    CHECK(soap_duration("P30000D") == -1);
    CHECK(soap_duration("PT9999999999S") == -1);
    // 94368760191893771 * 86400 wraps to 128 in 64 bits
    CHECK(soap_duration("P94368760191893771D") == -1);
    CHECK(soap_duration("PT99999999999999999999H") == -1);

    CHECK(soap_datetime("2026-09-28T10:00:00Z") == 1790589600);
    CHECK(soap_datetime("2026-09-28T10:00:00.123Z") == 1790589600);
    CHECK(soap_datetime("2026-09-28T10:00:00") == 1790589600);
    CHECK(soap_datetime("2026-09-28T12:00:00+02:00") == 1790589600);
    CHECK(soap_datetime("2026-09-28T04:30:00.5-05:30") == 1790589600);
    CHECK(soap_datetime("2026-09-28T10:00:00+02") == -1);
    CHECK(soap_datetime("2026-09-28T10:00:00+-2:00") == -1);
    CHECK(soap_datetime("2026-09-28T10:00:00+02:-5") == -1);
    CHECK(soap_datetime("2026-09-28T10:00:00Zjunk") == -1);
    CHECK(soap_datetime("yesterday") == -1);
    soap_datetime_format(1790589600, text, sizeof(text));
    CHECK(!strcmp(text, "2026-09-28T10:00:00Z"));

    time_t now = 1790589600;
    CHECK(soap_term_seconds("<a:InitialTerminationTime>PT24H</a:InitialTerminationTime>",
        "InitialTerminationTime", now, 3600, 86400) == 86400);
    CHECK(soap_term_seconds("<InitialTerminationTime>PT48H</InitialTerminationTime>",
        "InitialTerminationTime", now, 3600, 86400) == 86400);
    CHECK(soap_term_seconds("<x/>", "InitialTerminationTime", now, 3600, 86400) == 3600);
    CHECK(soap_term_seconds("<TerminationTime>junk</TerminationTime>",
        "TerminationTime", now, 3600, 86400) == 3600);
    CHECK(soap_term_seconds("<TerminationTime>2026-09-28T10:01:00Z</TerminationTime>",
        "TerminationTime", now, 3600, 86400) == 60);
    CHECK(soap_term_seconds("<TerminationTime>2026-09-28T09:00:00Z</TerminationTime>",
        "TerminationTime", now, 3600, 86400) == 1);

    char decoded[64];
    CHECK(base64_decode(decoded, "AAECAwQFBgcICQoLDA0ODw==", sizeof(decoded)) == 16);
    CHECK(base64_decode(decoded, "AAECAwQFBgcICQoLDA0O", sizeof(decoded)) == 15);
    CHECK(base64_decode(decoded, "QUI=", sizeof(decoded)) == 2 && !memcmp(decoded, "AB", 2));

    // Vectors computed with Python's base64 and hashlib
    CHECK(soap_digest_valid("AAECAwQFBgcICQoLDA0ODw==", "2026-09-28T10:00:00Z", "secret",
        "ULiogxchk2lxIjGaG/ZQGvrxNZo="));
    CHECK(soap_digest_valid("AAECAwQFBgcICQoLDA0O", "2026-09-28T10:00:00Z", "secret",
        "TeXki4uPUCX4dC42CRxGIPV4Nbo="));
    CHECK(!soap_digest_valid("AAECAwQFBgcICQoLDA0ODw==", "2026-09-28T10:00:00Z", "wrong",
        "ULiogxchk2lxIjGaG/ZQGvrxNZo="));

    CHECK_DONE();
}
