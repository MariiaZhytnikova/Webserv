# Webserv

https://beej.us/guide/bgnet/html/#sendall
https://www.tutorialspoint.com/cplusplus/cpp_web_programming.htm

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



