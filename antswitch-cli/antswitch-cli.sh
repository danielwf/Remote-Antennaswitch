#!/bin/bash

# Pfade definieren
BASE_DIR="/usr/local/antswitch-cli"
CONFIG_FILE="$BASE_DIR/antswitch-cli.config"
PORTS_FILE="$BASE_DIR/antswitch-cli.ports"
FRQ_FILE="$BASE_DIR/antswitch-cli.frq-map"

# 1. Konfiguration laden
if [[ -f "$CONFIG_FILE" ]]; then
    source "$CONFIG_FILE"
else
    echo "Fehler: $CONFIG_FILE nicht gefunden!"
    exit 1
fi

# --- NEU: Globaler Schalter für Sperre ignorieren ---
IGNORE_LOCK=0

# Wir scannen die Argumente vorab, um zu sehen ob --nolock dabei ist
# (Damit die Position des Parameters egal ist)
for arg in "$@"; do
    if [[ "$arg" == "--nolock" ]]; then
        IGNORE_LOCK=1
        break
    fi
done

# Hilfsfunktion: URL-Encoding für Sonderzeichen (Rein Bash)
urlencode() {
    local string="${1}"
    local strlen=${#string}
    local encoded=""
    for (( pos=0 ; pos<strlen ; pos++ )); do
        c=${string:$pos:1}
        case "$c" in
            [a-zA-Z0-9.~_-]) o="${c}" ;;
            *)               printf -v o '%%%02X' "'$c" ;;
        esac
        encoded+="${o}"
    done
    echo "${encoded}"
}

# Hilfsfunktion: INI-Werte lesen
get_ini_val() {
    local section=$1
    local key=$2
    sed -n "/^\[$section\]/,/^\[/p" "$PORTS_FILE" | grep "^$key=" | head -1 | cut -d'=' -f2 | tr -d '"'
}

# Zentrale Ausführung (Double-Eval für die Verschachtelung in der Config)
exec_api() {
    local template="$1"
    local port="$2"
    local label="$3"
    
    # Platzhalter ersetzen
    local substituted=$(echo "$template" | sed "s|{PORT}|$port|g" | sed "s|{LABEL}|$label|g")
    
    # Ausführen (Variablen wie ${COMMAND_BASE} werden hier aufgelöst)
    eval "eval $substituted"
}

# Sperr-Check (ERWEITERT)
check_lock() {
    # Wenn --nolock gesetzt wurde, ignorieren wir die Sperre komplett
    if [[ "$IGNORE_LOCK" -eq 1 ]]; then
        return 0
    fi

    if [[ -z "$CHECK_CMD_FOR_LOCK" ]]; then return 0; fi
    LOCK_RESULT=$(eval "$CHECK_CMD_FOR_LOCK" 2>/dev/null | xargs)
    [[ -n "$LOCK_RESULT" ]] && return 1 || return 0
}

# Download der Labels vom Schalter und Neuerstellung der .ports
sync_hardware_to_ports() {
    if [[ "$DOWNLOAD_LABELS" != "1" ]]; then
        echo "no download from hardware (DOWNLOAD_LABELS=0 in config)."
        return
    fi
    
    echo "Writing labels from hardware to $PORTS_FILE..."
    
    # Reading the number of ports and defaults in current .port-file
    local in_count=$(get_ini_val "Labels" "Inputs")
    local out_count=$(get_ini_val "Labels" "Outputs")
    local default_in=$(get_ini_val "Defaults" "DEFAULT_INPUT")
    local default_out=$(get_ini_val "Defaults" "DEFAULT_OUTPUT")
    # Fallback if empty
    [[ -z "$in_count" ]] && in_count=3
    [[ -z "$out_count" ]] && out_count=5
    [[ -z "$default_in" ]] && default_in=1
    [[ -z "$default_out" ]] && default_out=1

    {
        echo "# Global config of antennas and labels"
        echo "# This file can be used by other programms too!"
        echo "# File will be overwritten by download from antenna switch (if selected in config-file)"
        echo "[Labels]"
        echo "Inputs=$in_count"
        echo "Outputs=$out_count"
        
        for i in $(seq 1 $in_count); do
            local val=$(exec_api "$CMD_GET_LABEL_IN" "$i")
            echo "In_$i=\"$val\""
        done
        
        for i in $(seq 1 $out_count); do
            local val=$(exec_api "$CMD_GET_LABEL_OUT" "$i")
            echo "Out_$i=\"$val\""
        done
        
        echo ""
        echo "[Defaults]"
        echo "DEFAULT_INPUT=$default_in"
        echo "DEFAULT_OUTPUT=$default_out"
    } > "$PORTS_FILE"
    echo "Fertig."
}

# Automatic selection by frequency
find_port_by_freq() {
    local freq=$1
    [[ -z "$freq" ]] && return
    grep -E "^[0-9]+-[0-9]+=" "$FRQ_FILE" | while read -r line; do
        range=$(echo "$line" | cut -d'=' -f1)
        port_tuner=$(echo "$line" | cut -d'=' -f2) # Beachtet das Format Port|Tuner
        port=$(echo "$port_tuner" | cut -d'|' -f1)
        start=$(echo "$range" | cut -d'-' -f1)
        end=$(echo "$range" | cut -d'-' -f2)
        if [ "$freq" -ge "$start" ] && [ "$freq" -le "$end" ]; then
            echo "$port"
            break
        fi
    done
}

# --- Main Program ---

case "$1" in
    --set-in)
        check_lock || { echo "Gesperrt!"; exit 1; }
        exec_api "$CMD_SET_INPUT" "$2"
        ;;
    --set-out)
        check_lock || { echo "Gesperrt!"; exit 1; }
        exec_api "$CMD_SET_OUTPUT" "$2"
        ;;
    --set-freq)
        check_lock || { echo "Gesperrt!"; exit 1; }
        PORT=$(find_port_by_freq "$2")
        [[ -n "$PORT" ]] && exec_api "$CMD_SET_OUTPUT" "$PORT"
        ;;
    --get-in)    exec_api "$CMD_GET_INPUT" ;;
    --get-out)   exec_api "$CMD_GET_OUTPUT" ;;
    --swr)       exec_api "$CMD_GET_SWR" ;;
    --pwr)       exec_api "$CMD_GET_POWER" ;;
    --status)
        echo "In: $(exec_api "$CMD_GET_INPUT") | Out: $(exec_api "$CMD_GET_OUTPUT")"
        echo "SWR: $(exec_api "$CMD_GET_SWR") | PWR: $(exec_api "$CMD_GET_POWER")"
        ;;
    --sync)
        sync_hardware_to_ports
        ;;
    --ports-default)
        DEF_IN=$(get_ini_val "Defaults" "DEFAULT_INPUT")
        DEF_OUT=$(get_ini_val "Defaults" "DEFAULT_OUTPUT")
        [[ -z "$DEF_IN" ]] && DEF_IN=1
        [[ -z "$DEF_OUT" ]] && DEF_OUT=1
        echo "Setze Default-Ports (In: $DEF_IN, Out: $DEF_OUT)"
        # Hier nutzen wir direkt exec_api, um check_lock-Probleme bei Defaults zu umgehen
        exec_api "$CMD_SET_INPUT" "$DEF_IN"
        exec_api "$CMD_SET_OUTPUT" "$DEF_OUT"
        ;;
    *)
     echo "Nutzung: $0 {--set-in|--set-out|--set-freq|--get-in|--get-out|--swr|--pwr|--status|--sync|--ports-default} [--nolock]"
     ;;
esac
