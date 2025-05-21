#!/usr/bin/env nu

export def main [command: string] {
    match $command {
        "1" => {
            http get http://127.0.0.1:8080/
        }
        "2" => {
            http get http://127.0.0.1:8080/static/example.html
        }
        "3" => {
            http get http://127.0.0.1:8080/static/nonexistentfile.html
        }
        "4" => {
            http post http://127.0.0.1:8080/
        }
        "5" => {
            http get http://127.0.0.1:8080/ --headers ["X-Custom-Header" "Value"]
        }
        "6" => {
            http post http://127.0.0.1:8080/upload --content-type "text/plain" "This is the content of the uploaded file."
        }
        "7" => {
            http post http://127.0.0.1:8080/../invalid/path --content-type "text/plain" "Attempting to traverse"
        }
        _ => {
            print $"unrecognized command: ($command)"
        }
    }
}

