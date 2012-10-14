#!/bin/sh

ntest=1

basename=`basename ${dir}`
echo ${dir} | egrep '^/' >/dev/null 2>&1
if [ $? -eq 0 ]; then
	command="${dir}/../test_${basename}"
else
	command="`pwd`/${dir}/../test_${basename}"
fi

if [ "$basename" = "manage" ]; then
	command="`pwd`/../../manage/hfsm"
fi

if [ ! -x "$command" ]; then
	echo "can not run $command"
	exit
fi

echo "command = $command"

VALGRIND_DIR=${VALGRIND_DIR:-/tmp}
if [ ! -d "${VALGRIND_DIR}" ]; then
	mkdir -p "${VALGRIND_DIR}"
fi
VALGRIND_LOG=${VALGRIND_LOG:-${VALGRIND_DIR}/valgrind_${basename}.log}
EXIT_IF_FAIL=${EXIT_IF_FAIL:-TRUE}
ERR2STR=${ERR2STR:-./err2str}

valgrind -q --log-file-exactly=/tmp/log echo -n "" 2>/dev/null
if [ $? -eq 0 ]; then
	LOG_ARG="--log-file-exactly"
else
	valgrind -q --log-file=/tmp/log echo -n "" 2>/dev/null
	if [ $? -eq 0 ]; then
		LOG_ARG="--log-file"
	else
		echo "please check the version of valgrind"
		exit 1
	fi
fi
VALGRIND="valgrind -q $LOG_ARG=$VALGRIND_LOG"

expect()
{
	RET="${1}"
	shift
	
	out=`echo "${IN}" | $VALGRIND ${command} $* 2>/dev/null`
	ret=$?
	ret=$(${ERR2STR} ${ret})	
	
	FAIL="TRUE"
	if [ "${ret}" != "${RET}" ]; then
		echo "not ok ${ntest} - return ${ret}: expect ${RET} $*"
	else
		if [ "${OUT}" != "" ]; then
			#delete empty line
			out=$(echo "${out}" | sed '/^$/d')
			OUT=$(echo "${OUT}" | sed '/^$/d')
		fi
		# Sometimes we are too lazy to write right output
		if [ "${OUT}" != "" -a "${out}" != "${OUT}" ]; then
			echo "not ok ${ntest} - output \"${out}\": expect \"${OUT}\""
		else
			if [ -s "${VALGRIND_LOG}" ]; then
				echo "not ok ${ntest} - valgrind report error, see ${VALGRIND_LOG}"
				else
			echo "ok ${ntest}: expect ${RET} $*"
				FAIL="FALSE"
			fi
		fi
	fi
	
	if [ "${FAIL}" = "TRUE" -a "${EXIT_IF_FAIL}" = "TRUE" ]; then
		echo "EXSTING..."
		exit -1
	fi
	ntest=`expr $ntest + 1`
}
