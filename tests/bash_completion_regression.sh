#!/usr/bin/env bash

set -euo pipefail

bin_path="$1"
completion_script="$2"

bash -n "$completion_script"
export LTFA_COMPLETION_BIN="$bin_path"
source "$completion_script"

temp_root="$(mktemp -d)"
trap 'rm -rf -- "$temp_root"' EXIT
mkdir "$temp_root/space dir"

contains_reply() {
    local expected="$1"
    local reply
    for reply in "${COMPREPLY[@]}"; do
        if [[ $reply == "$expected" ]]; then
            return 0
        fi
    done
    return 1
}

COMP_WORDS=(ltfa mo)
COMP_CWORD=1
_ltfa_complete
[[ ${#COMPREPLY[@]} -eq 1 && ${COMPREPLY[0]} == model ]]

COMP_WORDS=(ltfa model --f)
COMP_CWORD=2
_ltfa_complete
contains_reply --format

COMP_WORDS=(ltfa model --format j)
COMP_CWORD=3
_ltfa_complete
[[ ${#COMPREPLY[@]} -eq 1 && ${COMPREPLY[0]} == json ]]

COMP_WORDS=(ltfa model --format json --)
COMP_CWORD=4
_ltfa_complete
contains_reply --model-root
if contains_reply --format; then
    printf 'used --format was suggested again\n' >&2
    exit 1
fi

COMP_WORDS=(ltfa model --model-root "$temp_root/sp")
COMP_CWORD=3
_ltfa_complete
contains_reply "$temp_root/space dir"

COMP_WORDS=(ltfa guide st)
COMP_CWORD=2
_ltfa_complete
[[ ${#COMPREPLY[@]} -eq 1 && ${COMPREPLY[0]} == statements ]]

COMP_WORDS=(ltfa guide statements r)
COMP_CWORD=3
_ltfa_complete
[[ ${#COMPREPLY[@]} -eq 1 && ${COMPREPLY[0]} == relations ]]

COMP_WORDS=(ltfa guide ob)
COMP_CWORD=2
_ltfa_complete
[[ ${#COMPREPLY[@]} -eq 1 && ${COMPREPLY[0]} == observations ]]

COMP_WORDS=(ltfa guide observations f)
COMP_CWORD=3
_ltfa_complete
contains_reply fields
contains_reply format

COMP_WORDS=(ltfa guide samples d)
COMP_CWORD=3
_ltfa_complete
[[ ${#COMPREPLY[@]} -eq 1 && ${COMPREPLY[0]} == diagnosis ]]
