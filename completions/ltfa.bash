_ltfa_complete()
{
    local cur bin output mode candidate

    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    bin="${LTFA_COMPLETION_BIN:-${COMP_WORDS[0]}}"

    if ! output="$("$bin" __completion --bash "${COMP_WORDS[@]:1}" 2>/dev/null)"; then
        return 0
    fi

    mode="${output%%$'\n'*}"
    mode="${mode#__ltfa_completion_mode=}"
    output="${output#*$'\n'}"

    case "$mode" in
        dir)
            while IFS= read -r candidate; do
                COMPREPLY+=("$candidate")
            done < <(compgen -d -- "$cur")
            compopt -o filenames 2>/dev/null || true
            return 0
            ;;
        file)
            while IFS= read -r candidate; do
                COMPREPLY+=("$candidate")
            done < <(compgen -f -- "$cur")
            compopt -o filenames 2>/dev/null || true
            return 0
            ;;
        none)
            return 0
            ;;
        plain)
            ;;
        *)
            return 0
            ;;
    esac

    while IFS= read -r candidate; do
        if [[ -n $candidate && $candidate == "$cur"* ]]; then
            COMPREPLY+=("$candidate")
        fi
    done <<<"$output"
    return 0
}

complete -F _ltfa_complete ltfa
