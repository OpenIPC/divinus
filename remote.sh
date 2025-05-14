#!/bin/bash
#
# This script sets up a remote debugging environment using gdbserver and gdb-multiarch.
# It allows you to debug a binary on a remote board over SSH.
#
# Usage: ./remote.sh [BOARD_IP] [USER] [PASSWORD] [SSH_PORT] [GDB_COMMANDS]
#
# Dependencies:
# - gdb-multiarch
# - sshpass (optional, for password-based SSH)

BOARD_IP=${1:-"192.168.1.17"}
BOARD_USER=${2:-"root"}
BOARD_PASS=${3:-""}
BOARD_PORT=${4:-"22"}

GDB_PORT="5000"
GDBSERVER_URL="https://github.com/therealsaumil/static-arm-bins/raw/refs/heads/master/gdbserver-armel-static-8.0.1"
LOCAL_EXECUTABLE_PATH="divinus"
LOCAL_GDBSERVER_PATH="$(basename $GDBSERVER_URL)"
REMOTE_EXECUTABLE_PATH="/tmp/divinus"
REMOTE_GDBSERVER_PATH="/tmp/gdbserver"
SCP_OPTS="-q -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -P $BOARD_PORT"
SSH_CONTROL_PATH="/tmp/ssh_control_divinus_%h_%p_%r"
SSH_OPTS="-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ControlMaster=auto -o ControlPath=$SSH_CONTROL_PATH -o ControlPersist=yes -o LogLevel=ERROR -p $BOARD_PORT"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

check_requirements() {
    echo -e "${BLUE}Verifying dependencies...${NC}"

    if [ -n "$BOARD_PASS" ]; then
        command -v sshpass >/dev/null 2>&1 || { echo -e "${RED}sshpass not found. Installing...${NC}"; apt-get update && apt-get install -y sshpass; }
    fi
    command -v gdb-multiarch >/dev/null 2>&1 || { echo -e "${RED}gdb-multiarch not found. Installing...${NC}"; apt-get update && apt-get install -y gdb-multiarch; }
}

download_gdbserver() {
    echo -e "${BLUE}Downloading gdbserver...${NC}"
    
    if [ ! -f "$LOCAL_GDBSERVER_PATH" ]; then
        wget -q --show-progress "$GDBSERVER_URL" -O "$LOCAL_GDBSERVER_PATH" || \
        curl -L --progress-bar -s "$GDBSERVER_URL" -o "$LOCAL_GDBSERVER_PATH" || {
            echo -e "${RED}Downloading gdbserver failed!${NC}"
            exit 1
        }
        chmod +x "$LOCAL_GDBSERVER_PATH"
        echo -e "${GREEN}gdbserver downloaded successfully!${NC}"
    fi
}

establish_ssh_connection() {
    echo -e "${BLUE}Establishing main SSH connection...${NC}"
    
    if [ -S "$SSH_CONTROL_PATH" ]; then
        ssh -O check -o ControlPath=$SSH_CONTROL_PATH "$BOARD_USER@$BOARD_IP" &>/dev/null || {
            echo -e "${YELLOW}Removing stale SSH control socket...${NC}"
            rm -f "$SSH_CONTROL_PATH"
        }
    fi
    
    if [ -z "$BOARD_PASS" ]; then
        ssh $SSH_OPTS "$BOARD_USER@$BOARD_IP" "echo 'SSH connection established'" 2>/dev/null || {
            echo -e "${RED}Failed to establish SSH connection.${NC}"
            exit 1
        }
    else
        sshpass -p "$BOARD_PASS" ssh $SSH_OPTS "$BOARD_USER@$BOARD_IP" "echo 'SSH connection established'" 2>/dev/null || {
            echo -e "${RED}Failed to establish SSH connection.${NC}"
            exit 1
        }
    fi
}

kill_remote_gdbserver() {
    echo -e "${BLUE}Checking for existing gdbserver processes...${NC}"

    if [ -z "$BOARD_PASS" ]; then
        ssh $SSH_OPTS "$BOARD_USER@$BOARD_IP" "killall -9 gdbserver || true" 2>/dev/null
    else
        sshpass -p "$BOARD_PASS" ssh $SSH_OPTS "$BOARD_USER@$BOARD_IP" "killall -9 gdbserver || true" 2>/dev/null
    fi

    sleep 1
}

copy_files() {
    if [ ! -f "$LOCAL_EXECUTABLE_PATH" ]; then
        echo -e "${RED}Error: Local executable '$LOCAL_EXECUTABLE_PATH' not found.${NC}"
        exit 1
    fi

    if [ ! -f "$LOCAL_GDBSERVER_PATH" ] || [ ! -x "$LOCAL_GDBSERVER_PATH" ]; then
        echo -e "${RED}Error: gdbserver not found or not executable.${NC}"
        exit 1
    fi

    echo -e "${BLUE}Copying files to $BOARD_IP...${NC}"
    
    if [ -z "$BOARD_PASS" ]; then
        scp $SCP_OPTS "$LOCAL_GDBSERVER_PATH" "$BOARD_USER@$BOARD_IP:$REMOTE_GDBSERVER_PATH" || {
            echo -e "${RED}Transfering gdbserver failed.${NC}"
            exit 1
        }
        scp $SCP_OPTS "$LOCAL_EXECUTABLE_PATH" "$BOARD_USER@$BOARD_IP:$REMOTE_EXECUTABLE_PATH" || {
            echo -e "${RED}Transfering '$LOCAL_EXECUTABLE_PATH' failed.${NC}"
            exit 1
        }
    else
        sshpass -p "$BOARD_PASS" scp $SCP_OPTS "$LOCAL_GDBSERVER_PATH" "$BOARD_USER@$BOARD_IP:$REMOTE_GDBSERVER_PATH" || {
            echo -e "${RED}Transfering gdbserver failed.${NC}"
            exit 1
        }
        sshpass -p "$BOARD_PASS" scp $SCP_OPTS "$LOCAL_EXECUTABLE_PATH" "$BOARD_USER@$BOARD_IP:$REMOTE_EXECUTABLE_PATH" || {
            echo -e "${RED}Transfering '$LOCAL_EXECUTABLE_PATH' failed.${NC}"
            exit 1
        }
    fi
    
    echo -e "${GREEN}Files copied successfully!${NC}"
}

start_gdbserver() {
    echo -e "${BLUE}Starting gdbserver on the board...${NC}"
    
    if [ -z "$BOARD_PASS" ]; then
        ssh $SSH_OPTS "$BOARD_USER@$BOARD_IP" "chmod +x $REMOTE_GDBSERVER_PATH && $REMOTE_GDBSERVER_PATH :$GDB_PORT $REMOTE_EXECUTABLE_PATH > /tmp/gdbserver.log 2>&1 & echo \$! > /tmp/gdbserver.pid"
    else
        sshpass -p "$BOARD_PASS" ssh $SSH_OPTS "$BOARD_USER@$BOARD_IP" "chmod +x $REMOTE_GDBSERVER_PATH && $REMOTE_GDBSERVER_PATH :$GDB_PORT $REMOTE_EXECUTABLE_PATH > /tmp/gdbserver.log 2>&1 & echo \$! > /tmp/gdbserver.pid"
    fi
    
    sleep 2
    if [ -z "$BOARD_PASS" ]; then
        SERVER_PID=$(ssh $SSH_OPTS "$BOARD_USER@$BOARD_IP" "if [ -f /tmp/gdbserver.pid ]; then cat /tmp/gdbserver.pid; else echo ''; fi")
    else
        SERVER_PID=$(sshpass -p "$BOARD_PASS" ssh $SSH_OPTS "$BOARD_USER@$BOARD_IP" "if [ -f /tmp/gdbserver.pid ]; then cat /tmp/gdbserver.pid; else echo ''; fi")
    fi
    
    if [ -n "$SERVER_PID" ]; then
        if [ -z "$BOARD_PASS" ]; then
            PID_EXISTS=$(ssh $SSH_OPTS "$BOARD_USER@$BOARD_IP" "kill -0 $SERVER_PID 2>/dev/null && echo 'yes' || echo 'no'")
        else
            PID_EXISTS=$(sshpass -p "$BOARD_PASS" ssh $SSH_OPTS "$BOARD_USER@$BOARD_IP" "kill -0 $SERVER_PID 2>/dev/null && echo 'yes' || echo 'no'")
        fi
        
        if [ "$PID_EXISTS" = "yes" ]; then
            echo -e "${GREEN}gdbserver started with PID $SERVER_PID on $BOARD_IP:$GDB_PORT.${NC}"
            return 0
        fi
    fi
    
    echo -e "${RED}Failed to start gdbserver. Check /tmp/gdbserver.log on the remote system.${NC}"
    exit 1
}

create_gdbinit() {
    echo -e "${BLUE}Creating .gdbinit file...${NC}"
    
    cat > .gdbinit << EOF
target remote $BOARD_IP:$GDB_PORT
set sysroot /
set solib-search-path ./
# Other GDB settings here!
EOF
    
    echo -e "${GREEN}.gdbinit file created successfully!${NC}"
}

start_gdb() {
    echo -e "${YELLOW}Launching GDB...${NC}"
    echo -e "${YELLOW}To continue execution, type 'continue' or 'c'${NC}"
    
    gdb-multiarch -q -x .gdbinit "$LOCAL_EXECUTABLE_PATH"
}

close_ssh_connection() {
    echo -e "${BLUE}Closing persistent SSH connection...${NC}"
    ssh -O exit -o ControlPath=$SSH_CONTROL_PATH "$BOARD_USER@$BOARD_IP" 2>/dev/null || true
}

main() {
    check_requirements
    download_gdbserver
    establish_ssh_connection
    kill_remote_gdbserver
    copy_files
    start_gdbserver
    create_gdbinit
    start_gdb
    close_ssh_connection
}

cleanup() {
    echo -e "\n${YELLOW}Cleaning up...${NC}"
    kill_remote_gdbserver
    close_ssh_connection
    exit 0
}

trap cleanup INT TERM EXIT

if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    echo "Usage: $0 [BOARD_IP] [USER] [PASSWORD] [SSH_PORT] [GDB_COMMANDS]"
    echo "  BOARD_IP     : Board IP address (default: 192.168.1.10)"
    echo "  USER         : SSH user (default: root)"
    echo "  PASSWORD     : SSH password (default: empty)"
    echo "  SSH_PORT     : SSH port (default: 22)"
    echo "  GDB_COMMANDS : Additional GDB commands to execute at startup (optional)"
    exit 0
fi

main