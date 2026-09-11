#include "WiznetW5500.hpp"

#include <cstdio>
#include <cstring>

#include "FreeRTOS.h"
#include "task.h"

// These WIZnet headers are are C but their public headers are written to be C++-safe
#include "wizchip_conf.h"
#include "socket.h"
#include "dhcp.h"
#include "dns.h"
// This WIZnet header is C and is not C++ safe
extern "C" {
#include "wizchip_spi.h"
}

// mbedTLS is also a C library, but its public headers are written to be C++-safe
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"

namespace pico_wiznet {

// Internal W5500 socket indices
// Supports 8 independent sockets simultaneously
// Supports hardwired TCP/IP Protocols:
// TCP, UDP, ICMP, IPv4, ARP, IGMP, PPPoE
constexpr uint8_t DHCP_SOCKET = 0;  // UDP
constexpr uint8_t DNS_SOCKET = 1;   // UDP
constexpr uint8_t TLS_SOCKET = 2;   // TCP

uint8_t dhcp_buffer[2048];
uint8_t dns_buffer[2048];

WiznetW5500::WiznetW5500() = default;

void apply_network_info() {
    wiz_NetInfo net_info{};

    getIPfromDHCP(net_info.ip);
    getGWfromDHCP(net_info.gw);
    getSNfromDHCP(net_info.sn);
    getDNSfromDHCP(net_info.dns);
    getSHAR(net_info.mac);

    net_info.dhcp = NETINFO_DHCP;

    wizchip_setnetinfo(&net_info);
}

void print_network_info() {
    wiz_NetInfo net_info{};

    wizchip_getnetinfo(&net_info);

    printf("IP: %u.%u.%u.%u\n",
        net_info.ip[0],
        net_info.ip[1],
        net_info.ip[2],
        net_info.ip[3]
    );

    printf("Gateway: %u.%u.%u.%u\n",
        net_info.gw[0],
        net_info.gw[1],
        net_info.gw[2],
        net_info.gw[3]
    );

    printf("Subnet: %u.%u.%u.%u\n",
        net_info.sn[0],
        net_info.sn[1],
        net_info.sn[2],
        net_info.sn[3]
    );

    printf("DNS: %u.%u.%u.%u\n",
        net_info.dns[0],
        net_info.dns[1],
        net_info.dns[2],
        net_info.dns[3]
    );

    printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
        net_info.mac[0],
        net_info.mac[1],
        net_info.mac[2],
        net_info.mac[3],
        net_info.mac[4],
        net_info.mac[5]
    );
}

const std::string& WiznetW5500::error_message() const {
    return error_message_;
}

bool WiznetW5500::initialise_dhcp() {
    error_message_.clear();

    printf("W5500: SPI init\n");
    wizchip_spi_initialize();

    printf("W5500: critical section init\n");
    wizchip_cris_initialize();

    printf("W5500: reset\n");
    wizchip_reset();

    printf("W5500: chip init\n");
    wizchip_initialize();

    printf("W5500: version check\n");
    wizchip_check();

    printf("W5500: chip OK\n");

    wiz_NetInfo net_info{};

    const uint8_t mac[6] = {0x02, 0x08, 0xDC, 0x55, 0x00, 0x01};

    std::memcpy(net_info.mac, mac, sizeof(mac));

    net_info.dhcp = NETINFO_DHCP;

    wizchip_setnetinfo(&net_info);

    printf("W5500: DHCP init\n");

    DHCP_init(DHCP_SOCKET, dhcp_buffer);

    TickType_t last_dhcp_tick = xTaskGetTickCount();

    while (true) {
        const TickType_t now = xTaskGetTickCount();

        if ((now - last_dhcp_tick) >= pdMS_TO_TICKS(1000)) {
            DHCP_time_handler();
            last_dhcp_tick = now;
        }

        const int result = DHCP_run();

        if (result == DHCP_IP_LEASED) {
            printf("W5500: DHCP lease obtained\n");

            apply_network_info();
            print_network_info();

            return true;
        }

        if (result == DHCP_FAILED) {
            error_message_ = "DHCP failed";
            return false;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

bool WiznetW5500::resolve_host(const std::string& host, uint8_t out_ip[4]) {
    wiz_NetInfo net_info{};
    wizchip_getnetinfo(&net_info);

    DNS_init(DNS_SOCKET, dns_buffer);

    if (DNS_run(net_info.dns, reinterpret_cast<uint8_t*>(const_cast<char*>(host.c_str())), out_ip) != 1) {
        error_message_ = "DNS lookup failed";
        return false;
    }

    return true;
}

int WiznetW5500::tls_send(
    void* ctx,
    const unsigned char* buf,
    size_t len
) {
    const auto socket_number =
        static_cast<uint8_t>(reinterpret_cast<uintptr_t>(ctx));

    const int32_t result = send(
        socket_number,
        const_cast<uint8_t*>(buf),
        static_cast<uint16_t>(len)
    );

    if (result > 0) {
        return static_cast<int>(result);
    }

    if (result == SOCK_BUSY) {
        return MBEDTLS_ERR_SSL_WANT_WRITE;
    }

    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}

int WiznetW5500::tls_recv(
    void* ctx,
    unsigned char* buf,
    size_t len
) {
    const auto socket_number =
        static_cast<uint8_t>(reinterpret_cast<uintptr_t>(ctx));

    const int32_t result = recv(
        socket_number,
        buf,
        static_cast<uint16_t>(len)
    );

    if (result > 0) {
        return static_cast<int>(result);
    }

    if (result == SOCK_BUSY) {
        return MBEDTLS_ERR_SSL_WANT_READ;
    }

    if (result == SOCKERR_SOCKSTATUS ||
        result == SOCKERR_SOCKCLOSED) {
        return 0;
    }

    printf(
        "W5500 recv failed: %ld\n",
        static_cast<long>(result)
    );

    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}

bool WiznetW5500::https_get(const std::string& host, uint16_t port, const std::string& path,
        const char* ca_certificate_pem, std::string& response) {
    const std::string request =
        "GET " + path + " HTTP/1.1\r\n"
        "Host: " + host + "\r\n"
        "Connection: close\r\n"
        "\r\n";

    return perform_https_request(host, port, request, ca_certificate_pem, response);
}

bool WiznetW5500::https_post(const std::string& host, uint16_t port, const std::string& path,
        const std::string& api_key, const std::string& json_body, const char* ca_certificate_pem,
        std::string& response) {
    const std::string request =
        "POST " + path + " HTTP/1.1\r\n"
        "Host: " + host + "\r\n"
        "Content-Type: application/json\r\n"
        "X-API-KEY: " + api_key + "\r\n"
        "Connection: close\r\n"
        "Content-Length: " + std::to_string(json_body.size()) + "\r\n"
        "\r\n" +
        json_body;

    return perform_https_request(host, port, request, ca_certificate_pem, response);
}

bool WiznetW5500::perform_https_request(const std::string& host, uint16_t port, const std::string& request,
        const char* ca_certificate_pem, std::string& response) {
    error_message_.clear();
    response.clear();

    uint8_t host_ip[4]{};

    if (!resolve_host(host, host_ip)) {
        return false;
    }

    if (socket(TLS_SOCKET, Sn_MR_TCP, 0, 0) != TLS_SOCKET) {
        error_message_ = "socket() failed";
        return false;
    }

    if (connect(TLS_SOCKET, host_ip, port) != SOCK_OK) {
        close(TLS_SOCKET);

        error_message_ = "TCP connect failed";

        return false;
    }

    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt ca;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;

    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_x509_crt_init(&ca);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    bool success = false;

    do {
        static constexpr char PERSONALISATION[] =
            "pico_wiznet_w5500";

        if (mbedtls_ctr_drbg_seed(
                &ctr_drbg,
                mbedtls_entropy_func,
                &entropy,
                reinterpret_cast<const unsigned char*>(PERSONALISATION),
                sizeof(PERSONALISATION) - 1
            ) != 0) {

            error_message_ = "mbedtls_ctr_drbg_seed() failed";

            break;
        }

        if (mbedtls_x509_crt_parse(
                &ca,
                reinterpret_cast<const unsigned char*>(ca_certificate_pem),
                std::strlen(ca_certificate_pem) + 1
            ) != 0) {

            error_message_ = "CA certificate parse failed";

            break;
        }

        if (mbedtls_ssl_config_defaults(
                &conf,
                MBEDTLS_SSL_IS_CLIENT,
                MBEDTLS_SSL_TRANSPORT_STREAM,
                MBEDTLS_SSL_PRESET_DEFAULT
            ) != 0) {

            error_message_ = "mbedtls_ssl_config_defaults() failed";

            break;
        }

        mbedtls_ssl_conf_authmode(
            &conf,
            MBEDTLS_SSL_VERIFY_REQUIRED
        );

        mbedtls_ssl_conf_ca_chain(
            &conf,
            &ca,
            nullptr
        );

        mbedtls_ssl_conf_rng(
            &conf,
            mbedtls_ctr_drbg_random,
            &ctr_drbg
        );

        if (mbedtls_ssl_setup(&ssl, &conf) != 0) {
            error_message_ = "mbedtls_ssl_setup() failed";

            break;
        }

        if (mbedtls_ssl_set_hostname(&ssl, host.c_str()) != 0) {
            error_message_ = "mbedtls_ssl_set_hostname() failed";

            break;
        }

        mbedtls_ssl_set_bio(
            &ssl,
            reinterpret_cast<void*>(
                static_cast<uintptr_t>(TLS_SOCKET)
            ),
            &WiznetW5500::tls_send,
            &WiznetW5500::tls_recv,
            nullptr
        );

        while (true) {
            const int result = mbedtls_ssl_handshake(&ssl);

            if (result == 0) {
                break;
            }

            if (
                result != MBEDTLS_ERR_SSL_WANT_READ &&
                result != MBEDTLS_ERR_SSL_WANT_WRITE
            ) {
                error_message_ = "TLS handshake failed";

                goto cleanup;
            }
        }

        size_t written = 0;

        while (written < request.size()) {
            const int result = mbedtls_ssl_write(
                &ssl,
                reinterpret_cast<const unsigned char*>(
                    request.data() + written
                ),
                request.size() - written
            );

            if (result < 0) {
                if (
                    result == MBEDTLS_ERR_SSL_WANT_READ ||
                    result == MBEDTLS_ERR_SSL_WANT_WRITE
                ) {
                    continue;
                }

                error_message_ = "TLS write failed";

                goto cleanup;
            }

            written += static_cast<size_t>(result);
        }

        char buffer[512];

        while (true) {
            const int result = mbedtls_ssl_read(
                &ssl,
                reinterpret_cast<unsigned char*>(buffer),
                sizeof(buffer)
            );

            if (result > 0) {
                response.append(
                    buffer,
                    static_cast<size_t>(result)
                );

                continue;
            }

            if (
                result == 0 ||
                result == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY
            ) {
                break;
            }

            if (
                result == MBEDTLS_ERR_SSL_WANT_READ ||
                result == MBEDTLS_ERR_SSL_WANT_WRITE
            ) {
                continue;
            }

            printf("mbedtls_ssl_read failed: %d (0x%04X)\n", result, static_cast<unsigned int>(-result));

            error_message_ = "TLS read failed";

            goto cleanup;
        }

        success = true;

    } while (false);

cleanup:

    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_x509_crt_free(&ca);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    close(TLS_SOCKET);

    return success;
}

} // namespace pico_wiznet
