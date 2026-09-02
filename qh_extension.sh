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

# --- quote history consumer (Alt+Up / Alt+Down browsing, identical to .bash_history) ---
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
	if (( ${#_qh_entries[@]} > 0 )); then
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

_qh_delete_current() {
# Do nothing if we are not actively browsing history
	(( _qh_idx == 0 )) && return

	local n=${#_qh_entries[@]}
	(( n == 0 )) && return

# Calculate the absolute array index of the currently viewed entry
	local target_idx=$(( n - _qh_idx ))

# Remove the element from the in-memory Bash array
	unset '_qh_entries[target_idx]'

# Re-index the array to prevent gaps/empty indices
	_qh_entries=("${_qh_entries[@]}")

# Sync changes back to the $QUOTE_HISTORY file using the RS separator
	if (( ${#_qh_entries[@]} > 0 )); then
		printf "%s\x1e" "${_qh_entries[@]}" > "$QUOTE_HISTORY"
	else
		> "$QUOTE_HISTORY" # Clear the file completely if history becomes empty
	fi

# Smart prompt update logic after deletion
	local new_n=${#_qh_entries[@]}

	if (( new_n == 0 )); then
# If no entries are left, restore the original user input buffer
		_qh_idx=0
		READLINE_LINE="$_qh_saved_buffer"
	elif (( _qh_idx > new_n )); then
# If the oldest entry was deleted, shift the index to the new oldest entry
		_qh_idx=$new_n
		READLINE_LINE="${_qh_entries[0]}"
	else
# Otherwise, display the entry that naturally moved into the current index slot
		READLINE_LINE="${_qh_entries[new_n - _qh_idx]}"
	fi

	READLINE_POINT=${#READLINE_LINE}
}

# Keybindings
bind -x '"\e[1;3A": _qh_browse_up'
bind -x '"\e[1;3B": _qh_browse_down'
bind -x '"\e[3;3~": _qh_delete_current'
########################################################################
