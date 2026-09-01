### quote-history DEBUG ################################################
_qh_tmp="/tmp/.qh_capture_$$"
QUOTE_HISTORY="$HOME/.quote_history"
QH_BIN="$HOME/.local/bin/qh_extract"

_qh_active=0
_qh_stdout=""
_qh_stderr=""

qh_debug() {
	# only once for the actual command
	(( _qh_active )) && return

	# ignore our own internal commands
	[[ $BASH_COMMAND == qh_* ]] && return

	_qh_active=1
	: > "$_qh_tmp"

	exec 19>&1
	exec 20>&2

	exec > >(tee -a "$_qh_tmp" >&19) 2>&1
	RENDER_ECHO
}

qh_prompt() {
	(( _qh_active )) || return

	exec >&19 2>&20
	exec 19>&-
	exec 20>&-

	_qh_active=0

	[[ -s "$_qh_tmp" ]] &&
		"$QH_BIN" "$_qh_tmp" "$QUOTE_HISTORY" -g
}

trap 'qh_debug' DEBUG
PROMPT_COMMAND="qh_prompt${PROMPT_COMMAND:+;$PROMPT_COMMAND}"

# --- quote history consumer (Alt+Up / Alt+Down browsing, identicno .bash_history) ---
_qh_idx=0
_qh_entries=()
_qh_saved_buffer=""

_qh_load() {
	[[ -f "$QUOTE_HISTORY" ]] || return
# flat mode map each line
#	mapfile -t _qh_entries < "$QUOTE_HISTORY"
# group mode:
# map entries by RS separator, remove last empty element
	mapfile -d $'\x1e' -t _qh_entries < "$QUOTE_HISTORY"
	if (( ${#_qh_entries[@]} > 0 )) then
		unset '_qh_entries[-1]'
	fi
}

_qh_browse_up() {
	if (( _qh_idx == 0 )); then
		_qh_load
		_qh_saved_buffer="$READLINE_LINE"
	fi
	local n=${#_qh_entries[@]}
	(( n == 0 )) && return
	(( _qh_idx < n )) && (( _qh_idx++ ))
	READLINE_LINE="${_qh_entries[n - _qh_idx]}"
	READLINE_POINT=${#READLINE_LINE}
}

_qh_browse_down() {
	local n=${#_qh_entries[@]}
	if (( _qh_idx > 1 )); then
		(( _qh_idx-- ))
		READLINE_LINE="${_qh_entries[n - _qh_idx]}"
	else
		_qh_idx=0
		READLINE_LINE="$_qh_saved_buffer"
	fi
	READLINE_POINT=${#READLINE_LINE}
}

bind -x '"\e[1;3A": _qh_browse_up'
bind -x '"\e[1;3B": _qh_browse_down'
########################################################################
