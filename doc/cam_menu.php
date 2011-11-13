<?php

	// No caching
	header("Cache-Control: no-cache, must-revalidate"); // HTTP/1.1
	header("Expires: Mon, 26 Jul 1997 05:00:00 GMT");   // Date du passée

	$port_server=0;
	if (isset($_GET['port_server']) && is_numeric($_GET['port_server'])) $port_server=intval($_GET['port_server']);
	$query=0;
	if (isset($_GET['query']) && is_numeric($_GET['query'])) $query=intval($_GET['query']);
	$key="";
	if (isset($_GET['key']) && strlen($_GET['key'])==1) $key=strval($_GET['key']);
	if ($port_server>0 && ($query==1 || ($query==2 && $key!="")))
	{
		// XML proxy reply
		header('Content-Type: application/xml; charset=UTF-8');
		if ($query==1)
			$url="http://localhost:".strval($port_server)."/cam/menu.xml";
		if ($query==2)
			$url="http://localhost:".strval($port_server)."/cam/action.xml?key=".$key;
		$cmdline="wget -T 1 -O - '".$url."'";
		passthru ($cmdline,$error);
		if (intval($error)!=0) echo("<?xml version=\"1.0\"?><error>wget error ".$error."</error>");
		die();
	}
	else
	{
		// HTML page
		header('Content-type: text/html; charset="utf-8"'); // HTML en UTF-8
	}
?>
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "-//W3C//DTD HTML 4.01 Transitional//EN">
<html>
	<head>
		<title>CAM Menu Management</title>
		<style>
			body {font-family:arial;color:black;background-color:white;}
			table#TabMenu {border-collapse:collapse;border:2px solid #7f7f7f; background-color: #f0f0f0;}
			table#TabMenu td {text-align:left;border:1px solid #7f7f7f;padding:2px;}
			table#TabMenu th {text-align:right;border:1px solid #7f7f7f;padding:2px;font-weight:bold;}
			table#TabKeys {border-collapse:collapse;border:2px solid #7f7f7f; background-color: #f0f0f0;}
			table#TabKeys td {text-align:center;padding:2px;border:1px solid #7f7f7f;}
			p.status {font-style:normal;}
			input.button {width:100px;background-color:#cccccc;font-weight:bold;}
			input.cancel {background-color:#c58888}
			input.ok {background-color:#89c588}
			input.menu {background-color:#889fc5}
		</style>
		<script language="javascript">
			// Get XML response for displaying the menu content
			function ResponseMenu()
			{
				var oStatusMenu=document.getElementById('StatusMenu');
				document.getElementById('RefreshMenu').innerHTML="";
				if(this.readyState==4) 
					if (this.status==200)
					{
						var Response=this.responseXML;
						if (Response)
						{
							if (Response.getElementsByTagName('menu').length==0)
								oStatusMenu.innerHTML="No valid data received";
							else
							{
								var HtmlContent="";
								HtmlContent+="<table id=\"TabMenu\">";
								if (Response.getElementsByTagName('datetime').length==1)
									HtmlContent+="<tr><th>Datetime: </th><td>"+Response.getElementsByTagName('datetime')[0].firstChild.data+"</td></tr>";
								if (Response.getElementsByTagName('cammenustring').length==1)
									HtmlContent+="<tr><th>CAM: </th><td>"+Response.getElementsByTagName('cammenustring')[0].firstChild.data+"</td></tr>";
								if (Response.getElementsByTagName('object').length==1)
									HtmlContent+="<tr><th>Object: </th><td>"+Response.getElementsByTagName('object')[0].firstChild.data+"</td></tr>";
								if (Response.getElementsByTagName('title').length==1)
									HtmlContent+="<tr><th>Title: </th><td>"+Response.getElementsByTagName('title')[0].firstChild.data+"</td></tr>";
								if (Response.getElementsByTagName('subtitle').length==1)
									HtmlContent+="<tr><th>Subtitle: </th><td>"+Response.getElementsByTagName('subtitle')[0].firstChild.data+"</td></tr>";
								for (i=0;i<Response.getElementsByTagName('item').length;i++)
									HtmlContent+="<tr><th>Item #"+Response.getElementsByTagName('item')[i].attributes[0].value+": </th><td>"+Response.getElementsByTagName('item')[i].firstChild.data+"</td></tr>";
								if (Response.getElementsByTagName('bottom').length==1)
									HtmlContent+="<tr><th>Bottom: </th><td>"+Response.getElementsByTagName('bottom')[0].firstChild.data+"</td></tr>";
								HtmlContent+="</table>";
								var oDisplayMenu=document.getElementById('DisplayMenu');
								oDisplayMenu.innerHTML=HtmlContent;
								oStatusMenu.innerHTML="Display OK";
							}
						}
						else
							oStatusMenu.innerHTML="Loaded with XML error";
					}
					else
						oStatusMenu.innerHTML="Loaded with HTTP error "+this.status;
			}
			// Auto refresh menu content
			function RefreshDisplay()
			{
				var oStatusMenu=document.getElementById('StatusMenu');
				try
				{
					var url="cam_menu.php?port_server="+document.getElementById('port_server').value+"&query=1";
					var client = new XMLHttpRequest();
					client.open("GET",url,true);
					client.onreadystatechange =  ResponseMenu;
					client.send(null); // Bug Firefox that needs argument for send
					document.getElementById('RefreshMenu').innerHTML="[Query]";
				}
				catch (err)
				{
					oStatusMenu.innerHTML="Error: "+err.message;
				}
				setTimeout ("RefreshDisplay()",2000);
			}
			// Response after sending a key
			function ResponseKey()
			{
				var StatusKey=document.getElementById('StatusKey');
				if(this.readyState==4) 
					if (this.status==200)
					{
						var Response=this.responseXML; 
						if (Response)
						{
							if (Response.getElementsByTagName('action').length==0)
								StatusKey.innerHTML="No valid data received";
							else
							{
								if (Response.getElementsByTagName('result').length==1)
									StatusKey.innerHTML=Response.getElementsByTagName('result')[0].firstChild.data;
								else
									StatusKey.innerHTML="No result code found";
							}
						}
						else
							StatusKey.innerHTML="Loaded with XML error";
					}
					else
						StatusKey.innerHTML="Loaded with HTTP error "+this.status;
			}
			// Send a action to the CAM
			function button(key)
			{
				var StatusKey=document.getElementById('StatusKey');
				try
				{
					var url="cam_menu.php?port_server="+document.getElementById('port_server').value+"&query=2&key="+key;
					var client = new XMLHttpRequest();
					client.open("GET",url,true);
					client.onreadystatechange =  ResponseKey;
					client.send(null); // Bug Firefox that needs argument for send
					StatusKey.innerHTML="Sending command...";
				}
				catch (err)
				{
					StatusKey.innerHTML="Error: "+err.message;
				}
			}
			// Launch auto refresh when the page is loaded
			window.onload="window.setTimeout (\"RefreshDisplay()\",2000)";
		</script>
	</head>
	<body onload="javascript:RefreshDisplay();">
		<h1>CAM Management</h1>
		<?php
			if ($port_server>0)
			
				echo('<p>Mumudvb HTTP port number : '.$port_server.'</p><input type="hidden" id="port_server" value="'.$port_server.'">');
			else
				echo('<p>Mumudvb HTTP port number : <input id="port_server" type="text" value="0"> (&gt;0)</p>');
		?>
		<h2>CAM Menu</h2>
		<p class="status">Status: <span id="StatusMenu">Not yet loaded</span> <span id="RefreshMenu"></span></p>
		<div id="DisplayMenu">No menu to display</div>
		<h2>CAM actions</h2>
		<p class="status">Status: <span id="StatusKey">No key sent</span></p>
		<table id="TabKeys">
			<tr><td><input type="button" class="button" value="1" onclick="javascript:button('1');"></td><td><input type="button" class="button" value="2" onclick="javascript:button('2');"></td><td><input type="button" class="button" value="3" onclick="javascript:button('3');"></td></tr>
			<tr><td><input type="button" class="button" value="4" onclick="javascript:button('4');"></td><td><input type="button" class="button" value="5" onclick="javascript:button('5');"></td><td><input type="button" class="button" value="6" onclick="javascript:button('6');"></td></tr>
			<tr><td><input type="button" class="button" value="7" onclick="javascript:button('7');"></td><td><input type="button" class="button" value="8" onclick="javascript:button('8');"></td><td><input type="button" class="button" value="9" onclick="javascript:button('9');"></td></tr>
			<tr><td><input type="button" class="button cancel" value="Cancel" onclick="javascript:button('C');"></td><td><input type="button" class="button" value="0" onclick="javascript:button('0');"></td><td><input type="button" class="button menu" value="Enter Menu" onclick="javascript:button('M');"></td></tr>
		</table>
	</body>
</html>
