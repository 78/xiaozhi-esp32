#!/bin/sh
# Simple wrapper for arm-none-eabi-ar to accept CMake's `--create` long option
# and translate it to the short flags accepted by this ar implementation.

CMD="arm-none-eabi-ar"
newargs=""
create_requested=0

# collect args, but handle --create specially to avoid duplicating short flags
for a in "$@"; do
  if [ "${a}" = "--create" ]; then
    create_requested=1
    # do not append now; we'll only append if no short op is present
    continue
  fi
  newargs="${newargs} ${a}"
done

# if --create was requested and no short operation containing 'r' present, add -cr
if [ "${create_requested}" -eq 1 ]; then
  has_short=0
  printf '%s' "${newargs}" | grep -q -- '-cr' && has_short=1
  printf '%s' "${newargs}" | grep -q -- ' -q' && has_short=1
  if [ "${has_short}" -eq 0 ]; then
    newargs="-cr ${newargs}"
  fi
fi

# shellcheck disable=SC2086
eval "exec ${CMD} ${newargs}"
