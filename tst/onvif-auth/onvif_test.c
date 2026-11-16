#include "check.h"
#include "onvif.h"

volatile char graceful = 0, keepRunning = 1;
char chip[16] = "TEST";
struct AppConfig app_config;
NetInfo netinfo;

void network_address(char *address) { strcpy(address, "192.0.2.10"); }

/* A UsernameToken as sent by a client that writes namespace prefixes. The
   nonce is the base64 text of the 16 bytes 0x00 .. 0x0F. */
static const char *tokenFormat =
    "<s:Envelope><s:Header><wsse:Security>"
    "<wsse:UsernameToken>"
    "<wsse:Username>%s</wsse:Username>"
    "<wsse:Password Type=\"http://docs.oasis-open.org/wss/2004/01/"
    "oasis-200401-wss-username-token-profile-1.0#PasswordDigest\">%s</wsse:Password>"
    "<wsse:Nonce EncodingType=\"http://docs.oasis-open.org/wss/2004/01/"
    "oasis-200401-wss-soap-message-security-1.0#Base64Binary\">AAECAwQFBgcICQoLDA0ODw==</wsse:Nonce>"
    "<wsu:Created>2024-01-01T00:00:00Z</wsu:Created>"
    "</wsse:UsernameToken></wsse:Security></s:Header>"
    "<s:Body><tds:GetProfiles/></s:Body></s:Envelope>";

static void digest_of(char *out, const char *created, const char *pass) {
    char nonce[16], hash[SHA1_DIGEST_SIZE];
    sha1_context ctx;
    for (int i = 0; i < 16; i++) nonce[i] = i;
    sha1_init(&ctx);
    sha1_update(&ctx, (unsigned char *)nonce, 16);
    sha1_update(&ctx, (unsigned char *)created, strlen(created));
    sha1_update(&ctx, (unsigned char *)pass, strlen(pass));
    sha1_final(hash, &ctx);
    base64_encode(out, hash, SHA1_DIGEST_SIZE);
}

int main(void) {
    char digest[64], request[4096];

    strcpy(app_config.onvif_auth_user, "admin");
    strcpy(app_config.onvif_auth_pass, "secret");
    digest_of(digest, "2024-01-01T00:00:00Z", "secret");

    /* A digest computed with the right password is accepted */
    snprintf(request, sizeof(request), tokenFormat, "admin", digest);
    CHECK(onvif_validate_soap_auth(request));

    /* A wrong password, a wrong user name and a wrong digest are refused */
    digest_of(digest, "2024-01-01T00:00:00Z", "other");
    snprintf(request, sizeof(request), tokenFormat, "admin", digest);
    CHECK(!onvif_validate_soap_auth(request));
    digest_of(digest, "2024-01-01T00:00:00Z", "secret");
    snprintf(request, sizeof(request), tokenFormat, "root", digest);
    CHECK(!onvif_validate_soap_auth(request));
    snprintf(request, sizeof(request), tokenFormat, "admin", "AAAAAAAAAAAAAAAAAAAAAAAAAAA=");
    CHECK(!onvif_validate_soap_auth(request));

    /* Without prefixes */
    CHECK(onvif_validate_soap_auth(
        "<Envelope><Header><Security><UsernameToken><Username>admin</Username>"
        "<Password Type=\"#PasswordText\">secret</Password></UsernameToken>"
        "</Security></Header><Body><GetProfiles/></Body></Envelope>"));
    CHECK(!onvif_validate_soap_auth(
        "<Envelope><Header><Security><UsernameToken><Username>admin</Username>"
        "<Password Type=\"#PasswordText\">wrong</Password></UsernameToken>"
        "</Security></Header><Body><GetProfiles/></Body></Envelope>"));

    /* A password element without a Type attribute used to crash the check */
    CHECK(onvif_validate_soap_auth(
        "<Header><Username>admin</Username><Password>secret</Password></Header>"));

    /* Values that do not fit the buffers are refused instead of overflowing */
    {
        char longName[1200];
        memset(longName, 'a', sizeof(longName) - 1);
        longName[sizeof(longName) - 1] = '\0';
        snprintf(request, sizeof(request), tokenFormat, longName, digest);
        CHECK(!onvif_validate_soap_auth(request));
        snprintf(request, sizeof(request),
            "<Header><Username>admin</Username><Password>%s</Password></Header>", longName);
        CHECK(!onvif_validate_soap_auth(request));
    }

    /* Missing elements */
    CHECK(!onvif_validate_soap_auth("<Body><GetProfiles/></Body>"));
    CHECK(!onvif_validate_soap_auth("<Header><Username>admin</Username></Header>"));

    CHECK_DONE();
}
