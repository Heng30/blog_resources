#来自rsyslog的启动脚本

#!/bin/bash
# Source function library.
. /etc/init.d/functions

RETVAL=0
prog=YOUR_PROGRAM_NAME
exec=YOUR_PROGRAM_PATH
args=YOUR_PROGRAM_ARGS
lockfile=/var/lock/subsys/$prog
PIDFILE=/var/run/${prog}.pid

# Source config
if [ -f /etc/sysconfig/$prog ] ; then
    . /etc/sysconfig/$prog
fi

start() {
	[ -x $exec ] || exit 5

	umask 077

        echo -n $"Starting " $prog
        daemon --pidfile="$PIDFILE" $exec $args
        RETVAL=$?
        echo
        [ $RETVAL -eq 0 ] && touch $lockfile
        return $RETVAL
}
stop() {
        echo -n $"Shutting down " $prog
        killproc -p "$PIDFILE" $exec
        RETVAL=$?
        echo
        [ $RETVAL -eq 0 ] && rm -f $lockfile
        return $RETVAL
}
rhstatus() {
        status -p "$PIDFILE" -l $prog $exec
}
restart() {
        stop
        start
}

case "$1" in
  start)
        start
        ;;
  stop)
        stop
        ;;
  restart)
        restart
        ;;
  reload)
        exit 3
        ;;
  force-reload)
        restart
        ;;
  status)
        rhstatus
        ;;
  try-restart)
        rhstatus >/dev/null 2>&1 || exit 0
        restart
        ;;
  *)
        echo $"Usage: $0 {start|stop|restart|try-restart|reload|force-reload|status}"
        exit 3
esac

exit $?
