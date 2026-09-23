#!/bin/bash
# minimal GTP fake engine for offline tests: echoes protocol behavior
# usage: fake_engine.sh <mode> ; modes: basic | crash
MODE="${1:-basic}"
while IFS= read -r line; do
    [ -z "$line" ] && continue
    case "$line" in
        \#*) continue ;;
    esac
    id="${line%% *}"; rest="${line#* }"
    [ "$rest" = "$line" ] && rest=""
    # analyze command: rest is "kata-analyze interval 50" — match the first word
    firstWord="${rest%% *}"
    case "$firstWord" in
        kata-analyze|lz-analyze)
            # stream a few info frames then a blank-line terminator, keep serving
            printf "info move D4 visits 120 winrate 0.55 scoreLead 0.3\n"
            printf "info move Q16 visits 80 winrate 0.45 scoreLead -0.2\n"
            printf "\n"
            continue ;;
        boardsize|clear_board|komi)
            printf "=%s\n\n" "$id"
            continue ;;
        genmove*)
            if [ -n "$YIGO_FAKE_GENMOVE" ]; then printf "=%s %s\n\n" "$id" "$YIGO_FAKE_GENMOVE"
            else printf "=%s D4\n\n" "$id"; fi
            continue ;;
        play*)
            printf "=%s\n\n" "$id"
            continue ;;
    esac
    case "$rest" in
        name) printf "=%s FakeEngine\n\n" "$id" ;;
        version) printf "=%s 1.0\n\n" "$id" ;;
        protocol_version) printf "=%s 2\n\n" "$id" ;;
        list_commands) printf "=%s name version protocol_version quit genmove play boardsize clear_board komi\n\n" "$id" ;;
        quit) printf "=%s\n\n" "$id"; exit 0 ;;
        genmove)
            if [ -n "$YIGO_FAKE_GENMOVE" ]; then printf "=%s %s\n\n" "$id" "$YIGO_FAKE_GENMOVE"
            else printf "=%s D4\n\n" "$id"; fi ;;
        play|boardsize|clear_board|komi) printf "=%s\n\n" "$id" ;;
        please*) kill -9 $$ ;;   # simulate hard crash mid-session
        *) printf "?%s unknown command\n\n" "$id" ;;
    esac
done

