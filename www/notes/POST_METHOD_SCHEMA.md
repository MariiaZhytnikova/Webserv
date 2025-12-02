# POST Method Processing Schema

## 📋 Overview
This document describes the complete flow of POST request handling in the Webserv project.

---

## 🔄 Complete Flow Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                         POST REQUEST RECEIVED                        │
│  Example: POST /uploads/myfile.txt HTTP/1.1                         │
│           Content-Type: multipart/form-data; boundary=----xyz        │
│           Content-Length: 1234                                       │
│           [body data]                                                │
└─────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    VALIDATION (RequestValidator)                     │
│  ✓ Check if POST method is allowed in location                      │
│  ✓ Validate Content-Length header exists and is a number            │
│  ✓ Check if Content-Type exists when body is non-empty              │
│  ✓ Verify body size <= client_max_body_size                         │
└─────────────────────────────────────────────────────────────────────┘
                                    │
                        ┌───────────┴───────────┐
                        │   Validation Failed?   │
                        └───────────┬───────────┘
                                    │
                        ┌───────────┴───────────┐
                        │                       │
                       YES                     NO
                        │                       │
                        ▼                       ▼
                 ┌─────────────┐      ┌─────────────────┐
                 │ Return 400  │      │ Continue to     │
                 │ Bad Request │      │ servePostStatic │
                 └─────────────┘      └─────────────────┘
                                                │
                                                ▼
┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃                    STEP 1: RESOLVE UPLOAD PATH                     ┃
┃  • Get request path: req.getPath()                                 ┃
┃  • Get base root: resolveRoot(srv, loc)                            ┃
┃  • Build fullPath: baseRoot + "/" + cleanPath                      ┃
┃                                                                     ┃
┃  Example:                                                           ┃
┃    Request:  POST /uploads/myfile.txt                              ┃
┃    Base:     ./www                                                  ┃
┃    Result:   ./www/uploads/myfile.txt                              ┃
┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
                                    │
                                    ▼
┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃              STEP 2: PARSE BODY BY CONTENT-TYPE                    ┃
┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
                                    │
                ┌───────────────────┼───────────────────┐
                │                   │                   │
                ▼                   ▼                   ▼
    ┌─────────────────────┐ ┌─────────────────┐ ┌──────────────┐
    │ multipart/form-data │ │ application/x-  │ │ Other/None   │
    │                     │ │ www-form-       │ │              │
    └─────────────────────┘ │ urlencoded      │ └──────────────┘
                │           └─────────────────┘         │
                │                   │                   │
                ▼                   ▼                   ▼
┌───────────────────────────────────────────────────────────────────┐
│ FORMAT 1: MULTIPART/FORM-DATA                                     │
├───────────────────────────────────────────────────────────────────┤
│ 1. Extract boundary from Content-Type header                      │
│    Example Input (full header):                                   │
│    Content-Type: multipart/form-data; boundary=----WebKitFormBoundary7MA4YWxkTrZu0gW │
│                                                                    │
│    Extraction process:                                             │
│    • Find "boundary=" in the header                               │
│    • Extract everything after "boundary="                         │
│    • Result: "----WebKitFormBoundary7MA4YWxkTrZu0gW"             │
│                                                                    │
│ 2. Remove quotes if present                                       │
│    boundary="xyz" → boundary=xyz                                  │
│    boundary=xyz   → boundary=xyz (no change)                      │
│                                                                    │
│ 3. Add "--" prefix → --boundary                                   │
│    Result: "--" + "----WebKitFormBoundary7MA4YWxkTrZu0gW"        │
│           = "------WebKitFormBoundary7MA4YWxkTrZu0gW"            │
│                                                                    │
│ 4. Find first boundary in body                                    │
│                                                                    │
│ 5. Parse headers between boundary and blank line (\r\n\r\n)       │
│    Look for:                                                       │
│    Content-Disposition: form-data; name="file"; filename="x.png"  │
│                                                                    │
│ 6. Extract filename (quoted or unquoted)                          │
│                                                                    │
│ 7. Extract file data between \r\n\r\n and next boundary           │
│                                                                    │
│ 8. Remove trailing \r\n before end boundary                       │
│                                                                    │
│ Result: fileName + fileData                                        │
└───────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌───────────────────────────────────────────────────────────────────┐
│ FORMAT 2: APPLICATION/X-WWW-FORM-URLENCODED                       │
├───────────────────────────────────────────────────────────────────┤
│ 1. Check if body contains '=' character                           │
│    If NO: treat entire body as raw data                           │
│           fileName = "raw_[timestamp].txt"                        │
│           fileData = body                                          │
│                                                                    │
│ 2. Parse key=value pairs separated by '&'                         │
│    Example: filename=test.txt&content=Hello+World                 │
│                                                                    │
│ 3. URL-decode each key and value                                  │
│    + → space, %20 → space, etc.                                   │
│                                                                    │
│ 4. Extract parameters:                                             │
│    • fileName = params["filename"] OR "upload_[timestamp].txt"    │
│    • fileData = params["content"] OR entire body                  │
│                                                                    │
│ Result: fileName + fileData                                        │
└───────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌───────────────────────────────────────────────────────────────────┐
│ FORMAT 3: RAW BINARY / OTHER                                      │
├───────────────────────────────────────────────────────────────────┤
│ Treat entire body as raw binary data                              │
│                                                                    │
│ fileName = "raw_[timestamp].bin"                                  │
│ fileData = body                                                    │
│                                                                    │
│ Result: fileName + fileData                                        │
└───────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃              STEP 3: SANITIZE & VALIDATE FILENAME                  ┃
┃  • Remove dangerous characters: / \ : * ? " < > |                  ┃
┃  • Remove control characters (ASCII < 32)                          ┃
┃  • If empty after sanitization → "upload_[timestamp].bin"          ┃
┃                                                                     ┃
┃  Example: "../../etc/passwd" → "etcpasswd"                         ┃
┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
                                    │
                                    ▼
┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃                STEP 4: DETERMINE FINAL FILE PATH                   ┃
┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
                                    │
                ┌───────────────────┼───────────────────┐
                │                   │                   │
                ▼                   ▼                   ▼
    ┌─────────────────┐  ┌────────────────┐  ┌─────────────────┐
    │ Path exists &   │  │ Path ends      │  │ No extension in │
    │ is directory?   │  │ with '/'?      │  │ last component? │
    └─────────────────┘  └────────────────┘  └─────────────────┘
            │YES              │YES                   │YES
            ▼                 ▼                      ▼
    ┌─────────────────────────────────────────────────────────┐
    │ fullPath = fullPath + "/" + fileName                    │
    │ Example: ./www/uploads/ + test.txt → ./www/uploads/test.txt │
    └─────────────────────────────────────────────────────────┘
                                    │
                                   NO (has extension)
                                    │
                                    ▼
                    ┌───────────────────────────────┐
                    │ Use fullPath as-is            │
                    │ (file already specified)      │
                    └───────────────────────────────┘
                                    │
                                    ▼
┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃           STEP 5: CREATE DIRECTORY STRUCTURE                       ┃
┃  1. Extract directory path (everything before last '/')            ┃
┃     Example: ./www/uploads/2024/images/file.png → ./www/uploads/2024/images ┃
┃                                                                     ┃
┃  2. Check if directory exists using stat()                         ┃
┃                                                                     ┃
┃  3. If NOT exists, recursively create each level:                  ┃
┃     ./www → ./www/uploads → ./www/uploads/2024 → etc.              ┃
┃                                                                     ┃
┃  4. Use mkdir() with permissions 0755                              ┃
┃                                                                     ┃
┃  5. If mkdir() fails (except EEXIST) → Return 500 error            ┃
┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
                                    │
                                    ▼
┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃                  STEP 6: WRITE FILE TO DISK                        ┃
┃  1. Open file: std::ofstream(fullPath, std::ios::binary)           ┃
┃                                                                     ┃
┃  2. If open fails → Return 500 error                               ┃
┃                                                                     ┃
┃  3. Write data: out.write(fileData.c_str(), fileData.size())       ┃
┃     (Binary-safe write, preserves exact bytes)                     ┃
┃                                                                     ┃
┃  4. Close file                                                      ┃
┃                                                                     ┃
┃  5. Log success: "POST: saved [fullPath]"                          ┃
┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
                                    │
                                    ▼
┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃               STEP 7: RETURN SUCCESS RESPONSE                      ┃
┃  HTTP/1.1 200 OK                                                    ┃
┃  Content-Type: text/html                                            ┃
┃  Content-Length: [size]                                             ┃
┃                                                                     ┃
┃  [Success page with template variables:]                           ┃
┃  • filename = fullPath                                              ┃
┃  • size = fileData.size()                                           ┃
┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
                                    │
                                    ▼
                            ┌───────────────┐
                            │  DONE ✓       │
                            └───────────────┘
```

---

## 📊 Content-Type Handling Summary

| Content-Type | Trigger | Parsing Logic | Example Command |
|-------------|---------|---------------|-----------------|
| **multipart/form-data** | Contains "multipart/form-data" | Parse boundaries, extract filename from headers, extract binary data | `curl -F "file=@image.png" http://localhost:8080/uploads/` |
| **application/x-www-form-urlencoded** | Contains "application/x-www-form-urlencoded" | Parse key=value pairs, URL-decode, look for filename & content | `curl -d "filename=test.txt&content=hello" http://localhost:8080/uploads/` |
| **Other / None** | Anything else | Treat entire body as raw binary, generate timestamp filename | `curl --data-binary @file.dat http://localhost:8080/uploads/` |

---

## 🧪 Test Examples

### 1. Multipart Form Data (Browser Upload)
```bash
curl -F "file=@myimage.png" http://localhost:8080/uploads/
```

**Complete HTTP Request Example:**
```http
POST /uploads/ HTTP/1.1
Host: localhost:8080
Content-Type: multipart/form-data; boundary=----WebKitFormBoundary7MA4YWxkTrZu0gW
Content-Length: 245

------WebKitFormBoundary7MA4YWxkTrZu0gW
Content-Disposition: form-data; name="file"; filename="myimage.png"
Content-Type: image/png

[PNG BINARY DATA HERE - 89 50 4E 47 0D 0A 1A 0A ...]
------WebKitFormBoundary7MA4YWxkTrZu0gW--
```

**Parsing Steps:**
1. **Extract boundary from header:**
   - Header: `Content-Type: multipart/form-data; boundary=----WebKitFormBoundary7MA4YWxkTrZu0gW`
   - Find `boundary=` → Extract: `----WebKitFormBoundary7MA4YWxkTrZu0gW`
   - Add `--` prefix → `------WebKitFormBoundary7MA4YWxkTrZu0gW`

2. **Find boundary in body:**
   - Search for: `------WebKitFormBoundary7MA4YWxkTrZu0gW`
   - Found at start of body

3. **Parse headers:**
   - `Content-Disposition: form-data; name="file"; filename="myimage.png"`
   - Extract filename: `myimage.png`

4. **Extract file data:**
   - Data between `\r\n\r\n` and next boundary
   - Binary PNG data

5. **Save file:**
   - Path: `./www/uploads/myimage.png`
   - Content: Binary PNG data

**Flow:**
- Content-Type: `multipart/form-data; boundary=----WebKitFormBoundary7MA4YWxkTrZu0gW`
- Parse boundary → Extract filename "myimage.png" → Extract binary data → Save as `./www/uploads/myimage.png`

---

### 2. URL-Encoded Form (HTML Form)
```bash
curl -d "filename=test.txt&content=Hello World" http://localhost:8080/uploads/
```
**Flow:**
- Content-Type: `application/x-www-form-urlencoded`
- Parse key=value → filename="test.txt", content="Hello World" → Save as `./www/uploads/test.txt`

---

### 3. Raw Binary (No Content-Type)
```bash
printf "POST /uploads/ HTTP/1.1\r\nHost: localhost:8080\r\nContent-Type: application/octet-stream\r\nContent-Length: 11\r\n\r\nhello world" | nc localhost 8080
```
**Flow:**
- Content-Type: `application/octet-stream` (not multipart or urlencoded)
- Treat as raw → fileName = "raw_1733140000.bin" → Save entire body

---

### 4. Missing Content-Type (Validation Error)
```bash
printf "POST /uploads/ HTTP/1.1\r\nHost: localhost:8080\r\nContent-Length: 11\r\n\r\nhello world" | nc localhost 8080
```
**Flow:**
- ❌ No Content-Type header
- ❌ Body is non-empty (11 bytes)
- ❌ Validator rejects with 400: "Content-Type required for non-empty POST body"

---

## 🔐 Security Features

### 1. Filename Sanitization
- Removes path traversal: `../../etc/passwd` → `etcpasswd`
- Removes dangerous characters: `/ \ : * ? " < > |`
- Removes control characters (ASCII < 32)
- Prevents directory escape attacks

### 2. Path Resolution
- Uses `resolveRoot()` for consistent base path
- Trims leading slashes to prevent absolute path injection
- Always operates within configured root directory

### 3. Directory Creation
- Creates intermediate directories safely
- Checks for errors (permission denied, disk full, etc.)
- Returns 500 on mkdir failure

### 4. File Writing
- Binary-safe write (preserves exact bytes)
- Checks if file can be opened
- Returns 500 on write failure

---

## 🎯 Key Variables

| Variable | Purpose | Example Value |
|----------|---------|---------------|
| `reqPath` | Request URI path | `/uploads/myfile.txt` |
| `baseRoot` | Server/location root | `./www` |
| `fullPath` | Final filesystem path | `./www/uploads/myfile.txt` |
| `contentType` | Content-Type header | `multipart/form-data; boundary=xyz` |
| `fileName` | Extracted filename | `myimage.png` |
| `fileData` | Actual file content (binary) | `[PNG binary data]` |
| `boundary` | Multipart boundary | `----WebKitFormBoundary7MA4YWxkTrZu0gW` |

---

## ⚠️ Error Responses

| Status | Reason | Trigger |
|--------|--------|---------|
| **400** | Bad Request | Missing Content-Type, malformed multipart, missing boundary |
| **413** | Payload Too Large | Body size > client_max_body_size |
| **500** | Internal Server Error | mkdir() fails, file write fails, cannot open file |

---

## 📝 Notes

1. **Binary Safety**: All file writes use `std::ios::binary` to preserve exact bytes
2. **Recursive Directory Creation**: Handles nested paths like `./www/uploads/2024/images/`
3. **Timestamp Fallback**: Generated filenames use Unix timestamp to avoid collisions
4. **Empty Filename Handling**: Validates filename is non-empty before accepting
5. **Boundary Quote Removal**: Handles both `boundary="value"` and `boundary=value`
6. **CRLF Handling**: Properly strips `\r\n` from multipart data

---

## 🔄 Comparison with GET Method

| Aspect | GET | POST |
|--------|-----|------|
| **Purpose** | Retrieve files | Upload/create files |
| **Body** | No body | Has body with data |
| **Path Resolution** | Uses `resolveRoot()` | Uses `resolveRoot()` |
| **Security** | Path traversal check | Filename sanitization |
| **Error Handling** | 404 if not found | 500 if write fails |
| **Content-Type** | Response based on file extension | Request determines parsing |

---

**Generated:** 2025-12-02  
**Project:** Webserv (42 School)  
**Authors:** Mariia Z., Evgeniia K., Marina Z.
