#!/bin/sh

if [ -z "${NCM_TEST_COMMAND_LOG}" ]; then
    exit 2
fi

echo "${NCM_TEST_CALLER:-unknown} $*" >> "${NCM_TEST_COMMAND_LOG}"
exit 0
