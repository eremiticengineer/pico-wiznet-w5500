#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace pico_wiznet {

class WiznetW5500 {

public:
    WiznetW5500();

    bool initialise_dhcp();

    bool resolve_host(const std::string& host, uint8_t out_ip[4]);

    bool https_get(
        const std::string& host,
        uint16_t port,
        const std::string& path,
        const char* ca_certificate_pem,
        std::string& response
    );

    bool https_post(
        const std::string& host,
        uint16_t port,
        const std::string& path,
        const std::string& api_key,
        const std::string& json_body,
        const char* ca_certificate_pem,
        std::string& response
    );

    const std::string& error_message() const;

private:
    bool perform_https_request(
        const std::string& host,
        uint16_t port,
        const std::string& request,
        const char* ca_certificate_pem,
        std::string& response
    );

    static int tls_send(void* ctx, const unsigned char* buf, size_t len);
    static int tls_recv(void* ctx, unsigned char* buf, size_t len);

    std::string error_message_;
};

} // namespace pico_wiznet