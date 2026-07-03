#!/bin/bash
DIR=$(cd "$(dirname "$0")" && pwd)
MYSQL_CONTAINER="mysql"

wait_for_mysql() {
    for i in $(seq 1 30); do
        if sudo docker exec $MYSQL_CONTAINER mysqladmin ping -uragnarok -pragnarok --silent 2>/dev/null; then
            return 0
        fi
        sleep 2
    done
    return 1
}

wait_for_port() {
    local port=$1
    local name=$2
    for i in $(seq 1 15); do
        if ss -tlnp | grep -q ":$port "; then
            echo "$name OK (port $port)"
            return 0
        fi
        sleep 1
    done
    echo "WARNING: $name may not be ready (port $port)"
    return 1
}

import_sql() {
    echo "Importing SQL tables..."
    for sql in sql-files/main.sql sql-files/logs.sql; do
        if [ -f "$sql" ]; then
            echo "  Importing $sql..."
            sudo docker exec -i $MYSQL_CONTAINER mysql -uroot -pragnarok ragnarok < "$sql"
        fi
    done
    echo "SQL import done."
}

need_import() {
    local count=$(sudo docker exec $MYSQL_CONTAINER mysql -uroot -pragnarok -N -e \
        "SELECT COUNT(*) FROM information_schema.TABLES WHERE table_schema='ragnarok';" 2>/dev/null)
    [ -z "$count" ] || [ "$count" -lt 10 ]
}

init_mysql_data() {
    echo "Initializing MySQL data directory..."
    sudo docker run --rm \
        -e MYSQL_ROOT_PASSWORD=ragnarok \
        -e MYSQL_DATABASE=ragnarok \
        -e MYSQL_USER=ragnarok \
        -e MYSQL_PASSWORD=ragnarok \
        -v mysql_data:/var/lib/mysql \
        mysql:8.0 --initialize-insecure 2>/dev/null
    echo "Done."
}

case "$1" in
  start)
    echo "Starting MySQL..."
    if ! sudo docker start $MYSQL_CONTAINER 2>/dev/null; then
        sudo docker run -d --name $MYSQL_CONTAINER \
            --restart=always \
            -e MYSQL_ROOT_PASSWORD=ragnarok \
            -e MYSQL_DATABASE=ragnarok \
            -e MYSQL_USER=ragnarok \
            -e MYSQL_PASSWORD=ragnarok \
            -p 3306:3306 \
            -v mysql_data:/var/lib/mysql \
            mysql:8.0
    fi

    if ! wait_for_mysql; then
        echo "ERROR: MySQL failed to start"
        echo "Try: bash run.sh clean-db"
        exit 1
    fi
    echo "MySQL is ready."

    if need_import; then
        import_sql
    fi

    echo "Starting rAthena servers..."
    cd "$DIR"

    screen -dmS login ./login-server
    wait_for_port 6900 "login-server"

    screen -dmS char ./char-server
    wait_for_port 6121 "char-server"

    screen -dmS map ./map-server
    wait_for_port 5121 "map-server"

    echo "Starting web-server..."
    nohup ./web-server > log/web-server.log 2>&1 &
    echo "Done."
    ;;

  stop)
    echo "Stopping rAthena..."
    for svr in map-server char-server login-server web-server; do
        pkill -15 -f "./$svr" 2>/dev/null
    done
    sleep 5
    for svr in map-server char-server login-server web-server; do
        if pgrep -f "./$svr" >/dev/null 2>&1; then
            pkill -9 -f "./$svr" 2>/dev/null
        fi
    done

    echo "Stopping MySQL..."
    sudo docker stop $MYSQL_CONTAINER
    echo "All stopped."
    ;;

  restart)
    echo "Restarting rAthena..."
    for svr in map-server char-server login-server web-server; do
        pkill -15 -f "./$svr" 2>/dev/null
    done
    sleep 3
    for svr in map-server char-server login-server web-server; do
        if pgrep -f "./$svr" >/dev/null 2>&1; then
            pkill -9 -f "./$svr" 2>/dev/null
        fi
    done
    sleep 2

    cd "$DIR"
    screen -dmS login ./login-server
    wait_for_port 6900 "login-server"

    screen -dmS char ./char-server
    wait_for_port 6121 "char-server"

    screen -dmS map ./map-server
    wait_for_port 5121 "map-server"

    echo "Starting web-server..."
    nohup ./web-server > log/web-server.log 2>&1 &
    echo "Done."
    ;;

  status)
    echo "=== MySQL ==="
    if sudo docker ps --filter name=$MYSQL_CONTAINER --format "{{.Names}} {{.Status}}" 2>/dev/null | grep -q .; then
        sudo docker ps --filter name=$MYSQL_CONTAINER --format "{{.Names}} {{.Status}}"
    else
        echo "mysql: stopped"
    fi
    echo
    echo "=== rAthena ==="
    for svr in login char map; do
        if screen -S "$svr" -Q select . >/dev/null 2>&1; then
            echo "$svr-server: running"
        else
            echo "$svr-server: stopped"
        fi
    done
    if pgrep -f "./web-server" >/dev/null 2>&1; then
        echo "web-server: running"
    else
        echo "web-server: stopped"
    fi
    ;;

  reload-sql)
    if sudo docker ps --filter name=$MYSQL_CONTAINER --format "{{.Names}}" | grep -q .; then
        import_sql
    else
        echo "MySQL is not running. Start it first."
        exit 1
    fi
    ;;

  clean-db)
    echo "WARNING: This will delete all MySQL data!"
    read -p "Continue? [y/N] " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        sudo docker stop $MYSQL_CONTAINER 2>/dev/null
        sudo docker rm $MYSQL_CONTAINER 2>/dev/null
        sudo docker volume rm mysql_data 2>/dev/null
        echo "MySQL data cleaned. Run 'bash run.sh start' to recreate."
    fi
    ;;

  *)
    echo "Usage: $0 {start|stop|restart|status|reload-sql|clean-db}"
    echo ""
    echo "web-server is automatically started/stopped with the rest."
    ;;
esac
