#!/usr/bin/env python3
import os

print("Content-Type: text/html\r\n\r\n")

# Use the root passed from the C++ server
root = os.getenv("SERVER_ROOT", ".")
layout_path = os.path.join(root, "pages/success_layout.html")

with open(layout_path) as f:
	html = f.read()

html = html.replace("{{title}}", "CGI Test")
html = html.replace("{{body}}", "<p>Dynamic content here!</p>")

print(html)