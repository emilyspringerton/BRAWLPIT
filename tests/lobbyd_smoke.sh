#!/usr/bin/env bash
# End-to-end over real HTTP: create, join, chat, ready, host-only start, leave. Needs curl.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
BIN="${1:-bin/brawlpit_lobbyd}"
PORT=$((20000 + RANDOM % 20000))
"$BIN" --port "$PORT" --game test.example:6978 2>/dev/null & PID=$!
trap 'kill $PID 2>/dev/null' EXIT
for _ in $(seq 50); do curl -sf "localhost:$PORT/health" >/dev/null 2>&1 && break; sleep 0.1; done
H="localhost:$PORT"
tok() { curl -s -o /dev/null -w '%{redirect_url}' "$@" | sed 's/.*t=//;s/&.*//'; }
fail() { echo "FAIL: $*"; exit 1; }
TA=$(tok -d 'name=Ann&title=Smoke+%26+Co' "$H/create")
[ ${#TA} -eq 16 ] || fail "create did not redirect with a token"
CODE=$(curl -s "$H/" | grep -o 'room=[A-Z]*' | head -1 | cut -d= -f2)
curl -s "$H/" | grep -q 'Smoke &amp; Co' || fail "lobby list missing escaped title"
TB=$(tok -d "room=$CODE&name=Bob" "$H/join"); [ ${#TB} -eq 16 ] || fail "join"
curl -s -d "t=$TB&say=hi+%3Cb%3E" -o /dev/null "$H/say"
curl -s "$H/l?t=$TA" | grep -q 'hi &lt;b&gt;' || fail "chat not escaped/visible"
curl -s -o /dev/null -w '%{redirect_url}' -d "t=$TA" "$H/start" | grep -q 'e=8' || fail "start must be refused until Bob is ready"
curl -s -o /dev/null -w '%{redirect_url}' -d "t=$TB" "$H/start" | grep -q 'e=7' || fail "only the host may start"
curl -s -o /dev/null -d "t=$TB" "$H/ready"
curl -s -o /dev/null -d "t=$TA" "$H/start"
curl -s "$H/l?t=$TB" | grep -q 'test.example:6978' || fail "started page should show the game address"
curl -s -o /dev/null -w '%{http_code}' -d "room=$CODE&name=Late" "$H/join" | grep -q 200 || fail "late join page"
curl -s "$H/leave?t=$TB" -o /dev/null
echo "lobbyd_smoke: all passed"
