#!@RCD_SCRIPTS_SHELL@
#
# $NetBSD: kdm.sh,v 1.1 2007/12/23 01:53:02 mwdavies Exp $
#

# PROVIDE: kdm
# REQUIRE: DAEMON LOGIN wscons
# KEYWORD: shutdown

if [ -f /etc/rc.subr ]; then
	. /etc/rc.subr
fi

name="kdm"
rcvar=$name
command="@PREFIX@/bin/${name}"
pidfile="/var/run/kdm.pid"
required_files="@PKG_SYSCONFDIR@/kdm/kdmrc"
command_args="-config ${required_files}"

if [ -f /etc/rc.subr ]; then
	load_rc_config $name
	run_rc_command "$1"
else
	echo -n " ${name}"
	${command} ${kdm_flags} ${command_args}
fi
