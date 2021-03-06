<?php
/*
	quagga_ospfd.inc
	Copyright (C) 2010 Ermal Lu�i
	Copyright (C) 2012 Jim Pingle
	part of pfSense
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	1. Redistributions of source code must retain the above copyright notice,
	   this list of conditions and the following disclaimer.

	2. Redistributions in binary form must reproduce the above copyright
	   notice, this list of conditions and the following disclaimer in the
	   documentation and/or other materials provided with the distribution.

	THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
	INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
	AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
	AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
	OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/

$quagga_config_base = "/var/etc/quagga";
$pkg_login	= "quagga";
$pkg_uid	= "101";
$pkg_group	= "quagga";
$pkg_gid	= "101";
$pkg_gecos	= "Quagga route daemon pseudo user";
$pkg_homedir	= "/var/etc/quagga";
$pkg_shell	= "/usr/sbin/nologin";

function quagga_ospfd_get_interfaces() {
	global $config;
	$interfaces = get_configured_interface_with_descr();
	$ospf_ifs = array();
	foreach ($interfaces as $iface => $ifacename) {
		$tmp["name"]  = $ifacename;
		$tmp["value"] = $iface;
		$ospf_ifs[] = $tmp;
	}

	foreach (array('server', 'client') as $mode) {
		if (is_array($config['openvpn']["openvpn-{$mode}"])) {
			foreach ($config['openvpn']["openvpn-{$mode}"] as $id => $setting) {
				if (!isset($setting['disable'])) {
					$tmp["name"]  = gettext("OpenVPN") . " ".$mode.": ".htmlspecialchars($setting['description']);
					$tmp["value"] = 'ovpn' . substr($mode, 0, 1) . $setting['vpnid'];
					$ospf_ifs[] = $tmp;
				}
			}
		}
	}

	return $ospf_ifs;
}

function quagga_ospfd_install_conf() {
	global $config, $g, $input_errors, $quagga_config_base, $pkg_login, $pkg_uid, $pkg_group, $pkg_gid, $pkg_gecos, $pkg_homedir, $pkg_shell;

	conf_mount_rw();

	if ($config['installedpackages']['quaggaospfd']['rawconfig'] && $config['installedpackages']['quaggaospfd']['rawconfig']['item']) {
		// if there is a raw config specifyed in tthe config.xml use that instead of the assisted config
		$conffile = implode("\n",$config['installedpackages']['quaggaospfd']['rawconfig']['item']);
		//$conffile = $config['installedpackages']['quaggaospfd']['rawconfig'];
	} else {
		// generate ospfd.conf based on the assistant
		if($config['installedpackages']['quaggaospfd']['config'])
			$ospfd_conf = &$config['installedpackages']['quaggaospfd']['config'][0];
		else {
			log_error("Quagga OSPFd: No config data found.");
			return;
		}

		$conffile = "# This file was created by the pfSense package manager.  Do not edit!\n\n";

		if($ospfd_conf['password'])
			$conffile .= "password {$ospfd_conf['password']}\n";

		if ($ospfd_conf['logging'])
			$conffile .= "log syslog\n";

		/* Interface Settings */
		$passive_interfaces = array();
		$interface_networks = array();

		if ($config['installedpackages']['quaggaospfdinterfaces']['config']) {
			foreach ($config['installedpackages']['quaggaospfdinterfaces']['config'] as $conf) {
				$realif = get_real_interface($conf['interface']);
				$conffile .= "interface {$realif}\n" ;
				if (!empty($conf['metric'])) {
					$conffile .= "  ip ospf cost {$conf['metric']}\n";
				}
				if (!empty($conf['hellointervalinseconds'])) {
					$conffile .= "  ip ospf hello-interval {$conf['hellointervalinseconds']}\n";
				}
				if ($conf['md5password'] && !empty($conf['password'])) {
					$conffile .= "  ip ospf authentication message-digest\n";
					$conffile .= "  ip ospf message-digest-key 1 md5 \"" . substr($conf['password'], 0, 15) . "\"\n";
				} else if (!empty($conf['password'])) {
					$conffile .= "  ip ospf authentication-key \"" . substr($conf['password'], 0, 8) . "\"\n";
				}
				if (!empty($conf['routerpriorityelections'])) {
					$conffile .= "  ip ospf priority {$conf['routerpriorityelections']}\n";
				}
				if (!empty($conf['retransmitinterval'])) {
					$conffile .= "  ip ospf retransmit-interval {$conf['retransmitinterval']}\n";
				}
				if (!empty($conf['deadtimer'])) {
					$conffile .= "  ip ospf dead-interval {$conf['deadtimer']}\n";
		}
				if (!empty($conf['passive'])) {
					$passive_interfaces[] = $realif;
				}
				$interface_ip = find_interface_ip($realif);
				$interface_subnet = find_interface_subnet($realif);
				/* Cheap hack since point-to-points won't attach if /32 is used. */
				if ($interface_subnet == 32)
					$interface_subnet = 30;
				$subnet = gen_subnet($interface_ip, $interface_subnet);
				if (!empty($conf['interfacearea'])) {
					$interface_networks[] = array( "subnet" => "{$subnet}/{$interface_subnet}", "area" => $conf['interfacearea']);
				}
				else {
					$interface_networks[] = array( "subnet" => "{$subnet}/{$interface_subnet}", "area" => $ospfd_conf['area']);
				}
				


 			}
		}
		

		/* OSPF Settings */

		$conffile .= "\n\nrouter ospf\n";

		// Specify router id
		if($ospfd_conf['routerid'])
			$conffile .= "  ospf router-id {$ospfd_conf['routerid']}\n";

		if ($ospfd_conf['updatefib'])
			$conffile .= "  area {$ospfd_conf['area']} stub\n";

		if ($ospfd_conf['logging'] && $ospfd_conf['adjacencylog'])
			$conffile .= "  log-adjacency-changes detail\n";

		if ($ospfd_conf['redistributeconnectedsubnets'])
			$conffile .= "  redistribute connected\n";

		if ($ospfd_conf['redistributestatic'])
			$conffile .= "  redistribute static\n";
		
		if ($ospfd_conf['redistributekernel'])
			$conffile .= "  redistribute kernel\n";
		

		if ($ospfd_conf['redistributedefaultroute'])
			$conffile .= "  default-information originate\n";

		if ($ospfd_conf['spfholdtime'] || $ospfd_conf['spfdelay']) {
			$spf_minhold = ($ospfd_conf['spfholdtime']) ? $ospfd_conf['spfholdtime'] : 1000;
			$spf_maxhold = $spf_minhold * 10;
			$spf_delay = ($ospfd_conf['spfdelay']) ? $ospfd_conf['spfdelay'] : 200;
			$conffile .= "  timers throttle spf {$spf_delay} {$spf_minhold} {$spf_maxhold}\n";
		}

		if ($ospfd_conf['rfc1583'])
			$conffile .= "  ospf rfc1583compatibility\n";

		if (is_array($passive_interfaces)) {
			foreach ($passive_interfaces as $pint)
				$conffile .= "  passive-interface {$pint}\n";
		}


		if (is_array($interface_networks)) {
			foreach ($interface_networks as $ifn) {
				if (is_subnet($ifn['subnet'])) {
						$conffile .= "  network {$ifn['subnet']} area {$ifn['area']}\n";
					}
			}
		}

		if (is_array($ospfd_conf['row'])) {
			foreach ($ospfd_conf['row'] as $redistr) {
				if (empty($redistr['routevalue']))
					continue;
				if (isset($redistr['redistribute'])) {
					$conffile .= "  no ";
				}
				if ($redistr['routearea'] == "") {
					$conffile .= "  network {$redistr['routevalue']} area {$ospfd_conf['area']}\n";	
				}
				else {
					$conffile .= "  network {$redistr['routevalue']} area {$redistr['routearea']}\n";
				}
			}
		}
	}
	safe_mkdir($quagga_config_base);
	$fd = fopen("{$quagga_config_base}/ospfd.conf", "w");

	// Write out the configuration file
	fwrite($fd, $conffile);

	// Close file handle
	fclose($fd);

	/* Make zebra config */
	$zebraconffile = "# This file was created by the pfSense package manager.  Do not edit!\n\n";
	if($ospfd_conf['password'])
		$zebraconffile .= "password {$ospfd_conf['password']}\n";
	if ($ospfd_conf['logging'])
		$zebraconffile .= "log syslog\n";
	$fd = fopen("{$quagga_config_base}/zebra.conf", "w");
	fwrite($fd, $zebraconffile);
	fclose($fd);

	// Create rc.d file
	$rc_file_stop = <<<EOF
if [ -e /var/run/quagga/zebra.pid ]; then
	kill -9 `cat /var/run/quagga/zebra.pid`
	rm -f /var/run/quagga/zebra.pid
fi
if [ -e /var/run/quagga/ospfd.pid ]; then
	kill -9 `cat /var/run/quagga/ospfd.pid`
	rm -f /var/run/quagga/ospfd.pid
fi
EOF;
	$rc_file_start = <<<EOF
/bin/mkdir -p /var/run/quagga
/bin/mkdir -p /var/log/quagga
rm -f /var/run/quagga/zebra.pid
rm -f /var/run/quagga/ospfd.pid

if [ `pw groupshow {$pkg_group} 2>&1 | grep -c "pw: unknown group"` -gt 0 ]; then
	/usr/sbin/pw groupadd {$pkg_group} -g {$pkg_gid}
fi
if [ `pw usershow {$pkg_login} 2>&1 | grep -c "pw: no such user"` -gt 0 ]; then
	/usr/sbin/pw useradd {$pkg_login} -u {$pkg_uid} -g {$pkg_gid} -c "{$pkg_gecos}" -d {$pkg_homedir} -s {$pkg_shell}
fi

/usr/sbin/chown -R quagga:quagga {$quagga_config_base}/
/usr/sbin/chown -R quagga:quagga /var/run/quagga
/usr/sbin/chown -R quagga:quagga /var/log/quagga
# Ensure no other copies of the daemons are running or it breaks.
killall -9 zebra 2>/dev/null
killall -9 ospfd 2>/dev/null
sleep 1
/usr/local/sbin/zebra -d -f {$quagga_config_base}/zebra.conf
/usr/local/sbin/ospfd -d -f {$quagga_config_base}/ospfd.conf
EOF;
	write_rcfile(array(
			"file" => "quagga.sh",
			"start" => $rc_file_start,
			"stop" =>  $rc_file_stop
		)
	);

	// Ensure files have correct permissions
	exec("chmod a+rx /usr/local/etc/rc.d/quagga.sh");
	exec("chmod u+rw,go-rw {$quagga_config_base}/ospfd.conf");
	exec("chmod u+rw,go-rw {$quagga_config_base}/zebra.conf");

	// Kick off newly created rc.d script
	exec("/usr/local/etc/rc.d/quagga.sh restart");

	// Back to RO mount for NanoBSD and friends
	conf_mount_ro();
}

function quagga_ospfd_validate_interface() {
	global $config, $g, $id, $input_errors;

	if ($config['installedpackages']['quaggaospfdinterfaces']['config']) {
		foreach ($config['installedpackages']['quaggaospfdinterfaces']['config'] as $index => $conf) {
			if ($index == 0)
				continue;
			if ($id != $index && $conf['interface'] == $_POST['interface'])
				$input_errors[] = "Interface {$_POST['interface']} is already configured.";
		}
	}
	if ($_POST['md5password'] && empty($_POST['password']))
		$input_errors[] = "Please input a password.";
}

function quagga_ospfd_validate_input() {
	global $config, $g, $input_errors;

	if (!empty($_POST['routerid']) && !is_ipaddr($_POST['routerid']))
		$input_errors[] = "Router ID must be an address.";
	if (!is_ipaddr($_POST['area']))
		$input_errors[] = "Area needs to be a valid ip_address.";
	if ($_POST['spfholdtime'] <> "" && ($_POST['spfholdtime'] < 1 || $_POST['spfholdtime'] > 5))
		$input_errors[] = "SPF holdtime needs to be between 1 and 5.";
	if ($_POST['spfdelay'] <> "" && ($_POST['spfdelay'] < 1 || $_POST['spfdelay'] > 10))
		$input_errors[] = "SPF delay needs to be between 1 and 10.";
	if (!$config['installedpackages']['quaggaospfdinterfaces']['config'])
		$input_errors[] = "Please select an interface to use for Quagga OSPFd.";
}

// get the raw ospfd confi file for manual inspection/editing
function quagga_ospfd_get_raw_config() {
	return file_get_contents("{$quagga_config_base}/ospfd.conf");
}

// serialize the raw ospfd confi file to config.xml
function quagga_ospfd_put_raw_config($conffile) {
	global $config;
	if ($conffile == "")
		unset($config['installedpackages']['quaggaospfd']['rawconfig']);
	else {
		$config['installedpackages']['quaggaospfd']['rawconfig'] = array();
		$config['installedpackages']['quaggaospfd']['rawconfig']['item'] = explode("\n",$_POST['quagga_ospfd_raw']);
		$config['installedpackages']['quaggaospfd']['rawconfig'] = $conffile;
	}
}

?>
