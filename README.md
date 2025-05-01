# HTTP

The Hypertext Transfer Protocol (HTTP) is an application layer protocol used
for transmitting hypermedia documents, such as HTML. It is stateless, meaning
each request from a client is treated as an independent transaction, without any
context from previous requests.

<https://developer.mozilla.org/en-US/docs/Web/HTTP/Resources_and_specifications>

A socket is an endpoint for sending or receiving data across a computer
network. It is defined by an IP address and a port number. In this case,
we're using TCP sockets for reliable communication. The Transmission Control
Protocol (TCP) is part of the Internet Protocol Suite (TCP/IP). It is a
connection-oriented protocol that ensures data is sent and received reliably and
in order.

An HTTP request typically includes:

- Method: (e.g., GET, POST) indicates the desired action.
  <https://developer.mozilla.org/en-US/docs/Web/HTTP/Methods>

- Path: The resource being requested (e.g., `/index.html`).

- Headers: Additional information about the request (e.g., `Accept-Encoding`).
  <https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers>

- Body: (optional) Contains data sent by the client, primarily in POST requests.

An HTTP response includes:

- Status Line: Indicates the status of the response (e.g., `HTTP/1.1 200 OK`).
  <https://developer.mozilla.org/en-US/docs/Web/HTTP/Status>

- Headers: Information about the response (e.g., `Content-Type`, `Content-Encoding`).

- Body: The actual content being returned (e.g., HTML, JSON).

flow of network
---------------

HTTP -> TLS -> TCP -> IP -> NIC -> Router

you call `get https://www.example.com` from your code or script.
let’s say the above is nu shell.

nu parses the url and figures out:

1. it has to make an HTTP GET request
2. it needs to resolve the domain name
3. it's HTTPS, so TLS handshake is required

nu shell doesn't implement HTTP, TLS, or DNS itself. it uses lower-level
system libs (like `libcurl`, `reqwest`, sockets).

*dns resolution*

first step is dns resolution for the url host. `getaddrinfo()` is called
(via glibc or musl). it resolves the domain name to an IP address.

for this the following is done

- check `/etc/hosts`
- check local DNS cache
- if not found, send UDP packet to DNS server (`8.8.8.8` or from `/etc/resolv.conf`)

DNS request/response happens over UDP (port 53), handled by kernel’s networking
stack. DNS server replies with an IP, e.g. `93.184.216.34`

*tcp connection*

now that we have IP of the url. the client calls `connect()` with the resolved
IP + port (443 for HTTPS). for this the kernel does the following

- allocates socket
- starts TCP 3-way handshake:
    - SYN -> SYN-ACK -> ACK
- assigns ephemeral port
- sets sequence/ack numbers
- opens TCP socket

packets are handed to NIC driver for transmission

*tls handshake*

TLS happens after TCP is connected. libs like OpenSSL or Rustls takes over:

- client sends `ClientHello`
- server sends `ServerHello`, certs
- key exchange (e.g., ECDHE)
- session keys negotiated
- encrypted tunnel established

now that we have a connection. nu or the lib it is using (most likely reqwest
or libcurl) builds the http request.

```txt
GET / HTTP/1.1
Host: www.example.com
User-Agent: Nu/Shell
```

this is then encrypted with TLS
-> wrapped in TCP segment
-> inside IP packet
-> into Ethernet frame
-> to NIC

*nic and physical layer*

NIC takes Ethernet frame and converts the binary data to electrical signals
(ethernet) or to radio waves (wifi) which is sent local router/switch.

*router and forwarding*

router receives frame then reads Ethernet header then reads IP header to check
dest IP. it decrements TTL and forwards to next hop until it reaches web
server.

---

the server receives packets. then following happens

TCP reassembly -> TLS decryption -> HTTP request parsing

server sends HTTP response (HTML, JSON, etc.) and it goes through same layers
in reverse

to implement http server
------------------------

- bind to a specific ip and port.
- use a loop to listen for incoming connections from clients.
- read data from incoming connection and parse it
- based on the incoming information perform approprite action
- send appropriate request back.

additional things that can/should be implemented

- logging
- use thread-pool or thread per request to handle concurrent connections
- implement compression
- security (things like path traversal, CSRF, HTTPS implementation, etc etc)

## basic HTTP server

_this is just to glance at in linear manner. this is not even remotely robust.
the `src/main.rs` contains another version of similarly limited HTTP server._

```rust
use std::fs::File;
use std::io::{prelude::*, BufReader};
use std::net::{TcpListener, TcpStream};
use std::thread;

fn handle_client(mut stream: TcpStream) {
    let mut buffer = [0; 512];

    // read the request from the client
    match stream.read(&mut buffer) {
        Ok(_) => {
            // convert the request buffer to a string
            let request = String::from_utf8_lossy(&buffer[..]);
            let mut lines = request.lines();

            // get the first line and extract the HTTP method and path
            let request_line = lines.next().unwrap_or("");
            if request_line.is_empty() {
                let response = "HTTP/1.1 400 BAD REQUEST\r\nContent-Type: text/plain\r\n\r\nBad Request";
                stream.write_all(response.as_bytes()).unwrap();
                return;
            }

            let method = request_line.split_whitespace().next().unwrap_or("");
            // extract path from request line.
            let mut path = "/";
            let path = request_line.split_whitespace().nth(1).unwrap_or("/");
            println!("Request Method: {}, Request Path: {}", method, path);

            let mut content_length = 0;

            // read headers and get content length
            for line in lines {
                if line.is_empty() {
                    break; // End of headers
                }
                if line.to_lowercase().starts_with("content-length:") {
                    if let Some(length_str) = line.split(':').nth(1) {
                        content_length = length_str.trim().parse().unwrap_or(0);
                    }
                }
            }

            // read the body if Content-Length is greater than 0
            let mut body = String::new();
            if content_length > 0 {
                let mut body_buffer = vec![0; content_length];
                if let Err(e) = stream.read_exact(&mut body_buffer) {
                    eprintln!("Failed to read request body: {}", e);
                    let response = "HTTP/1.1 400 BAD REQUEST\r\nContent-Type: text/plain\r\n\r\nFailed to read request body";
                    stream.write_all(response.as_bytes()).unwrap();
                    return;
                }
                body = String::from_utf8_lossy(&body_buffer).to_string();
                println!("Request Body: {}", body);
            }

            // handle specific resource "/file"
            if path == "/file" {
                match File::open("dummy.txt") {
                    Ok(file) => {
                        let mut reader = BufReader::new(file);
                        let mut file_content = String::new();
                        if let Err(e) = reader.read_to_string(&mut file_content) {
                            eprintln!("Failed to read file: {}", e);
                            let response = "HTTP/1.1 500 INTERNAL SERVER ERROR\r\nContent-Type: text/plain\r\n\r\nFailed to read file";
                            stream.write_all(response.as_bytes()).unwrap();
                            return;
                        }
                        let response = format!(
                            "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: {}\r\n\r\n{}",
                            file_content.len(),
                            file_content
                        );
                        stream.write_all(response.as_bytes()).unwrap();
                    }
                    Err(e) => {
                        eprintln!("File not found: {}", e);
                        let response = "HTTP/1.1 404 NOT FOUND\r\nContent-Type: text/plain\r\n\r\nFile not found";
                        stream.write_all(response.as_bytes()).unwrap();
                    }
                }
            } else {
                // default response for any other path
                let response_body = format!(
                    "<html><body><h1>Hello, World!</h1><p>You requested the path: {}</p><h2>Request Body:</h2><pre>{}</pre></body></html>",
                    path, body
                );
                let response = format!(
                    "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: {}\r\n\r\n{}",
                    response_body.len(),
                    response_body
                );
                stream.write_all(response.as_bytes()).unwrap();
            }
        }
        // repond with Internal Server Error when reading fails
        Err(e) => {
            eprintln!("Failed to read from stream: {}", e);
            let response = "HTTP/1.1 500 INTERNAL SERVER ERROR\r\nContent-Type: text/plain\r\n\r\nFailed to read request";
            stream.write_all(response.as_bytes()).unwrap();
        }
    }
}

fn main() {
    // bind to a port
    let listener = TcpListener::bind("127.0.0.1:7878").unwrap();
    println!("Server running on 127.0.0.1:7878");

    // listen for incoming requests
    for stream in listener.incoming() {
        match stream {
            Ok(stream) => {
                // spawn a thread for handling the request
                thread::spawn(move || {
                    handle_client(stream);
                });
            }
            Err(e) => {
                eprintln!("Failed to accept connection: {}", e);
            }
        }
    }
}
```
