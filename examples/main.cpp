#include <cstdio>
#include <string>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "WiznetW5500.hpp"
#include "WebServerCertificate.hpp"
#include "secrets.hpp"

void ethernet_task(void*) {
    pico_wiznet::WiznetW5500 ethernet;

    printf("Initialising W5500 Ethernet...\n");

    if (!ethernet.initialise_dhcp()) {
        printf("Ethernet failed: %s\n", ethernet.error_message().c_str());
        vTaskDelete(nullptr);
    }

    printf("Ethernet ready\n");

    for (;;) {
        std::string response;

        if (ethernet.https_get(secrets::WEB_HOST, secrets::WEB_PORT, secrets::WEB_PATH_GET,
                web_server_certificate::CA_CERTIFICATE, response)) {
            printf("GET successful\n");
            printf("%s\n", response.c_str());
        }
        else {
            printf("GET failed: %s\n", ethernet.error_message().c_str());
        }

        const std::string json = R"({"device":"w5500-evb-pico2","message":"hello"})";

        if (ethernet.https_post(secrets::WEB_HOST, secrets::WEB_PORT, secrets::WEB_PATH_POST, secrets::API_KEY, json,
                web_server_certificate::CA_CERTIFICATE, response)) {
            printf("POST successful\n");
            printf("%s\n", response.c_str());
        } else {
            printf("POST failed: %s\n", ethernet.error_message().c_str());
        }

        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

int main() {
    stdio_init_all();

    sleep_ms(2000);

    xTaskCreate(ethernet_task, "ethernet", 4096, nullptr, 2, nullptr);

    vTaskStartScheduler();

    while (true) { tight_loop_contents(); }
}
