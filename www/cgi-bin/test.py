#!/usr/bin/env python3
import os
import random
from datetime import datetime

print("Content-Type: text/html\r\n\r\n")

root = os.getenv("SERVER_ROOT", ".")
layout_path = os.path.join(root, "pages/success_layout.html")

with open(layout_path) as f:
    html = f.read()

messages = [
	"Everything worked perfectly! 🎉",
	"CGI is alive and running! 🚀",
	"You executed a Python script through your server. Nice! 🧠",
	"Backend magic happening right here ✨",
	"CGI? More like Cool Great Interface! 😎"
]
html = html.replace("{{title}}", "CGI Success")
html = html.replace("{{icon}}", "🐍")
html = html.replace("{{heading}}", "CGI Script Executed Successfully")
html = html.replace("{{message}}", random.choice(messages))
html = html.replace("{{time}}", datetime.now().strftime("%Y-%m-%d %H:%M:%S"))

print(html)