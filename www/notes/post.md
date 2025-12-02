## POST

POST request arrives
    ↓
What's the Content-Type?
    ↓
┌─────────────────────────────────────────────────────────┐
│ Content-Type: application/x-www-form-urlencoded         │
└─────────────────────────────────────────────────────────┘
    ↓
Does the body contain "=" character?
    ↓
    ├─── YES (has "=")                    NO (no "=") ───┤
    │                                                      │
    │    Body: "hello=world"                 Body: "hello world"
    │         or "key1=val1&key2=val2"            (no equals sign)
    │                                                      │
    ↓                                                      ↓
Parse as FORM DATA                              Treat as RAW TEXT
    ↓                                                      ↓
Look for these parameters:                      fileName = "raw_TIMESTAMP.txt"
- filename=...                                  fileData = entire body
- content=...                                              ↓
    ↓                                           Save "hello world"
Found "filename" parameter?                     to raw_TIMESTAMP.txt
    ├─── YES ──────┬─── NO ───┐
    │              │           │
fileName =      fileName =    fileName = 
params["filename"]  "upload_TIMESTAMP.txt"
    │              │           │
    └──────────────┴───────────┘
              ↓
Found "content" parameter?
    ├─── YES ──────┬─── NO ───┐
    │              │           │
fileData =      fileData =  fileData = 
params["content"]  entire body   entire body