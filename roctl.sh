#!/bin/bash
# roctl.sh - manage rAthena servers (login/char/map/web) with logging
cd "$(dirname "$0")" || exit 1

SERVERS="login-server char-server map-server web-server"
LOG_DIR="log"

case "$1" in
  start)
    mkdir -p "$LOG_DIR"
    for s in $SERVERS; do
      if [ -f ".$s.pid" ] && kill -0 "$(cat .$s.pid)" 2>/dev/null; then
        echo "$s already running (pid $(cat .$s.pid))"
        continue
      fi
      setsid ./"$s" > "$LOG_DIR/$s.log" 2>&1 < /dev/null &
      echo "$!" > ".$s.pid"
      echo "$s started (pid $!)"
    done
    # Allow daemons to fully detach/reparent before the calling session exits
    # (prevents WSL teardown from killing freshly-started processes)
    sleep 3
    ;;
  stop)
    for s in $SERVERS; do
      if [ -f ".$s.pid" ]; then
        pid=$(cat .$s.pid)
        if kill -0 "$pid" 2>/dev/null; then
          kill "$pid" && echo "$s stopped (pid $pid)"
        else
          echo "$s not running (stale pid)"
        fi
        rm -f ".$s.pid"
      else
        pkill -f "^\./$s$" 2>/dev/null && echo "$s stopped (pkill)" || echo "$s not running"
      fi
    done
    ;;
  status)
    for s in $SERVERS; do
      if [ -f ".$s.pid" ] && kill -0 "$(cat .$s.pid)" 2>/dev/null; then
        echo "$s running (pid $(cat .$s.pid))"
      else
        echo "$s down"
      fi
    done
    ;;
  *)
    echo "Usage: $0 {start|stop|status}"
    exit 1
    ;;
esac