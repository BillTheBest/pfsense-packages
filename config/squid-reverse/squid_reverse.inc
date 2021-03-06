<?php
/* $Id$ */
/*
	squid_reverse.inc
	Copyright (C) 2012 Martin Fuchs	
	Copyright (C) 2012 Marcello Coutinho
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

function squid_resync_reverse() {
	global $config;
	//if(!is_array($valid_acls))
	//	return;

	//CONFIG FILE
	if (is_array($config['installedpackages']['squidreversegeneral']))
		$settings = $config['installedpackages']['squidreversegeneral']['config'][0];
	if (is_array($config['installedpackages']['squidreversepeer']))
		$reverse_peers=$config['installedpackages']['squidreversepeer']['config'];
	if (is_array($config['installedpackages']['squidreverseuri']))
		$reverse_maps=$config['installedpackages']['squidreverseuri']['config'];

	$conf = "# Reverse Proxy settings\n";

	if(isset($settings["reverse_ssl_cert"]) && $settings["reverse_ssl_cert"] != "none") {
    	$svr_cert = lookup_cert($settings["reverse_ssl_cert"]);
		if ($svr_cert != false) {
			if(base64_decode($svr_cert['crt'])) {
				file_put_contents(SQUID_CONFBASE . "/{$settings["reverse_ssl_cert"]}.crt",sq_text_area_decode($svr_cert['crt']));
				$reverse_crt = SQUID_CONFBASE . "/{$settings["reverse_ssl_cert"]}.crt";
				}
			if(base64_decode($svr_cert['prv'])) {
				file_put_contents(SQUID_CONFBASE . "/{$settings["reverse_ssl_cert"]}.key",sq_text_area_decode($svr_cert['prv']));
				$reverse_key = SQUID_CONFBASE . "/{$settings["reverse_ssl_cert"]}.key";
       			}
			}
		}
       
	if (!empty($settings['reverse_int_ca']))
		file_put_contents(SQUID_CONFBASE . "/{$settings["reverse_ssl_cert"]}.crt","\n" . sq_text_area_decode($settings['reverse_int_ca']),FILE_APPEND | LOCK_EX);

	$ifaces = ($settings['reverse_interface'] ? $settings['reverse_interface'] : 'wan');
	$real_ifaces = array();
	
	#set HTTP port and defsite
	$http_port=(empty($settings['reverse_http_port'])?"80":$settings['reverse_http_port']);
	$http_defsite=(empty($settings['reverse_http_defsite'])?$settings['reverse_external_fqdn']:$settings['reverse_http_defsite']);
	
	#set HTTPS port and defsite
	$https_port=(empty($settings['reverse_https_port'])?"80":$settings['reverse_https_port']);
	$https_defsite=(empty($settings['reverse_https_defsite'])?$settings['reverse_external_fqdn']:$settings['reverse_https_defsite']);
	
	foreach (explode(",", $ifaces) as $i => $iface) {
		$real_ifaces[] = squid_get_real_interface_address($iface);
		if($real_ifaces[$i][0]) {
		      //HTTP
		      if (!empty($settings['reverse_http']))
		      		$conf .= "http_port {$real_ifaces[$i][0]}:{$http_port} accel defaultsite={$http_defsite} vhost\n";
		      //HTTPS
		      if (!empty($settings['reverse_https']))
		      		$conf .= "https_port {$real_ifaces[$i][0]}:{$https_port} accel cert={$reverse_crt} key={$reverse_key} defaultsite={$https_defsite}\n";
			}
		}
		
	if(!empty($settings['reverse_ip'])) {
		$reverse_ip = explode(";", ($settings['reverse_ip']));
		foreach ($reverse_ip as $reip) {
		      //HTTP
		      if (!empty($settings['reverse_http']))
		      		$conf .= "http_port {$reip}:{$http_port} accel defaultsite={$http_defsite} vhost\n";
		      //HTTPS
		      if (!empty($settings['reverse_https']))
		      		$conf .= "https_port {$reip}:{$https_port} accel cert={$reverse_crt} key={$reverse_key} defaultsite={$https_defsite}\n";
			}
	 	}

	//PEERS
 	if (($settings['reverse_owa'] == 'on') && (!empty($settings['reverse_owa_ip'])))
 		$conf .= "cache_peer {$settings['reverse_owa_ip']} parent 443 0 proxy-only no-query originserver login=PASS connection-auth=on ssl sslflags=DONT_VERIFY_PEER front-end-https=on name=OWA_HOST_pfs\n";

 	$active_peers=array();
 	if (is_array($reverse_peers))
 	 foreach ($reverse_peers as $rp){
 		if ($rp['enable'] =="on" && $rp['name'] !="" && $rp['ip'] !="" && $rp['port'] !=""){
 			$conf_peer = "#{$rp['description']}\n";
 			$conf_peer .= "cache_peer {$rp['ip']} parent {$rp['port']} 0 proxy-only no-query originserver login=PASS ";
 			if($rp['protocol'] == 'HTTPS')
	            	$conf_peer .= "ssl sslflags=DONT_VERIFY_PEER front-end-https=auto ";
			$conf_peer .= "name={$rp['name']}\n\n";
			
			// add peer only if reverse proxy is enabled for http
			if($rp['protocol'] == 'HTTP' && $settings['reverse_http'] =="on"){
				$conf .= $conf_peer;
				array_push($active_peers,$rp['name']);
				}
			// add peer only if if reverse proxy is enabled for https
			if($rp['protocol'] == 'HTTPS' && $settings['reverse_https'] =="on"){
				$conf .= $conf_peer;
 				array_push($active_peers,$rp['name']);
				}
 			}
 	}
  	
	//ACLS and MAPPINGS

 	//create an empty owa_dirs to populate based on user selected options
	$owa_dirs=array();
	if (($settings['reverse_owa'] == 'on') && $settings['reverse_https'] =="on"){
		if(!empty($settings['reverse_owa_ip'])){
			array_push($owa_dirs,'owa','exchange','public','exchweb','ecp','OAB');
			if($settings['reverse_owa_activesync'])
					array_push($owa_dirs,'Microsoft-Server-ActiveSync');
			if($settngs['reverse_owa_rpchttp'])	
					array_push($owa_dirs,'rpc/rpcproxy.dll','rpcwithcert/rpcproxy.dll');
			if($settings['reverse_owa_autodiscover'])
					array_push($owa_dirs,'autodiscover');
			if($settings['reverse_owa_webservice']){
					array_push($owa_dirs,'EWS');
					$conf .= "ignore_expect_100 on\n";
					}
			}
		if (is_array($owa_dirs))
		  foreach ($owa_dirs as $owa_dir)
			$conf .= "acl OWA_URI_pfs url_regex -i ^https://{$settings['reverse_external_fqdn']}/$owa_dir.*$\n";
		}	
	//$conf .= "ssl_unclean_shutdown on";
	if (is_array($reverse_maps))
	 foreach ($reverse_maps as $rm){
		if ($rm['enable'] == "on" && $rm['name']!="" && $rm['peers']!=""){
		    if (is_array($rm['row']))
				foreach ($rm['row'] as $uri){
					$url_regex=($uri['vhost'] == ''?$settings['reverse_external_fqdn']:$uri['vhost']);
					$conf .= "acl {$rm['name']} url_regex -i {$url_regex}/{$uri['uri']}.*$\n";
					$cache_peer_never_direct_conf .= "never_direct allow {$rm['name']}\n";
					$http_access_conf .= "http_access allow {$rm['name']}\n";
					foreach (explode(',',$rm['peers']) as $map_peer)
						if (in_array($map_peer,$active_peers)){
							$cache_peer_allow_conf .= "cache_peer_access {$map_peer} allow {$rm['name']}\n";
							$cache_peer_deny_conf .= "cache_peer_access {$map_peer} deny allsrc\n";
						}
				}
	 	}
	}

	//ACCESS
  	if ($settings['reverse_owa'] == 'on' && !empty($settings['reverse_owa_ip']) && $settings['reverse_https'] =="on") {
		$conf .= "cache_peer_access OWA_HOST_pfs allow OWA_URI_pfs\n";
		$conf .= "cache_peer_access OWA_HOST_pfs deny allsrc\n";
		$conf .= "never_direct allow OWA_URI_pfs\n";
		$conf .= "http_access allow OWA_URI_pfs\n";        
		}

	$conf .= $cache_peer_allow_conf.$cache_peer_deny_conf.$cache_peer_never_direct_conf.$http_access_conf."\n";

	if (!empty($settings['deny_info_tcp_reset']))
		$conf .= "deny_info TCP_RESET allsrc\n";                                                                                          

	return $conf;
}
?>
