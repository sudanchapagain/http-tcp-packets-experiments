<div align="center">
   <h1>HTTP server</h1>
</div>

## Basics

A **socket** is an endpoint for sending or receiving data across a computer
network. It is defined by an **IP** address and a port number. In this case,
we're using TCP sockets for reliable communication. The **Transmission Control
Protocol** (TCP) is part of the Internet Protocol Suite (TCP/IP). It is a
connection-oriented protocol that ensures data is sent and received reliably and
in order.

## What is HTTP?

The **Hypertext Transfer Protocol** (HTTP) is an application layer protocol used
for transmitting hypermedia documents, such as HTML. It is stateless, meaning
each request from a client is treated as an independent transaction, without any
context from previous requests.

[resources & specifications](https://developer.mozilla.org/en-US/docs/Web/HTTP/Resources_and_specifications)

## HTTP Request and Response Structure

**Request**: An HTTP request typically includes:
- Method: (e.g., GET, POST) indicates the desired action. [for more](https://developer.mozilla.org/en-US/docs/Web/HTTP/Methods)
- Path: The resource being requested (e.g., `/index.html`).
- Headers: Additional information about the request (e.g., `Accept-Encoding`). [for more](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers)
- Body: (optional) Contains data sent by the client, primarily in POST requests.

**Response**: An HTTP response includes:
- Status Line: Indicates the status of the response (e.g., `HTTP/1.1 200 OK`). [for more](https://developer.mozilla.org/en-US/docs/Web/HTTP/Status)
- Headers: Information about the response (e.g., `Content-Type`, `Content-Encoding`).
- Body: The actual content being returned (e.g., HTML, JSON).

## Implementing a HTTP server

- bind to a specific ip and port.
- use a loop to listen for incoming connections from clients.
- read data from incoming connection and parse it
- based on the incoming information perform approprite action
- send appropriate request back.

Additional things that can/should be implemented

- logging
- use thread-pool or thread per request to handle concurrent connections
- implement compression
- security (things like path traversal, CSRF, HTTPS implementation, etc etc)

## Minimal and Extremely basic HTTP Server

> [!IMPORTANT]
> this is just to glance at in linear manner. this is not even remotely robust.
> the `src/main.rs` contains another version of similarly limited HTTP server.

```rust
use std::fs::File;
use std::io::{prelude::*, BufReader};
use std::net::{TcpListener, TcpStream};
use std::thread;

fn handle_client(mut stream: TcpStream) {
    let mut buffer = [0; 512];

    // Read the request from the client
    match stream.read(&mut buffer) {
        Ok(_) => {
            // Convert the request buffer to a string
            let request = String::from_utf8_lossy(&buffer[..]);
            let mut lines = request.lines();

            // Get the first line and extract the HTTP method and path
            let request_line = lines.next().unwrap_or("");
            if request_line.is_empty() {
                let response = "HTTP/1.1 400 BAD REQUEST\r\nContent-Type: text/plain\r\n\r\nBad Request";
                stream.write_all(response.as_bytes()).unwrap();
                return;
            }

            let method = request_line.split_whitespace().next().unwrap_or("");
            // Extract path from request line.
            let mut path = "/";
            let path = request_line.split_whitespace().nth(1).unwrap_or("/");
            println!("Request Method: {}, Request Path: {}", method, path);

            let mut content_length = 0;

            // Read headers and get content length
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

            // Read the body if Content-Length is greater than 0
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

            // Handle specific resource "/file"
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
                // Default response for any other path
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
        // Repond with Internal Server Error when reading fails
        Err(e) => {
            eprintln!("Failed to read from stream: {}", e);
            let response = "HTTP/1.1 500 INTERNAL SERVER ERROR\r\nContent-Type: text/plain\r\n\r\nFailed to read request";
            stream.write_all(response.as_bytes()).unwrap();
        }
    }
}

fn main() {
    // Bind to a port
    let listener = TcpListener::bind("127.0.0.1:7878").unwrap();
    println!("Server running on 127.0.0.1:7878");

    // Listen for incoming requests
    for stream in listener.incoming() {
        match stream {
            Ok(stream) => {
                // Spawn a thread for handling the request
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
