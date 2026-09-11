# pico_wiznet_w5500

A Pico SDK C++ library for using the WIZnet W5500 Ethernet controller with HTTPS via mbedTLS.

The library splits the network stack between the RP2350 and the W5500:

- the **RP2350** handles HTTP, TLS, certificate validation, cryptography, and application logic
- the **W5500** handles TCP, IPv4, ARP, Ethernet framing, and the physical Ethernet interface

The W5500 does not understand HTTPS or TLS. It provides a reliable TCP byte stream over Ethernet, while mbedTLS on the RP2350 performs the TLS protocol and encrypts/decrypts the application data.

---

## Architecture

A typical HTTPS request flows through the system like this:

```text
Application code on RP2350
        |
        v
HTTP / JSON
        |
        v
mbedTLS
        |
        v
tls_send() / tls_recv()
        |
        | SPI
        v
W5500 socket TX/RX buffers
        |
        v
TCP
        |
        v
IPv4
        |
        v
Ethernet MAC
        |
        v
Ethernet PHY
        |
        v
Magnetics / RJ45 / CAT cable
        |
        v
Network / Internet
        |
        v
Remote server
```

---

## OSI Model Mapping

The OSI model is not a perfect one-to-one description of the modern TCP/IP stack, particularly at Layers 5 and 6, but it is useful for visualising how the system works.

| OSI layer | This project |
|---|---|
| 7 - Application | HTTP + JSON |
| 6 - Presentation | TLS encryption / mbedTLS |
| 5 - Session | Effectively handled within TLS, TCP, and the application |
| 4 - Transport | TCP - W5500 hardware |
| 3 - Network | IPv4 - W5500 hardware |
| 2 - Data Link | Ethernet MAC - W5500 hardware |
| 1 - Physical | 10/100 Ethernet PHY - W5500 + magnetics + cable |

---

## Opening the TCP Connection

Before TLS begins, the W5500 opens a plain TCP socket:

```cpp
socket(TLS_SOCKET, Sn_MR_TCP, 0, 0);
connect(TLS_SOCKET, ip, port);
```

The W5500 then performs the TCP connection setup with the remote server:

```text
W5500                              Server

    SYN -------------------------->

        <------------------ SYN + ACK

    ACK -------------------------->
```

At this point the TCP connection is established.

Nothing is encrypted yet.

The W5500 knows about:

- local IP address
- local TCP port
- remote IP address
- remote TCP port
- TCP sequence numbers
- acknowledgements
- retransmissions

It does **not** know about:

- HTTPS
- TLS
- certificates
- public keys
- AES
- session keys
- HTTP
- JSON

Those belong to mbedTLS and the application running on the RP2350.

---

## Connecting mbedTLS to the W5500

mbedTLS is connected to the W5500 using transport callbacks:

```cpp
mbedtls_ssl_set_bio(
    &ssl,
    reinterpret_cast<void*>(static_cast<uintptr_t>(TLS_SOCKET)),
    &WiznetW5500::tls_send,
    &WiznetW5500::tls_recv,
    nullptr
);
```

This tells mbedTLS:

- call `tls_send()` when TLS bytes need to be transmitted
- call `tls_recv()` when TLS bytes need to be received
- pass `TLS_SOCKET` to those callbacks as their context

The callbacks then call the WIZnet socket functions:

```cpp
send(socket_number, buf, len);
recv(socket_number, buf, len);
```

The resulting data path is:

```text
mbedTLS
   |
   | TLS protocol bytes
   v
tls_send()
   |
   v
WIZnet send()
   |
   | SPI
   v
W5500 TX buffer
   |
   v
TCP / IPv4 / Ethernet
```

Incoming data follows the reverse path.

---

# HTTPS Data Flow

## Layer 7 - HTTP and JSON

Suppose the application creates an HTTP request containing JSON:

```http
POST /weather HTTP/1.1
Host: example.com
Content-Type: application/json
X-API-KEY: example-key
Content-Length: ...

{"temperature":12.4}
```

At this point the request is simply plaintext bytes in RP2350 RAM.

Conceptually:

```text
JSON
  |
  v
HTTP request
```

---

## Layer 6 - TLS

The request bytes are passed to:

```cpp
mbedtls_ssl_write(...)
```

By this point the TLS handshake has already completed, so mbedTLS knows how to encrypt the HTTP request.

mbedTLS turns the plaintext HTTP request into one or more encrypted TLS records:

```text
+----------------------+
| TLS record header    |
+----------------------+
| encrypted HTTP data  |
| ******************** |
| ******************** |
+----------------------+
```

To the W5500, the encrypted TLS record is simply a blob of bytes.

The W5500 does not know that those bytes contain HTTP.

---

## Layer 4 - TCP

The encrypted TLS bytes reach the W5500 through `send()` and are placed into the W5500's TX buffer.

The W5500 TCP engine may divide a larger TLS record across several TCP segments.

For example:

```text
Segment 1
+----------------+
| TCP header     |
+----------------+
| payload        |
+----------------+

Segment 2
+----------------+
| TCP header     |
+----------------+
| remaining data |
+----------------+
```

The TCP header contains information such as:

- source port
- destination port
- sequence number
- acknowledgement number
- flags
- receive window
- checksum

Conceptually:

```text
+-------------------------+
| TCP header              |
+-------------------------+
| encrypted TLS record    |
+-------------------------+
```

The protocol nesting is therefore:

```text
JSON
  |
  v
HTTP
  |
  v
TLS
  |
  v
TCP
```

---

## Layer 3 - IPv4

The W5500 wraps the TCP segment inside an IPv4 packet:

```text
+-------------------------+
| IPv4 header             |
+-------------------------+
| TCP header              |
+-------------------------+
| TLS encrypted data      |
+-------------------------+
```

The IPv4 header contains information such as:

- source IP address
- destination IP address
- protocol = TCP
- TTL
- packet length
- header checksum

---

## Layer 2 - Ethernet

The W5500 then places the IP packet inside an Ethernet frame:

```text
+--------------------------+
| Ethernet header          |
+--------------------------+
| IPv4 header              |
+--------------------------+
| TCP header               |
+--------------------------+
| TLS encrypted data       |
+--------------------------+
| Ethernet FCS             |
+--------------------------+
```

The Ethernet header contains:

- destination MAC address
- source MAC address
- EtherType

For IPv4:

```text
EtherType = 0x0800
```

The destination MAC address is normally **not** the MAC address of the remote web server.

MAC addresses are local to a Layer 2 network.

For a server elsewhere on the Internet, the frame is normally sent to the local router:

```text
Destination IP:
203.0.113.20

Destination Ethernet MAC:
router MAC address
```

The W5500 uses ARP to determine the MAC address associated with the local gateway:

```text
192.168.1.1 -> AA:BB:CC:DD:EE:FF
```

---

## Layer 1 - Ethernet PHY

The W5500 includes the Ethernet PHY.

The PHY converts the digital Ethernet frame into the electrical signalling used on twisted-pair Ethernet.

Conceptually:

```text
W5500 digital logic
       |
       v
Ethernet MAC
       |
       v
Ethernet PHY
       |
       v
TX/RX differential signals
       |
       v
Ethernet magnetics
       |
       v
RJ45
       |
       v
CAT cable
```

---

# SPI Between the RP2350 and W5500

SPI is not part of the network OSI stack.

It is simply the local communication bus between the RP2350 and the W5500.

The WIZnet driver uses SPI to access W5500 registers and its internal socket buffers:

```text
RP2350 RAM
    |
    | SPI
    v
W5500 TX buffer
    |
    | TCP/IP processing
    v
W5500 Ethernet MAC
    |
    v
W5500 PHY
    |
    v
Ethernet cable
```

---

# Across the Network

Once the Ethernet frame leaves the device, it may pass through several network devices:

```text
Pico 2
   |
   | SPI
   v
W5500
   |
   | Ethernet
   v
network switch
   |
   v
home router
   |
   v
ISP router
   |
   v
Internet
   |
   v
web server
```

At every router, the Layer 2 framing can change while the Layer 3 IP packet continues towards its destination.

On the local LAN:

```text
[W5500 MAC -> router MAC]
    [IP: client -> server]
        [TCP]
            [TLS]
```

After the router forwards the packet through another network:

```text
[different Layer-2 framing]
    [IP: client -> server]
        [TCP]
            [TLS]
```

Layer 2 is hop-local.

Layer 3 allows the packet to travel between networks.

---

# Receiving the Request at the Server

At the destination, the process happens in reverse.

```text
electrical Ethernet signal
        |
        v
Ethernet frame
        |
        v
IP packet
        |
        v
TCP segment
        |
        v
TLS byte stream
        |
        v
TLS decrypts the data
        |
        v
HTTP request
        |
        v
application / API
```

The server eventually sees something like:

```http
POST /weather HTTP/1.1
Host: ...
X-API-KEY: ...
```

and the application can read the request body.

For example, in PHP:

```php
$raw_json = file_get_contents('php://input');
```

---

# TLS Handshake

Once:

```cpp
connect(TLS_SOCKET, ip, port);
```

has succeeded, there is a normal TCP connection between the W5500 and the server.

Then:

```cpp
mbedtls_ssl_handshake(&ssl);
```

causes mbedTLS on the RP2350 to conduct the TLS handshake through that TCP connection.

The W5500 does not know that these bytes represent TLS.

For a modern TLS 1.3 connection, the handshake is approximately:

```text
RP2350 / mbedTLS                         Server
       |                                   |
       |        TCP already connected      |
       |<=================================>|
       |                                   |
       | ------ ClientHello -------------> |
       |                                   |
       | <----- ServerHello -------------- |
       | <----- EncryptedExtensions -------|
       | <----- Certificate ---------------|
       | <----- CertificateVerify ---------|
       | <----- Finished ------------------|
       |                                   |
       |    validate certificate           |
       |    derive session keys            |
       |                                   |
       | ------ Finished ----------------> |
       |                                   |
       | ===== encrypted application ===== |
       | ------ HTTP POST ---------------> |
       | <----- HTTP response ------------ |
       |                                   |
```

The exact handshake depends on the negotiated TLS version and cipher suite.

---

## 1. TCP Exists Before TLS Begins

The W5500 has already created the TCP connection:

```cpp
socket(TLS_SOCKET, Sn_MR_TCP, 0, 0);
connect(TLS_SOCKET, ip, port);
```

Nothing is encrypted yet.

TLS runs **on top of** this TCP connection.

---

## 2. `mbedtls_ssl_handshake()` Starts

mbedTLS already knows how to access the network because `mbedtls_ssl_set_bio()` has registered the W5500 send and receive callbacks.

Conceptually:

```text
mbedTLS
   |
   | TLS protocol bytes
   v
tls_send()
   |
   v
W5500 send()
   |
   v
TCP
```

The reverse path is used for incoming handshake data.

---

## 3. ClientHello

The RP2350 sends the first TLS handshake message:

```text
ClientHello
```

mbedTLS constructs it.

Conceptually the client says:

> I want to establish a secure connection. Here are the TLS versions and cryptographic algorithms I support, together with the information required to establish keys.

A TLS 1.3 ClientHello can contain:

- supported TLS versions
- supported cipher suites
- supported cryptographic groups
- signature algorithms
- random data
- a key share
- SNI hostname
- other TLS extensions

The SNI hostname comes from:

```cpp
mbedtls_ssl_set_hostname(
    &ssl,
    host.c_str()
);
```

For example:

```text
weather.example.com
```

A conceptual ClientHello might look like:

```text
ClientHello
├── TLS versions
│   └── TLS 1.3
│
├── random
│
├── cipher suites
│   ├── TLS_AES_128_GCM_SHA256
│   └── ...
│
├── supported groups
│   ├── X25519
│   └── secp256r1
│
├── key_share
│   └── client's ephemeral public key
│
└── server_name
    └── weather.example.com
```

---

## 4. ClientHello Reaches the Server

mbedTLS serialises the ClientHello and calls:

```cpp
WiznetW5500::tls_send(...)
```

which ultimately calls:

```cpp
send(TLS_SOCKET, buf, len);
```

The path is:

```text
RP2350 RAM
   |
   | ClientHello bytes
   v
tls_send()
   |
   | SPI
   v
W5500 TX buffer
   |
   v
TCP engine
   |
   v
IPv4
   |
   v
Ethernet
   |
   v
wire
```

The W5500 may split the data across one or more TCP segments.

It does not know that the data is a ClientHello.

---

## 5. ServerHello

The server receives the ClientHello, selects compatible parameters, and sends:

```text
ServerHello
```

This communicates information such as:

- selected TLS version
- selected cipher suite
- server random data
- server ephemeral key share

Conceptually:

```text
ClientHello

"I support A, B and C.
 Here is my ephemeral public key."

             |
             v

ServerHello

"Use B.
 Here is my ephemeral public key."
```

At this point both sides have enough information to independently derive shared cryptographic secret material.

---

## 6. Ephemeral Key Agreement

With a common modern TLS 1.3 configuration, both sides generate ephemeral private/public key pairs.

Conceptually:

```text
Client:
    private key: a
    public key:  A

Server:
    private key: b
    public key:  B
```

The public values are exchanged.

The private values are never transmitted.

Each side combines its own private value with the other side's public value to derive the same shared secret.

Conceptually:

```text
client derives secret S
server derives secret S

network observer cannot feasibly derive S
```

The resulting encryption keys are not sent across Ethernet.

Both endpoints derive them independently.

---

## 7. TLS Derives Handshake Keys

TLS 1.3 does not use the raw shared secret directly as an encryption key.

Instead it passes cryptographic secret material through the TLS key schedule, based around HKDF.

Conceptually:

```text
shared secret
      |
      v
     HKDF
      |
      +--> client handshake traffic secret
      |
      +--> server handshake traffic secret
```

These secrets are used to derive encryption keys and IVs.

`ClientHello` and `ServerHello` are observable on the network.

Subsequent handshake messages are protected.

---

## 8. Server Sends Its Certificate Chain

The server sends its certificate chain.

The certificate contains identity and public-key information used to authenticate the server.

---

## 9. RP2350 Validates the Certificate

The project configures mbedTLS with a trusted CA certificate:

```cpp
mbedtls_x509_crt_parse(...);
mbedtls_ssl_conf_ca_chain(...);
mbedtls_ssl_conf_authmode(...);
mbedtls_ssl_set_hostname(...);
```

The intention is not merely:

> Encrypt the connection.

It is also:

> Prove that the server is genuinely the server I intended to contact.

mbedTLS validates the certificate chain and hostname according to the configured trust information.

---

## 10. CertificateVerify

In TLS 1.3 the server sends:

```text
CertificateVerify
```

This proves that the server possesses the private key associated with its authenticated identity.

Conceptually:

```text
handshake transcript
        |
        v
server signs transcript information
        |
        v
CertificateVerify
```

The client verifies that signature using the corresponding public key.

---

## 11. Server Finished

The server then sends:

```text
Finished
```

This proves that the server:

- participated in this exact handshake
- derived the expected handshake secrets
- saw the same handshake transcript

Conceptually:

```text
ClientHello
ServerHello
EncryptedExtensions
Certificate
CertificateVerify
        |
        v
cryptographic transcript hash
        |
        +
derived key
        |
        v
Finished
```

This protects the integrity of the handshake.

---

## 12. RP2350 Verifies Server Finished

The data arrives through:

```cpp
tls_recv()
```

and ultimately:

```cpp
recv(TLS_SOCKET, ...);
```

mbedTLS decrypts the record and verifies the expected Finished value.

If this verification fails, the TLS handshake fails.

---

## 13. RP2350 Sends Its Finished

The client sends its own:

```text
Finished
```

Conceptually it says:

> I derived the same secrets and observed the same authenticated handshake.

At this point both sides agree on:

```text
server identity               ✓
cryptographic algorithms      ✓
shared secret                 ✓
handshake integrity           ✓
traffic keys                  ✓
```

The TLS connection is established.

---

## 14. Application Traffic Keys

TLS derives keys for encrypted application data.

Conceptually:

```text
shared cryptographic state
            |
            v
           HKDF
            |
       +----+----+
       |         |
       v         v
 client app   server app
 traffic key  traffic key
```

The two directions use distinct cryptographic material:

```text
client -> server key
server -> client key
```

The session keys remain inside mbedTLS on the RP2350.

They are never given to the W5500.

---

## 15. Sending the HTTP Request

After:

```cpp
mbedtls_ssl_handshake(&ssl);
```

returns successfully, the application can call:

```cpp
mbedtls_ssl_write(...)
```

The data path becomes:

```text
HTTP plaintext
      |
      v
mbedTLS
      |
      | encrypt + authenticate
      v
TLS application-data record
      |
      v
tls_send()
      |
      | SPI
      v
W5500
      |
      v
TCP / IPv4 / Ethernet
```

The W5500 only sees ciphertext.

---

# What a Packet Capture Can See

A packet capture on the Ethernet network can still see metadata such as:

```text
Ethernet
    source MAC
    destination MAC

IPv4
    source IP
    destination IP

TCP
    source port
    destination port 443

TLS
    encrypted application data
```

It can therefore infer information such as:

- client IP address
- server IP address
- TCP port
- packet sizes
- packet timing
- that TLS is being used

It cannot ordinarily see the protected application data, such as:

```text
POST /weather
X-API-KEY
JSON body
temperature value
```

---

# The W5500's View of the TLS Handshake

The W5500 has no knowledge of TLS semantics.

During the handshake, its view is approximately:

```text
ClientHello:
    "send these bytes"

ServerHello:
    "I received some bytes"

Certificate:
    "I received more bytes"

Key establishment:
    "I have no idea what these bytes mean"

HTTP POST:
    "send these bytes"
```

Its job is simply to provide a reliable, ordered TCP byte stream.

mbedTLS gives those bytes meaning.

---

# SPI During the TLS Handshake

SPI is used continuously while the handshake runs.

Outbound:

```text
mbedTLS on RP2350
        |
        | TLS handshake / encrypted bytes
        v
    tls_send()
        |
        v
  WIZnet send()
        |
        | SPI
        v
 W5500 TX RAM
        |
        v
 TCP / IPv4 / Ethernet
        |
        v
      server
```

Inbound:

```text
server
   |
Ethernet / TCP
   |
   v
W5500 RX RAM
   |
   | SPI
   v
WIZnet recv()
   |
   v
tls_recv()
   |
   v
mbedTLS
   |
   | decrypt / verify / parse
   v
RP2350
```

During:

```cpp
mbedtls_ssl_handshake(&ssl);
```

there may be many calls to `tls_send()` and `tls_recv()`.

mbedTLS may return:

```cpp
MBEDTLS_ERR_SSL_WANT_READ
```

or:

```cpp
MBEDTLS_ERR_SSL_WANT_WRITE
```

This essentially means:

> The handshake has not failed. More transport I/O is required before the TLS state machine can continue.

---

# Complete TCP + TLS Flow

```text
            RP2350                       W5500                    SERVER
              |                             |                        |
              | socket/connect ------------>|                        |
              |                             |--- TCP SYN ----------->|
              |                             |<-- SYN/ACK ------------|
              |                             |--- ACK --------------->|
              |                             |                        |
              |                             |    TCP ESTABLISHED     |
              |                             |                        |
        mbedtls_ssl_handshake()             |                        |
              |                             |                        |
create        |                             |                        |
ClientHello   |                             |                        |
              | tls_send() ---------------->|                        |
              |         SPI                 |--- ClientHello ------->|
              |                             |                        |
              |                             |<-- ServerHello --------|
              |<--------------- tls_recv() -|                        |
              |                             |                        |
        derive handshake secrets            |                        |
              |                             |                        |
              |                             |<-- Certificate --------|
              |<--------------- tls_recv() -|                        |
              |                             |                        |
        validate CA chain                   |                        |
        validate hostname                   |                        |
        verify server signature             |                        |
              |                             |                        |
              |                             |<-- Finished -----------|
              |<--------------- tls_recv() -|                        |
              |                             |                        |
        verify Finished                     |                        |
        derive application keys             |                        |
              |                             |                        |
              | tls_send() ---------------->|--- Finished ---------->|
              |                             |                        |
        TLS ESTABLISHED                     |                        |
              |                             |                        |
HTTP POST     |                             |                        |
plaintext     |                             |                        |
      |       |                             |                        |
      v       |                             |                        |
   encrypt    |                             |                        |
      |       |                             |                        |
      v       |                             |                        |
TLS ciphertext                              |                        |
              | tls_send() ---------------->|                        |
              |        SPI                  |--- TCP/TLS ----------->|
              |                             |                        |
              |                             |<-- encrypted HTTP -----|
              |<--------------- tls_recv() -|                        |
              |                             |                        |
        decrypt                             |                        |
              |                             |                        |
        HTTP response                       |                        |
```

---

# Division of Responsibility

## RP2350 / mbedTLS

The RP2350 is responsible for:

```text
HTTP
JSON
server identity
certificates
certificate validation
TLS state machine
cryptography
session keys
encryption
decryption
```

## W5500

The W5500 is responsible for:

```text
TCP connection
sequencing
acknowledgements
retransmission
IPv4
ARP
Ethernet framing
MAC
PHY
```

The most important separation is:

> **TLS session keys never travel through the W5500 or across the network.**

The RP2350 and server independently derive matching cryptographic secrets during the TLS handshake.

The W5500 merely transports the handshake messages and subsequent ciphertext reliably over TCP.

---

# W5500 Compared with Wi-Fi + lwIP

With a Pico W or Pico 2 W using lwIP, considerably more of the TCP/IP stack runs in software around the RP2350.

With the W5500, much of the lower network stack is handled by the Ethernet controller itself.

That leaves the RP2350 responsible for the application and security layers while the W5500 handles the lower transport, network, data-link, and physical layers.
