# DO NOT EXECUTE THIS FILE. the output is messy.
# this is just for reference commands

# Root request
curl http://127.0.0.1:8080/

# Static file request
curl http://127.0.0.1:8080/static/example.html

# A non-existent static file request
curl http://127.0.0.1:8080/static/nonexistentfile.html

# Invalid request method (using POST on root)
curl -X POST http://127.0.0.1:8080/

# Request with custom headers
curl -H "X-Custom-Header: Value" http://127.0.0.1:8080/

# Request body with POST (uploading a file)
curl -X POST http://127.0.0.1:8080/upload -d "This is the content of the uploaded file."

# Upload a file using POST with the actual file content
curl -X POST http://127.0.0.1:8080/upload -d @path/to/your/local/file.txt

# Upload with invalid path (to test security)
curl -X POST http://127.0.0.1:8080/../invalid/path -d "Attempting to traverse"
