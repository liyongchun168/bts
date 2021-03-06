#!/bin/bash
#
TEST="Test table against invalid PCCT"
NAME=test-0001.sh
TMPLOG=$TMP/pcct.log.$$

$FWTS --show-tests | grep PCCT > /dev/null
if [ $? -eq 1 ]; then
	echo SKIP: $TEST, $NAME
	exit 77
fi

$FWTS --log-format="%line %owner " -w 80 --dumpfile=$FWTSTESTDIR/pcct-0001/acpidump-0002.log pcct - | cut -c7- | grep "^pcct" > $TMPLOG
diff $TMPLOG $FWTSTESTDIR/pcct-0001/pcct-0002.log >> $FAILURE_LOG
ret=$?
if [ $ret -eq 0 ]; then
	echo PASSED: $TEST, $NAME
else
	echo FAILED: $TEST, $NAME
fi

rm $TMPLOG
exit $ret
