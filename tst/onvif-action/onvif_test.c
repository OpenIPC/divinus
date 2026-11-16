#include "check.h"
#include "onvif.h"

volatile char graceful = 0, keepRunning = 1;
char chip[16] = "TEST";
struct AppConfig app_config;
NetInfo netinfo;

void network_address(char *address) { strcpy(address, "192.0.2.10"); }

static int action_is(const char *request, const char *expect) {
    char *action = onvif_extract_soap_action(request);
    return action && !strcmp(action, expect);
}

int main(void) {
    /* No prefix, with and without attributes */
    CHECK(action_is("<Envelope><Body><GetCapabilities xmlns=\"http://www.onvif.org/ver10/device/wsdl\">"
        "<Category>All</Category></GetCapabilities></Body></Envelope>", "GetCapabilities"));
    CHECK(action_is("<Envelope><Body><GetProfiles></GetProfiles></Body></Envelope>", "GetProfiles"));

    /* Namespace prefixes on the body and on the action */
    CHECK(action_is("<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\">"
        "<s:Body><tds:GetCapabilities xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">"
        "<tds:Category>All</tds:Category></tds:GetCapabilities></s:Body></s:Envelope>",
        "GetCapabilities"));
    CHECK(action_is("<SOAP-ENV:Envelope><SOAP-ENV:Body>\n  <trt:GetProfiles>\n"
        "</trt:GetProfiles></SOAP-ENV:Body></SOAP-ENV:Envelope>", "GetProfiles"));

    /* The prefix may be declared higher up, so there is no space after the name */
    CHECK(action_is("<s:Envelope><s:Body><tds:GetSystemDateAndTime/></s:Body></s:Envelope>",
        "GetSystemDateAndTime"));
    CHECK(action_is("<s:Envelope><s:Body><tds:GetDeviceInformation/></s:Body></s:Envelope>",
        "GetDeviceInformation"));

    /* No action at all */
    CHECK(onvif_extract_soap_action("") == NULL);
    CHECK(onvif_extract_soap_action("<Envelope></Envelope>") == NULL);
    CHECK(onvif_extract_soap_action("<Envelope><Body></Body></Envelope>") == NULL);
    CHECK(onvif_extract_soap_action("<s:Envelope><s:Body/></s:Envelope>") == NULL);
    CHECK(onvif_extract_soap_action("<Envelope><Body>") == NULL);

    CHECK_DONE();
}
