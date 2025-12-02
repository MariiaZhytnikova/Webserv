#!/bin/bash

# ================================
# CONFIG
# ================================
BASE_URL="http://localhost:8080"

REDIRECT_URL="/redirect-me"
REDIRECT_TARGET="/pages/this_is_redirect.html"
UPLOAD_ENDPOINT="/uploads/echo"
DELETE_TARGET="/uploads/delete_me.txt"
TOO_LARGE="/too_large/abc"
PYTHON_CGI="/cgi-bin/test.py"
PHP_CGI="/cgi-bin/test.php"

# ================================
# COLORS
# ================================
GREEN="\033[32m"
RED="\033[31m"
YELLOW="\033[33m"
RESET="\033[0m"

pass() { echo -e "${GREEN}[PASS]${RESET} $1"; }
fail() { echo -e "${RED}[FAIL]${RESET} $1"; }

# ================================
# Helpers
# ================================
status_code() {
	curl -s -o /dev/null -w "%{http_code}" "$1"
}

body_of() {
	curl -s "$1"
}

print_header() {
	echo
	echo "========================================"
	echo "$1"
	echo "========================================"
}

# ================================
# 1. GET / should return 200
# ================================
test_get_root() {
	code=$(status_code "${BASE_URL}/")
	[ "$code" = "200" ] && pass "GET / returned 200" \
						|| fail "GET / returned $code"
}

# ================================
# 2. GET static ok.html
# ================================
test_get_ok_page() {
	code=$(status_code "${BASE_URL}/pages/ok.html")
	[ "$code" = "200" ] && pass "GET ok.html returned 200" \
						|| fail "GET ok.html returned $code"
}

# ================================
# 3. GET non-existing → 404
# ================================
test_get_404() {
	code=$(status_code "${BASE_URL}/no-such-page")
	[ "$code" = "404" ] && pass "404 page returned correctly" \
						|| fail "Expected 404, got $code"
}

# ================================
# 4. Redirect test
# ================================
test_redirect() {
	code=$(status_code "${BASE_URL}${REDIRECT_URL}")
	if [ "$code" = "301" ]; then
		pass "Redirect returned 301"
	else
		fail "Redirect returned $code (expected 301)"
		return
	fi

	loc=$(curl -I -s "${BASE_URL}${REDIRECT_URL}" | grep -i Location | awk '{print $2}' | tr -d '\r')
	if [[ "$loc" == *"$REDIRECT_TARGET"* ]]; then
		pass "Redirect Location header points to $REDIRECT_TARGET"
	else
		fail "Redirect Location incorrect: '$loc'"
	fi
}

# ================================
# 5. POST upload → /uploads/echo
# ================================
test_post_upload() {
	code=$(curl -s -o /dev/null -w "%{http_code}" \
		-X POST -d "hello=world" "${BASE_URL}${UPLOAD_ENDPOINT}")

	[ "$code" = "200" ] || [ "$code" = "201" ] \
		&& pass "POST upload returned $code" \
		|| fail "POST upload returned $code"
}

# ================================
# 6. POST too large
# ================================
test_post_too_big() {
	big="$(head -c 500 </dev/zero | tr '\0' 'A')"

	code=$(curl -s -o /dev/null -w "%{http_code}" \
		-X POST -d "$big" "${BASE_URL}${TOO_LARGE}")

	[ "$code" = "413" ] && pass "POST too_large returned 413" \
						|| fail "POST too_large returned $code (expected 413)"
}

# ================================
# 7. DELETE file
# ================================
test_delete() {
	echo "to delete" > "./www${DELETE_TARGET}"

	code=$(curl -s -o /dev/null -w "%{http_code}" \
		-X DELETE "${BASE_URL}${DELETE_TARGET}")

	if [[ "$code" = "200" || "$code" = "204" ]]; then
		if [ ! -f "./www${DELETE_TARGET}" ]; then
			pass "DELETE succeeded ($code)"
		else
			fail "DELETE returned $code but file still exists"
		fi
	else
		fail "DELETE returned $code (expected 2xx)"
	fi
}

# ================================
# 8. Autoindex test
# ================================
test_autoindex() {
	body=$(body_of "${BASE_URL}/uploads/")

	if echo "$body" | grep -q "<a href"; then
		pass "Autoindex on /uploads/ works"
	else
		fail "Autoindex /uploads/ missing listing"
	fi
}

# ================================
# 9. Method not allowed (405)
# ================================
test_method_not_allowed() {
	print_header "405 Method Not Allowed"

	for method in GET POST PUT; do
		code=$(curl -s -o /dev/null -w "%{http_code}" \
			-X "$method" "${BASE_URL}/forbidden/")

		if [ "$code" = "405" ]; then
			pass "$method correctly rejected with 405"
		else
			fail "$method returned $code (expected 405)"
		fi
	done
}

# ================================
# 10. Unknown method (501)
# ================================
test_unknown_method() {
	code=$(printf "BREW / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
			| nc localhost 8080 | head -1 | awk '{print $2}')

	if [ "$code" = "501" ]; then
		pass "Unknown method returns 501"
	else
		fail "Unknown method returned $code (expected 501)"
	fi
}

# ================================
# 11. CGI Python
# ================================
test_python_cgi() {
	code=$(status_code "${BASE_URL}${PYTHON_CGI}")
	[ "$code" = "200" ] && pass "Python CGI returned 200" \
						|| fail "Python CGI returned $code"
}

# ================================
# 12. CGI PHP
# ================================
test_php_cgi() {
	code=$(status_code "${BASE_URL}${PHP_CGI}")
	[ "$code" = "200" ] && pass "PHP CGI returned 200" \
						|| fail "PHP CGI returned $code"
}

# ================================
# 13. Keep-alive
# ================================
test_keepalive() {
	print_header "KEEP-ALIVE TEST"

	echo "Sending two sequential requests using one curl session..."

	output=$(curl -v \
		-H "Connection: keep-alive" \
		http://localhost:8080/ \
		--next http://localhost:8080/css/style.css 2>&1)

	# Check if server responded with keep-alive header
	if echo "$output" | grep -qi "Connection: keep-alive"; then
		echo -e "[OK] Server keeps connection open (keep-alive header found)"
	else
		echo -e "[FAIL] No 'Connection: keep-alive' header in response"
	fi

	# Optional: Check if curl reused the connection
	if echo "$output" | grep -qi "Re-using existing connection"; then
		echo -e "[OK] CURL confirms it reused the connection"
	else
		echo -e "[WARN] CURL did NOT reuse the connection (server may be closing it)"
	fi

	echo ""
}

# ================================
# RUN ALL TESTS
# ================================
echo -e "${YELLOW}=== Running Webserv Test Suite ===${RESET}"

test_get_root
test_get_ok_page
test_get_404
test_redirect
test_post_upload
test_post_too_big
test_delete
test_autoindex
test_method_not_allowed
test_unknown_method
test_python_cgi
test_php_cgi
test_keepalive

echo -e "${YELLOW}=== Tests Completed ===${RESET}"
