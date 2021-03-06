<?php
/*
 snort_rules.php
 Copyright (C) 2004, 2005 Scott Ullrich
 Copyright (C) 2008, 2009 Robert Zelaya
 Copyright (C) 2011 Ermal Luci
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


require_once("guiconfig.inc");
require_once("/usr/local/pkg/snort/snort_gui.inc");
require_once("/usr/local/pkg/snort/snort.inc");

global $g;

if (!is_array($config['installedpackages']['snortglobal']['rule']))
	$config['installedpackages']['snortglobal']['rule'] = array();
$a_nat = &$config['installedpackages']['snortglobal']['rule'];

$id = $_GET['id'];
if (isset($_POST['id']))
	$id = $_POST['id'];
if (is_null($id)) {
        header("Location: /snort/snort_interfaces.php");
        exit;
}

if (isset($id) && $a_nat[$id]) {
	$pconfig['enable'] = $a_nat[$id]['enable'];
	$pconfig['interface'] = $a_nat[$id]['interface'];
	$pconfig['rulesets'] = $a_nat[$id]['rulesets'];
}

/* convert fake interfaces to real */
$if_real = snort_get_real_interface($pconfig['interface']);
$iface_uuid = $a_nat[$id]['uuid'];

/* Check if the rules dir is empy if so warn the user */
/* TODO give the user the option to delete the installed rules rules */
if (!is_dir("/usr/local/etc/snort/snort_{$iface_uuid}_{$if_real}/rules"))
	exec("/bin/mkdir -p /usr/local/etc/snort/snort_{$iface_uuid}_{$if_real}/rules");

$isrulesfolderempty = exec("ls -A /usr/local/etc/snort/snort_{$iface_uuid}_{$if_real}/rules/*.rules");
if ($isrulesfolderempty == "") {
	$isrulesfolderempty = exec("ls -A /usr/local/etc/snort/rules/*.rules");
	if ($isrulesfolderempty == "") {
		include_once("head.inc");
		include_once("fbegin.inc");

		echo "<body link=\"#000000\" vlink=\"#000000\" alink=\"#000000\">";

		if($pfsense_stable == 'yes'){echo '<p class="pgtitle">' . $pgtitle . '</p>';}

		echo "<table width=\"99%\" border=\"0\" cellpadding=\"0\" cellspacing=\"0\">\n
		   <tr>\n
			<td>\n";

		$tab_array = array();
		$tabid = 0;
		$tab_array[$tabid] = array(gettext("Snort Interfaces"), false, "/snort/snort_interfaces.php");
		$tabid++;
		$tab_array[$tabid] = array(gettext("If Settings"), false, "/snort/snort_interfaces_edit.php?id={$id}");
		$tabid++;
		$tab_array[$tabid] = array(gettext("Categories"), false, "/snort/snort_rulesets.php?id={$id}");
		$tabid++;
		$tab_array[$tabid] = array(gettext("Rules"), true, "/snort/snort_rules.php?id={$id}");
		$tabid++;
		$tab_array[$tabid] = array(gettext("Servers"), false, "/snort/snort_define_servers.php?id={$id}");
		$tabid++;
		$tab_array[$tabid] = array(gettext("Preprocessors"), false, "/snort/snort_preprocessors.php?id={$id}");
		$tabid++;
		$tab_array[$tabid] = array(gettext("Barnyard2"), false, "/snort/snort_barnyard.php?id={$id}");
		display_top_tabs($tab_array);
		echo  		"</td>\n
		  </tr>\n
		  <tr>\n
		    <td>\n
				<div id=\"mainarea\">\n
					<table id=\"maintable\" class=\"tabcont\" width=\"100%\" border=\"0\" cellpadding=\"0\" cellspacing=\"0\">\n
						<tr>\n
							<td>\n
		# The rules directory is empty.\n
						</td>\n
						</tr>\n
					</table>\n
				</div>\n
			</td>\n
		  </tr>\n
		</table>\n
		\n
		</form>\n
		\n
		<p>\n\n";

		echo "Please click on the Update Rules tab to install your selected rule sets.";
		include("fend.inc");

		echo "</body>";
		echo "</html>";

		exit(0);
	} else {
		/* Make sure that we have the rules */
		mwexec("/bin/cp /usr/local/etc/snort/rules/*.rules /usr/local/etc/snort/snort_{$iface_uuid}_{$if_real}/rules", true);
	}
}

function get_middle($source, $beginning, $ending, $init_pos) {
	$beginning_pos = strpos($source, $beginning, $init_pos);
	$middle_pos = $beginning_pos + strlen($beginning);
	$ending_pos = strpos($source, $ending, $beginning_pos);
	$middle = substr($source, $middle_pos, $ending_pos - $middle_pos);
	return $middle;
}

function write_rule_file($content_changed, $received_file)
{
	@file_put_contents($received_file, implode("\n", $content_changed));
}

function load_rule_file($incoming_file)
{
	//read file into string, and get filesize
	$contents = @file_get_contents($incoming_file);

	//split the contents of the string file into an array using the delimiter
	return explode("\n", $contents);
}

$ruledir = "/usr/local/etc/snort/snort_{$iface_uuid}_{$if_real}/rules/";
//$ruledir = "/usr/local/etc/snort/rules/";
$dh  = opendir($ruledir);
while (false !== ($filename = readdir($dh)))
{
	//only populate this array if its a rule file
	$isrulefile = strstr($filename, ".rules");
	if ($isrulefile !== false)
		$files[] = basename($filename);
}
sort($files);

if ($_GET['openruleset'])
	$rulefile = $_GET['openruleset'];
else
	$rulefile = $ruledir.$files[0];

//Load the rule file
$splitcontents = load_rule_file($rulefile);

if ($_GET['act'] == "toggle" && $_GET['ids']) {

	$lineid= $_GET['ids'];

	//copy rule contents from array into string
	$tempstring = $splitcontents[$lineid];

	//explode rule contents into an array, (delimiter is space)
	$rule_content = explode(' ', $tempstring);

	$findme = "# alert"; //find string for disabled alerts
	$disabled = strstr($tempstring, $findme);

	//if find alert is false, then rule is disabled
	if ($disabled !== false) {
		//rule has been enabled
		$tempstring = substr($tempstring, 2);
	} else
		$tempstring = "# ". $tempstring;

	//copy string into array for writing
	$splitcontents[$lineid] = $tempstring;

	//write the new .rules file
	write_rule_file($splitcontents, $rulefile);

	//write disable/enable sid to config.xml
	$sid = get_middle($tempstring, 'sid:', ';', 0);
	if (is_numeric($sid)) {
		// rule_sid_on registers
		if (!empty($a_nat[$id]['rule_sid_on']))
			$a_nat[$id]['rule_sid_on'] = str_replace("||enablesid $sid", "", $a_nat[$id]['rule_sid_on']);
		if (!empty($a_nat[$id]['rule_sid_on']))
			$a_nat[$id]['rule_sid_off'] = str_replace("||disablesid $sid", "", $a_nat[$id]['rule_sid_off']);
		if ($disabled === false)
			$a_nat[$id]['rule_sid_off'] = "||disablesid $sid_off" . $a_nat[$id]['rule_sid_off'];
		else
			$a_nat[$id]['rule_sid_on'] = "||enablesid $sid_on" . $a_nat[$id]['rule_sid_on'];
	}

	write_config();

	header("Location: /snort/snort_rules.php?id={$id}&openruleset={$rulefile}");
	exit;
}

$currentruleset = basename($rulefile);

$ifname = strtoupper($pconfig['interface']);

require_once("guiconfig.inc");
include_once("head.inc");

$pgtitle = "Snort: $id $iface_uuid $if_real Category: $currentruleset";
?>

<body link="#0000CC" vlink="#0000CC" alink="#0000CC">
<?php
include("fbegin.inc");
if ($pfsense_stable == 'yes'){echo '<p class="pgtitle">' . $pgtitle . '</p>';}

echo "{$snort_general_css}\n";
?>
<form action="snort_rules.php" method="post" name="iform" id="iform">

<script language="javascript" type="text/javascript">
function go()
{
    var box = document.iform.selectbox;
    destination = box.options[box.selectedIndex].value;
    if (destination) 
		location.href = destination;
}
function popup(url) 
{
 params  = 'width='+screen.width;
 params += ', height='+screen.height;
 params += ', top=0, left=0'
 params += ', fullscreen=yes';

 newwin=window.open(url,'windowname4', params);
 if (window.focus) {newwin.focus()}
 return false;
}
</script>

<table style="table-layout:fixed;" width="99%" border="0" cellpadding="0" cellspacing="0">
<tr><td>
<?php
        $tab_array = array();
        $tabid = 0;
        $tab_array[$tabid] = array(gettext("Snort Interfaces"), false, "/snort/snort_interfaces.php");
        $tabid++;
        $tab_array[$tabid] = array(gettext("If Settings"), false, "/snort/snort_interfaces_edit.php?id={$id}");
        $tabid++;
        $tab_array[$tabid] = array(gettext("Categories"), false, "/snort/snort_rulesets.php?id={$id}");
        $tabid++;
        $tab_array[$tabid] = array(gettext("Rules"), true, "/snort/snort_rules.php?id={$id}");
        $tabid++;
        $tab_array[$tabid] = array(gettext("Servers"), false, "/snort/snort_define_servers.php?id={$id}");
        $tabid++;
        $tab_array[$tabid] = array(gettext("Preprocessors"), false, "/snort/snort_preprocessors.php?id={$id}");
        $tabid++;
        $tab_array[$tabid] = array(gettext("Barnyard2"), false, "/snort/snort_barnyard.php?id={$id}");
        display_top_tabs($tab_array);
?>
</td></tr>
<tr>
	<td>
	<div id="mainarea2">
	<table id="maintable" class="tabcont" width="100%" border="0" cellpadding="0" cellspacing="0">
		<tr>
			<td class="listt" colspan="8">
			<br>Category: 
			<select id="selectbox" name="selectbox" class="formfld" onChange="go()">
			<?php
				foreach ($files as $value) {
					echo "<option value='?id={$id}&openruleset={$ruledir}{$value}' ";
					if ($value === $currentruleset)
						echo "selected";
					echo ">{$value}</option>\n";
				}
			?>
			</select>
			</td>
		</tr>
		<tr id="frheader">
			<td width="3%" class="list">&nbsp;</td>
			<td width="5%" class="listhdr">SID</td>
			<td width="6%" class="listhdrr">Proto</td>
			<td width="15%" class="listhdrr">Source</td>
			<td width="10%" class="listhdrr">Port</td>
			<td width="15%" class="listhdrr">Destination</td>
			<td width="10%" class="listhdrr">Port</td>
			<td width="32%" class="listhdrr">Message</td>
		</tr>
		<?php
			foreach ( $splitcontents as $counter => $value )
			{
				$disabled = "False";
				$comments = "False";
				$findme = "# alert"; //find string for disabled alerts
				$disabled_pos = strstr($value, $findme);

				$counter2 = 1;
				$sid = get_middle($value, 'sid:', ';', 0);
				//check to see if the sid is numberical
				if (!is_numeric($sid))
					continue;

				//if find alert is false, then rule is disabled
				if ($disabled_pos !== false){
					$counter2 = $counter2+1;
					$textss = "<span class=\"gray\">";
					$textse = "</span>";
					$iconb = "icon_block_d.gif";

					$ischecked = "";
				} else {
					$textss = $textse = "";
					$iconb = "icon_block.gif";

					$ischecked = "checked";
				}

				$rule_content = explode(' ', $value);

				$protocol = $rule_content[$counter2];//protocol location
				$counter2++;
				$source = substr($rule_content[$counter2], 0, 20) . "...";//source location
				$counter2++;
				$source_port = $rule_content[$counter2];//source port location
				$counter2 = $counter2+2;
				$destination = substr($rule_content[$counter2], 0, 20) . "...";//destination location
				$counter2++;
				$destination_port = $rule_content[$counter2];//destination port location

				if (strstr($value, 'msg: "'))
					$message = get_middle($value, 'msg: "', '";', 0);
				else if (strstr($value, 'msg:"'))
					$message = get_middle($value, 'msg:"', '";', 0);

                                       echo "<tr><td class=\"listt\"> $textss\n";
		?>
				<a href="?id=<?=$id;?>&openruleset=<?=$rulefile;?>&act=toggle&ids=<?=$counter;?>"><img
					src="../themes/<?= $g['theme']; ?>/images/icons/<?=$iconb;?>"
					width="10" height="10" border="0"
					title="click to toggle enabled/disabled status"></a>
				<!-- <input name="enable" type="checkbox" value="yes" <?= $ischecked; ?> onClick="enable_change(false)"> -->
				<!-- TODO: add checkbox and save so that that disabling is nicer -->
		<?php
				echo "$textse
                                       </td>
                                       <td width='5%' class=\"listlr\">
					$textss
					$sid
					$textse
                                       </td>
                                       <td width='6%' class=\"listlr\">
					$textss
					$protocol";
					echo "$textse
                                       </td>
                                       <td width='20%' class=\"listlr\">
					$textss
					$source
					$textse
                                       </td>
                                       <td width='5%' class=\"listlr\">
					$textss
					$source_port
					$textse
                                       </td>
                                       <td width='20%' class=\"listlr\">
					$textss
					$destination
					$textse
                                       </td>
                                       <td width='5%' class=\"listlr\">
                                       $textss
                                       $destination_port
                                       $textse
                                       </td>
				<td width='30%' class=\"listbg\"><font color=\"white\"> 
					$textss
					$message
					$textse
                                       </td>";
		?>
				<td valign="middle" nowrap class="list">
					<table border="0" cellspacing="0" cellpadding="1">
					<tr>
						<td><a href="javascript: void(0)"
							onclick="popup('snort_rules_edit.php?id=<?=$id;?>&openruleset=<?=$rulefile;?>&ids=<?=$counter;?>')"><img
							src="../themes/<?= $g['theme']; ?>/images/icons/icon_e.gif"
							title="edit rule" width="17" height="17" border="0"></a></td>
							<!-- Codes by Quackit.com -->
					</tr>
					</table>
				</td>
		<?php
			}
		?>
				
			</table>
			</td>
		</tr>
		<tr>
			<td class="listlr">
			<?php echo "  <strong><span class='red'>There are {$counter} rules in this category. <br/><br/></span></strong>"; ?>
			</td>
		</tr>
		<tr>
			<td>
			<table class="tabcont" width="100%" border="0" cellspacing="0" cellpadding="0">
				<tr>
					<td width="16"><img
						src="../themes/<?= $g['theme']; ?>/images/icons/icon_block.gif"
						width="11" height="11"></td>
					<td>Rule Enabled</td>
				</tr>
				<tr>
					<td><img
						src="../themes/<?= $g['theme']; ?>/images/icons/icon_block_d.gif"
						width="11" height="11"></td>
					<td nowrap>Rule Disabled</td>
				</tr>
				<tr>
						<!-- TODO: add save and cancel for checkbox options -->
						<!-- <td><pre><input name="Submit" type="submit" class="formbtn" value="Save">	<input type="button" class="formbtn" value="Cancel" onclick="history.back()"><pre></td> -->
				</tr>
				<tr>
					<td colspan="10">
					<p><!--<strong><span class="red">Warning:<br/> </span></strong>Editing these r</p>-->
					</td>
				</tr>
			</table>
			</td>
		</tr>
	</table>
	</td>
</tr>
</table>
</form>
<?php include("fend.inc"); ?>
</body>
</html>
