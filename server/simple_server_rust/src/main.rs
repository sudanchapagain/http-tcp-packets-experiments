use std::{
    collections::HashMap,
    error::Error,
    fs::File,
    io::{BufReader, Read, Write},
    net::{TcpListener, TcpStream},
    str,
};

#[derive(Debug)]
struct HttpRequest {
    method: String,
    path: String,
    headers: Vec<(String, String)>,
    body: String,
}

#[derive(Debug)]
struct HttpResponse {
    status: HttpStatus,
    headers: Vec<(String, String)>,
    body: String,
}

#[derive(Debug)]
enum HttpStatus {
    Ok,
    NotFound,
    BadRequest,
    NotImplemented,
    InternalServerError,
}

impl HttpStatus {
    fn code(&self) -> u16 {
        match self {
            HttpStatus::Ok => 200,
            HttpStatus::NotFound => 404,
            HttpStatus::BadRequest => 400,
            HttpStatus::NotImplemented => 501,
            HttpStatus::InternalServerError => 500,
        }
    }

    fn message(&self) -> &str {
        match self {
            HttpStatus::Ok => "OK",
            HttpStatus::NotFound => "Not Found",
            HttpStatus::BadRequest => "Bad Request",
            HttpStatus::NotImplemented => "Not Implemented",
            HttpStatus::InternalServerError => "Internal Server Error",
        }
    }
}

impl HttpResponse {
    fn to_string(&self) -> String {
        let mut response = format!(
            "HTTP/1.1 {} {}\r\n",
            self.status.code(),
            self.status.message()
        );
        for (key, value) in &self.headers {
            response.push_str(&format!("{}: {}\r\n", key, value));
        }
        response.push_str("\r\n");
        response.push_str(&self.body);
        response
    }
}

fn create_response(status: HttpStatus, body: String) -> HttpResponse {
    let content_length = body.len().to_string();
    HttpResponse {
        status,
        headers: vec![
            ("Content-Type".to_string(), "text/html".to_string()),
            ("Content-Length".to_string(), content_length),
        ],
        body,
    }
}

fn parse_request(buffer: &[u8]) -> Result<HttpRequest, Box<dyn Error>> {
    let request_str = str::from_utf8(buffer)?;
    let mut lines = request_str.lines();

    let request_line = lines.next().ok_or("Invalid request line")?;
    let mut parts = request_line.split_whitespace();
    let method = parts.next().ok_or("Missing HTTP method")?.to_string();
    let path = parts.next().ok_or("Missing HTTP path")?.to_string();

    let mut headers = Vec::new();
    let mut content_length = 0;

    for line in lines {
        if line.is_empty() {
            break;
        }
        let mut header_parts = line.splitn(2, ": ");
        if let (Some(key), Some(value)) = (header_parts.next(), header_parts.next()) {
            headers.push((key.to_string(), value.to_string()));
            if key.eq_ignore_ascii_case("Content-Length") {
                content_length = value.trim().parse::<usize>()?;
            }
        }
    }

    let mut body = String::new();
    if content_length > 0 {
        body = String::from_utf8(buffer[buffer.len() - content_length..].to_vec())?;
    }
    Ok(HttpRequest {
        method,
        path,
        headers,
        body,
    })
}

fn is_path_safe(path: &str) -> bool {
    let safe_base = std::env::current_dir().unwrap().join("server");
    let requested_path = safe_base.join(path.trim_start_matches('/'));

    requested_path.starts_with(&safe_base)
}

fn handle_file_request(path: &str) -> HttpResponse {
    if !is_path_safe(path) {
        return create_response(HttpStatus::BadRequest, "Invalid file path".to_string());
    }

    match File::open(format!("server{}", path)) {
        Ok(file) => {
            let mut reader = BufReader::new(file);
            let mut file_content = String::new();
            if reader.read_to_string(&mut file_content).is_err() {
                return create_response(
                    HttpStatus::InternalServerError,
                    "Internal Server Error".to_string(),
                );
            }
            let content_type = if path.ends_with(".css") {
                "text/css"
            } else if path.ends_with(".html") {
                "text/html"
            } else {
                "application/octet-stream"
            };
            let mut response = create_response(HttpStatus::Ok, file_content);
            response
                .headers
                .push(("Content-Type".to_string(), content_type.to_string()));
            response
        }
        Err(_) => {
            eprintln!("File not found: server{}", path);
            create_response(HttpStatus::NotFound, "File not found".to_string())
        }
    }
}

fn handle_upload_request(request: &HttpRequest) -> HttpResponse {
    let filename = request
        .headers
        .iter()
        .find(|(key, _)| key.eq_ignore_ascii_case("filename"))
        .map(|(_, value)| value.clone());

    if let Some(filename) = filename {
        let safe_path = format!("server/{}", filename);

        match std::fs::write(&safe_path, request.body.clone()) {
            Ok(_) => create_response(HttpStatus::Ok, "File uploaded successfully.".to_string()),
            Err(_) => create_response(
                HttpStatus::InternalServerError,
                "Could not save the file.".to_string(),
            ),
        }
    } else {
        create_response(HttpStatus::BadRequest, "Missing filename.".to_string())
    }
}

fn handle_default_request(request: &HttpRequest) -> HttpResponse {
    let user_agent = request
        .headers
        .iter()
        .find(|(key, _)| key.eq_ignore_ascii_case("User-Agent"))
        .map(|(_, value)| value.clone())
        .unwrap_or_else(|| "Unknown".to_string());

    let response_body = format!(
        "<html><body><h1>Hello, World!</h1><p>You requested the path: {}</p><h2>Your User-Agent: {}</h2><h2>Request Body:</h2><pre>{}</pre></body></html>",
        request.path, user_agent, request.body
    );
    create_response(HttpStatus::Ok, response_body)
}

type Handler = fn(&HttpRequest) -> HttpResponse;

struct Router {
    routes: HashMap<String, Handler>,
}

impl Router {
    fn new() -> Self {
        let mut routes = HashMap::new();
        routes.insert("/".to_string(), handle_default_request as Handler);
        routes.insert("/upload".to_string(), handle_upload_request as Handler);
        Router { routes }
    }

    fn route(&self, path: &str, request: &HttpRequest) -> HttpResponse {
        if path.starts_with("/static/") {
            let file_path = path.strip_prefix("/static/").unwrap_or(path);
            handle_file_request(file_path)
        } else if let Some(handler) = self.routes.get(path) {
            handler(request)
        } else {
            create_response(HttpStatus::NotFound, "Not Found".to_string())
        }
    }
}

fn handle_request(request: HttpRequest, router: &Router) -> HttpResponse {
    println!("Handling request: {} {}", request.method, request.path);
    for (key, value) in &request.headers {
        println!("Header: {}: {}", key, value);
    }

    match request.method.as_str() {
        "GET" => router.route(&request.path, &request),
        "POST" => {
            if request.path.starts_with("/upload") {
                handle_upload_request(&request)
            } else {
                create_response(HttpStatus::NotImplemented, "Not Implemented".to_string())
            }
        }
        _ => create_response(HttpStatus::NotImplemented, "Not Implemented".to_string()),
    }
}

fn handle_client(mut stream: TcpStream) {
    let mut buffer = [0; 512];

    match stream.read(&mut buffer) {
        Ok(_) => match parse_request(&buffer) {
            Ok(request) => {
                let router = Router::new();
                let response = handle_request(request, &router);
                stream.write_all(response.to_string().as_bytes()).unwrap();
            }
            Err(err) => {
                eprintln!("Error parsing request: {}", err);
                let response =
                    create_response(HttpStatus::BadRequest, format!("Bad Request: {}", err));
                stream.write_all(response.to_string().as_bytes()).unwrap();
            }
        },
        Err(e) => {
            eprintln!("Failed to read from stream: {}", e);
        }
    }
}

fn main() {
    let listener = TcpListener::bind("127.0.0.1:8080").unwrap();
    println!("server listening on port 8080");

    for stream in listener.incoming() {
        match stream {
            Ok(stream) => {
                handle_client(stream);
            }
            Err(e) => {
                eprintln!("Failed to accept connection: {}", e);
            }
        }
    }
}

