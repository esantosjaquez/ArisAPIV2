#!/bin/bash
# Auto-sync script for ArisAPIV2
# Pulls changes from GitHub, rebuilds, and restarts service if needed

REPO_DIR="/home/resonance/CrSDK_v2.00.00_20251030a_Linux64PC/rest_server"
BUILD_DIR="/home/resonance/CrSDK_v2.00.00_20251030a_Linux64PC/build"
LOG_FILE="/var/log/arisapi-git-sync.log"
SERVICE_NAME="arisapi.service"

cd "$REPO_DIR" || exit 1

# Ensure SSH agent is running
eval "$(ssh-agent -s)" > /dev/null 2>&1
ssh-add ~/.ssh/id_ed25519 > /dev/null 2>&1

# Fetch and check for updates
git fetch origin main 2>&1

LOCAL=$(git rev-parse HEAD)
REMOTE=$(git rev-parse origin/main)

if [ "$LOCAL" != "$REMOTE" ]; then
    echo "[$(date)] New version detected. Pulling updates..." >> "$LOG_FILE"

    # Pull changes
    git pull origin main >> "$LOG_FILE" 2>&1

    # Rebuild
    echo "[$(date)] Rebuilding..." >> "$LOG_FILE"
    cd "$BUILD_DIR" && cmake --build . --target CrSDKRestServer >> "$LOG_FILE" 2>&1
    BUILD_RESULT=$?

    if [ $BUILD_RESULT -eq 0 ]; then
        echo "[$(date)] Build successful. Restarting service..." >> "$LOG_FILE"
        systemctl restart "$SERVICE_NAME" >> "$LOG_FILE" 2>&1
        echo "[$(date)] Service restarted." >> "$LOG_FILE"
    else
        echo "[$(date)] ERROR: Build failed! Service NOT restarted." >> "$LOG_FILE"
    fi
else
    echo "[$(date)] Already up to date." >> "$LOG_FILE"
fi
