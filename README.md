# 🌐 Webserv

<div align="center">

**A lightweight, high-performance HTTP/1.1 server built from scratch in C++98**

[Features](#-features) • [Installation](#-installation) • [Usage](#-usage) • [Configuration](#-configuration) • [Testing](#-testing) • [Documentation](#-documentation)

</div>

---

## 🎥 Video Demo

> **Add your demo video here!**

<!-- Uncomment and add your video link when ready -->
<!-- [![Webserv Demo](https://img.youtube.com/vi/YOUR_VIDEO_ID/maxresdefault.jpg)](https://www.youtube.com/watch?v=YOUR_VIDEO_ID) -->

<!-- Or use a direct embed: -->
<!-- <video width="100%" controls>
  <source src="path/to/your/demo.mp4" type="video/mp4">
</video> -->

---

## 📖 Table of Contents

- [About](#-about)
- [Features](#-features)
- [Architecture](#-architecture)
- [Installation](#-installation)
- [Usage](#-usage)
- [Configuration](#-configuration)
- [HTTP Methods](#-http-methods)
- [Testing](#-testing)
- [Error Handling](#-error-handling)
- [Documentation](#-documentation)
- [Resources](#-resources)
- [Team](#-team)

---

## 🎯 About

**Webserv** is a 42 school project that challenges students to implement a fully functional HTTP/1.1 server from scratch using **C++98**. The server handles multiple simultaneous connections using **non-blocking I/O**, serves static files, processes CGI scripts, manages file uploads, and provides custom error pages.

This implementation follows the HTTP/1.1 specification (RFC 7230-7235) and includes both mandatory and bonus features.

### Why This Project?

- **Deep understanding of HTTP protocol** and web server architecture
- **Network programming** with sockets, poll(), and non-blocking I/O
- **Concurrent connection handling** without threads or fork()
- **Real-world software design** with modular, maintainable code
- **Security considerations** (path traversal, buffer overflows, etc.)

---

## ✨ Features

### Core Features ✅
- ✅ **HTTP/1.1 Compliance** - Full implementation of HTTP/1.1 protocol
- ✅ **Non-blocking I/O** - Uses `poll()` for event-driven architecture
- ✅ **Virtual Hosts** - Multiple server configurations on different ports
- ✅ **Static File Serving** - Efficient file delivery with proper MIME types
- ✅ **Directory Listing** - Auto-generated index pages (autoindex)
- ✅ **CGI Support** - Execute Python, PHP, and custom CGI scripts
- ✅ **File Upload** - Handle POST requests with multipart/form-data
- ✅ **Custom Error Pages** - Branded error responses (400, 403, 404, 500, etc.)
- ✅ **HTTP Methods** - GET, POST, DELETE support
- ✅ **Redirections** - 301 permanent redirects
- ✅ **Session Management** - Persistent connections (Keep-Alive)
- ✅ **Request Validation** - Strict parsing and validation
- ✅ **Body Size Limits** - Configurable `client_max_body_size`

### Bonus Features 🌟
- 🌟 **Multiple CGI Extensions** - Support for .py, .php, .cgi
- 🌟 **Cookie Handling** - Session cookies and theme persistence
- 🌟 **Advanced Routing** - Location-based configuration inheritance
- 🌟 **Request Logging** - Detailed access and error logs
- 🌟 **Security Features** - Path traversal prevention, filename sanitization

---

## 🏗️ Architecture

### System Design

```
┌─────────────────────────────────────────────────────────────┐
│                      ServerManager                          │
│  • Manages all servers and sockets                          │
│  • Event loop with poll()                                   │
│  • Routes requests to appropriate handlers                  │
└─────────────────────────────────────────────────────────────┘
                           │
           ┌───────────────┼───────────────┐
           ▼               ▼               ▼
    ┌──────────┐    ┌──────────┐    ┌──────────┐
    │ Server 1 │    │ Server 2 │    │ Server N │
    │ :8080    │    │ :8081    │    │ :...     │
    └──────────┘    └──────────┘    └──────────┘
           │               │               │
           └───────────────┴───────────────┘
                           │
                           ▼
              ┌────────────────────────┐
              │   Request Validator    │
              │  • Parse HTTP request  │
              │  • Validate headers    │
              │  • Check permissions   │
              └────────────────────────┘
                           │
           ┌───────────────┼───────────────┐
           ▼               ▼               ▼
    ┌──────────┐    ┌──────────┐    ┌──────────┐
    │   GET    │    │   POST   │    │  DELETE  │
    │ Handler  │    │ Handler  │    │ Handler  │
    └──────────┘    └──────────┘    └──────────┘
           │               │               │
           └───────────────┴───────────────┘
                           │
                           ▼
              ┌────────────────────────┐
              │   Response Builder     │
              │  • Generate headers    │
              │  • Apply templates     │
              │  • Send to client      │
              └────────────────────────┘
```

### Key Components

| Component | File | Responsibility |
|-----------|------|---------------|
| **ServerManager** | `ServerManager.cpp` | Main event loop, socket management, request routing |
| **ConfigParser** | `ConfigParser.cpp` | Parse nginx-like configuration files |
| **HttpRequest** | `HttpRequest.cpp` | Parse and represent HTTP requests |
| **HttpResponse** | `HttpResponse.cpp` | Build and format HTTP responses |
| **RequestHandler** | `RequestHandler.cpp` | Route requests to appropriate handlers |
| **StaticGet** | `StaticGet.cpp` | Serve static files and directories |
| **StaticPost** | `StaticPost.cpp` | Handle file uploads and form submissions |
| **StaticDelete** | `StaticDelete.cpp` | Delete resources |
| **CgiHandler** | `CgiHandler.cpp` | Execute and manage CGI processes |
| **SessionManager** | `SessionManager.cpp` | Manage client sessions and cookies |
| **Logger** | `Logger.cpp` | Log access and errors |

---

## 🚀 Installation

### Prerequisites

- **C++ Compiler**: g++ or clang++ (C++98 standard)
- **Make**: GNU Make
- **Operating System**: Linux or macOS
- **Optional**: Python3, PHP-CGI (for CGI support)

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/MariiaZhytnikova/Webserv.git
cd Webserv

# Compile the project
make

# The executable will be created as 'webServ'
```

### Compilation Flags

```makefile
-Wall -Wextra -Werror -std=c++98
```

---

## 🎮 Usage

### Basic Usage

```bash
# Start server with default configuration
./webServ

# Start server with custom configuration
./webServ conf/default.conf

# Start server with custom configuration
./webServ conf/max.conf
```

### Access the Server

Open your browser and navigate to:
```
http://localhost:8080/
```

### Quick Test Commands

```bash
# Test GET request
curl -i http://localhost:8080/

# Test POST file upload
curl -F "file=@myfile.txt" http://localhost:8080/uploads/

# Test POST form data
curl -d "filename=test.txt&content=Hello World" http://localhost:8080/uploads/

# Test DELETE request
curl -i -X DELETE http://localhost:8080/uploads/myfile.txt

# Test CGI script
curl http://localhost:8080/cgi-bin/test.py

# Load testing with siege
siege -b http://localhost:8080/
siege -v -r100 -c10 'http://localhost:8080/uploads/echo POST hello=world'
```

---

## ⚙️ Configuration

Webserv uses an **nginx-like configuration syntax**. Configuration files are located in the `conf/` directory.

### Example Configuration

```nginx
server {
    listen 8080;
    server_name localhost;
    root ./www;
    index index.html;
    
    error_page 404 /errors/404.html;
    client_max_body_size 2M;
    
    location / {
        autoindex on;
        allow_methods GET POST DELETE;
    }
    
    location /uploads/ {
        root ./www;
        autoindex on;
        allow_methods POST GET DELETE;
    }
    
    location /cgi-bin/ {
        root ./www/cgi-bin;
        allow_methods GET POST;
        cgi_extension .py /usr/bin/python3;
        cgi_extension .php /usr/bin/php-cgi;
    }
    
    location /redirect-me {
        return 301 /pages/redirected.html;
    }
}
```

### Configuration Directives

| Directive | Context | Description | Example |
|-----------|---------|-------------|---------|
| `listen` | server | Port to listen on | `listen 8080;` |
| `server_name` | server | Server hostname | `server_name localhost;` |
| `root` | server, location | Document root directory | `root ./www;` |
| `index` | server, location | Default index file | `index index.html;` |
| `autoindex` | location | Enable directory listing | `autoindex on;` |
| `allow_methods` | location | Allowed HTTP methods | `allow_methods GET POST;` |
| `client_max_body_size` | server, location | Max request body size | `client_max_body_size 2M;` |
| `error_page` | server | Custom error pages | `error_page 404 /404.html;` |
| `return` | location | HTTP redirect | `return 301 /new-url;` |
| `cgi_extension` | location | CGI handler mapping | `cgi_extension .py /usr/bin/python3;` |

---

## 🔧 HTTP Methods

### GET - Retrieve Resources

**Purpose**: Fetch static files, directory listings, or CGI output

**Examples**:
```bash
# Get index page
curl http://localhost:8080/

# Get specific file
curl http://localhost:8080/pictures/mura_1.jpeg

# Get directory listing (if autoindex is on)
curl http://localhost:8080/pictures/

# Execute CGI script
curl http://localhost:8080/cgi-bin/test.py
```
---

### POST - Upload Files & Submit Forms

**Purpose**: Upload files, submit form data, create resources

**Supported Content-Types**:
1. `multipart/form-data` - File uploads
2. `application/x-www-form-urlencoded` - Form submissions
3. Raw binary data

**Examples**:
```bash
# Upload file (multipart/form-data)
curl -F "file=@photo.jpg" http://localhost:8080/uploads/

# Submit form data
curl -d "filename=data.txt&content=Hello World" http://localhost:8080/uploads/

# Raw binary upload
curl --data-binary @archive.zip http://localhost:8080/uploads/
```

**Processing Flow**:
1. ✅ Resolve upload directory path
2. ✅ Parse body based on Content-Type
3. ✅ Sanitize filename (prevent path traversal)
4. ✅ Create directory structure if needed
5. ✅ Write file to disk
6. ✅ Return success response

---

### DELETE - Remove Resources

**Purpose**: Delete files from the server

**Examples**:
```bash
# Delete a file
touch www/uploads/tmp.txt
curl -X DELETE http://localhost:8080/uploads/oldfile.txt

```
---

## 🧪 Testing

### Manual Testing

```bash
# Test with curl
curl -i http://localhost:8080/

# Test with netcat (raw HTTP)
printf "GET / HTTP/1.1\r\nHost: localhost:8080\r\n\r\n" | nc localhost 8080

# Test POST with proper headers
printf "POST /uploads/ HTTP/1.1\r\nHost: localhost:8080\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 35\r\n\r\nfilename=test.txt&content=hello123" | nc localhost 8080
```

### Browser Testing

1. Open `http://localhost:8080/` in your browser
2. Navigate through the demo pages
3. Test file upload form at `/pages/upload.html`
4. Test file deletion at `/pages/delete.html`
5. Try CGI scripts at `/cgi-bin/test.py`

---

## ⚠️ Error Handling

### Supported HTTP Status Codes

| Code | Status | Usage |
|------|--------|-------|
| **200** | OK | Successful request |
| **201** | Created | Successful file upload |
| **204** | No Content | Successful DELETE |
| **301** | Moved Permanently | Redirect |
| **400** | Bad Request | Invalid syntax, missing headers |
| **403** | Forbidden | Permission denied |
| **404** | Not Found | Resource doesn't exist |
| **405** | Method Not Allowed | Method not supported for location |
| **413** | Payload Too Large | Body exceeds `client_max_body_size` |
| **500** | Internal Server Error | Server-side error |
| **501** | Not Implemented | Method not implemented |
| **502** | Bad Gateway | CGI script error |
| **505** | HTTP Version Not Supported | Unsupported HTTP version |

### Custom Error Pages

Configure custom error pages in `conf/default.conf`:
```nginx
error_page 404 /errors/404.html;
error_page 500 /errors/500.html;
```

---

## 📚 Documentation

### Additional Documentation

- **[HTTP/1.1 Specification (RFC 7230)](https://tools.ietf.org/html/rfc7230)** - Official HTTP protocol
- **[CGI Specification (RFC 3875)](https://tools.ietf.org/html/rfc3875)** - Common Gateway Interface

### Project Structure

```
Webserv/
├── conf/                   # Configuration files
│   ├── default.conf        # Default server config
│   └── max.conf            # Extended config example
├── inc/                    # Header files
├── srcs/                   # Source files
├── www/                    # Web root directory
│   ├── index.html          # Main page
│   ├── cgi-bin/            # CGI scripts
│   ├── css/                # Stylesheets
│   ├── errors/             # Error pages
│   ├── images/             # Images
│   ├── pages/              # Additional pages
│   └── uploads/            # Upload directory
├── log/                    # Server logs
│   ├── access.log          # Access log
│   └── error.log           # Error log
├── Makefile                # Build configuration
├── README.md               # This file

```
---

## 📖 Resources

### References Used

- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/html/) - Socket programming fundamentals
- [Building an HTTP Server from Scratch](https://osasazamegbe.medium.com/showing-building-an-http-server-from-scratch-in-c-2da7c0db6cb7) - Implementation guide
- [C++ Web Programming](https://www.tutorialspoint.com/cplusplus/cpp_web_programming.htm) - C++ networking basics
- [RFC 7230-7235](https://tools.ietf.org/html/rfc7230) - HTTP/1.1 Specification
- [RFC 3875](https://tools.ietf.org/html/rfc3875) - CGI Specification

### Learning Outcomes

✅ **Network Programming**: Mastered sockets, poll(), non-blocking I/O  
✅ **HTTP Protocol**: Deep understanding of request/response cycle  
✅ **Concurrent Handling**: Event-driven architecture without threads  
✅ **Security**: Path traversal prevention, input validation  
✅ **Configuration Parsing**: Complex config file parsing  
✅ **CGI Integration**: Process management and pipe communication  



---

## 📄 License

This project is part of the 42 school curriculum. Free to use for educational purposes.

---

## 🙏 Acknowledgments

- **42 School** for the comprehensive project requirements
- **HTTP Working Group** for detailed specifications
- **Open source community** for networking resources and guides

---

### Functionality:
1️⃣ Server & Socket Setup

	Actual listening sockets:
	Create socket() for each Server.
	Set SO_REUSEADDR to allow quick restarts.
	Bind to _host:_port.
	Listen on the socket.

Each listening socket corresponds to a Server. If multiple servers share the same port, you can reuse the same socket (common for host-based virtual hosting).

2️⃣ Event Loop (select/poll/epoll)

Webserv uses non-blocking I/O:

	poll() or select() (epoll is bonus)

Track all sockets:

	Listening sockets → accept new connections
	Client sockets → read request / write response

3️⃣ HTTP Request Parsing

For each client connection:

	Read request into buffer.
	Parse request line: METHOD PATH HTTP_VERSION
	Parse headers: Host, Content-Length, Content-Type, etc.
	Parse body (if POST / chunked).

This is where client_max_body_size from your Server and Location objects matters.

4️⃣ Routing Requests

Based on Host + Port + PATH:

Select the correct server:

	Match _host:_port and server_name
	Default server if no match

Select the correct location block:

	Longest matching _path prefix
	Merge location settings over server defaults
	Check allowed methods (allow_methods)
	Apply return / redirects if set

5️⃣ Serving Responses

Depending on the location and method:

	Static file → read file from _root + path

Directory:

	If _autoindex → generate listing
	Else → serve _index file
	CGI execution if cgi_pass is set
	Return redirects if return is set
	Upload handling (if POST and upload_path)

6️⃣ HTTP Response Building

	Construct headers: Status-Line, Content-Length, Content-Type
	Send response back to client

7️⃣ Error Handling

	Use _errorPages from Server
	Fallback to 404/500 if no custom page
	Close connection if necessary (Connection: close)

✅ Minimum errors
| Code    | Status phrase              | Meaning / When to use                    |
| ------- | -------------------------- | ---------------------------------------- |
| **200** | OK                         | Successful request                       |
| **201** | Created                    | After successful file upload (POST)      |
| **204** | No Content                 | Successful DELETE request                |
| **301** | Moved Permanently          | If you support redirection               |
| **400** | Bad Request                | Invalid syntax or malformed request      |
| **403** | Forbidden                  | Access denied (e.g. permission)          |
| **404** | Not Found                  | File or route doesn’t exist              |
| **405** | Method Not Allowed         | Method not supported for location        |
| **411** | Length Required            | Missing Content-Length in POST           |
| **413** | Payload Too Large          | Body exceeds `client_max_body_size`      |
| **414** | URI Too Long               | Path exceeds max length                  |
| **415** | Unsupported Media Type     | Wrong content type (rarely needed)       |
| **500** | Internal Server Error      | General server error (default catch-all) |
| **501** | Not Implemented            | Method not implemented                   |
| **502** | Bad Gateway                | If CGI crashes or returns invalid output |
| **505** | HTTP Version Not Supported | For unsupported HTTP version             |

<div align="center">

**Made with ♥ by the Webserv Team**

⭐ Star this repo if you found it helpful!

</div>

