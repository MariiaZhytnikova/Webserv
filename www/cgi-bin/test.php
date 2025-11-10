<?php

// Output HTTP header first
header("Content-Type: text/html");

// Print basic HTML output
echo "<!DOCTYPE html>";
echo "<html>";
echo "<head><title>CGI PHP Test</title></head>";
echo "<body>";
echo "<h1>PHP CGI Test</h1>";
echo "<p>If you can see this, PHP CGI is working correctly!</p>";

// Show environment variables (useful for debugging CGI)
echo "<h2>Environment Variables</h2><pre>";
print_r($_SERVER);
echo "</pre>";

// Handle GET and POST data
echo "<h2>GET Data</h2><pre>";
print_r($_GET);
echo "</pre>";

echo "<h2>POST Data</h2><pre>";
print_r($_POST);
echo "</pre>";

echo "</body></html>";
?>