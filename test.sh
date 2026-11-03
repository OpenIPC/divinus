#!/bin/bash
set -u

ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
TEST_ROOT="$ROOT/tst"

RED=$'\033[0;31m'
GREEN=$'\033[0;32m'
BLUE=$'\033[0;34m'
NC=$'\033[0m'

TESTS=()
RUN=()

usage() {
    printf 'Usage: %s [--all | --list | test ...]\n' "${0##*/}"
    printf 'Tests are immediate subdirectories of tst/, each with a run.sh.\n'
}

collect_tests() {
    local dir

    TESTS=()
    for dir in "$TEST_ROOT"/*/; do
        [ -d "$dir" ] || continue
        TESTS+=("${dir%/}")
    done
}

list_tests() {
    local path

    for path in "${TESTS[@]}"; do
        printf '%s\n' "${path##*/}"
    done
}

select_tests() {
    local selected=0 rendered=0 key seq i mark any
    local -a checked=()

    RUN=()
    printf '%sSelect tests%s (up/down, space to check, Enter to run, q to quit):\n' "$BLUE" "$NC"
    while true; do
        [ "$rendered" -eq 1 ] && printf '\033[%dA' "${#TESTS[@]}"
        rendered=1
        for i in "${!TESTS[@]}"; do
            if [ "${checked[i]:-0}" -eq 1 ]; then
                mark="[x]"
            else
                mark="[ ]"
            fi
            if [ "$i" -eq "$selected" ]; then
                printf '\033[2K\033[7m> %s %s\033[0m\n' "$mark" "${TESTS[i]##*/}"
            else
                printf '\033[2K  %s %s\n' "$mark" "${TESTS[i]##*/}"
            fi
        done

        IFS= read -rsn1 key || return 1
        case "$key" in
            "")
                any=0
                for i in "${!TESTS[@]}"; do
                    if [ "${checked[i]:-0}" -eq 1 ]; then
                        RUN+=("${TESTS[i]}")
                        any=1
                    fi
                done
                [ "$any" -eq 1 ] || RUN+=("${TESTS[selected]}")
                return 0
                ;;
            " ")
                if [ "${checked[selected]:-0}" -eq 1 ]; then
                    checked[selected]=0
                else
                    checked[selected]=1
                fi
                ;;
            q|Q)
                return 1
                ;;
            $'\e')
                IFS= read -rsn2 -t 0.05 seq || seq=""
                case "$seq" in
                    '[A') selected=$(((selected - 1 + ${#TESTS[@]}) % ${#TESTS[@]})) ;;
                    '[B') selected=$(((selected + 1) % ${#TESTS[@]})) ;;
                esac
                ;;
        esac
    done
}

run_test() {
    local path=$1 name=${1##*/} work status

    printf '\n%s==> %s%s\n' "$BLUE" "$name" "$NC"
    work=$(mktemp -d "${TMPDIR:-/tmp}/divinus-test.XXXXXX") || return 1
    work=$(cd -- "$work" && pwd -P) || return 1

    (
        trap 'rm -rf -- "$work"' EXIT HUP INT TERM
        cd -- "$work" || exit 1
        PROJECT_ROOT="$ROOT" TEST_NAME="$name" TEST_DIR="$path" WORK_DIR="$work" \
            bash -- "$path/run.sh"
    )
    status=$?

    if [ "$status" -eq 0 ]; then
        printf '%sPASS%s %s\n' "$GREEN" "$NC" "$name"
    else
        printf '%sFAIL%s %s (exit %d)\n' "$RED" "$NC" "$name" "$status"
    fi
    return "$status"
}

collect_tests
if [ "${#TESTS[@]}" -eq 0 ]; then
    printf 'No tests found in %s\n' "$TEST_ROOT" >&2
    exit 1
fi

if [ "$#" -eq 0 ]; then
    if [ ! -t 0 ] || [ ! -t 1 ]; then
        list_tests
        exit 0
    fi
    select_tests || exit 0
else
    RUN=()
    for arg in "$@"; do
        case "$arg" in
            -h|--help)
                usage
                exit 0
                ;;
            -l|--list)
                list_tests
                exit 0
                ;;
            -a|--all)
                RUN+=("${TESTS[@]}")
                ;;
            -*)
                usage >&2
                exit 2
                ;;
            *)
                found=""
                for path in "${TESTS[@]}"; do
                    if [ "${path##*/}" = "$arg" ]; then
                        found=$path
                        break
                    fi
                done
                if [ -z "$found" ]; then
                    printf 'Unknown test: %s\n' "$arg" >&2
                    exit 2
                fi
                RUN+=("$found")
                ;;
        esac
    done
fi

passed=0
failed=0
for path in "${RUN[@]}"; do
    if run_test "$path"; then
        passed=$((passed + 1))
    else
        failed=$((failed + 1))
    fi
done

printf '\n%d passed, %d failed\n' "$passed" "$failed"
[ "$failed" -eq 0 ]
