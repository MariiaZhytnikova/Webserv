#!/usr/bin/env php
<?php

header("Content-Type: text/html; charset=utf-8");

$root = getenv("SERVER_ROOT") ?: ".";
$layoutPath = $root . "/pages/success_layout.html";

$html = file_get_contents($layoutPath);

$quotes = [
	"PHP is running through CGI! 🐘",
	"CGI + PHP = retro and cool 😎",
	"Your Webserv just executed real PHP code ✨",
	"Did someone say PHP-Fu? 👊",
	"The elephant approves this message 🐘💙"
];

$quote = $quotes[array_rand($quotes)];
$time = date("Y-m-d H:i:s");

$html = str_replace("{{title}}", "PHP Test", $html);
$html = str_replace("{{icon}}", "🐘", $html);
$html = str_replace("{{heading}}", "PHP CGI Executed Successfully", $html);
$html = str_replace("{{message}}", $quote, $html);
$html = str_replace("{{time}}", $time, $html);

echo $html;
?>