HTTPD Change Log May 2024 - version 3.0.0

	Major release due to Lua integration with the server internals
	as well as CGI processing.

	Gone are the days of multiple PARM='...' settings for the 
	HTTPD server. All configuration settings are now Lua script
	processed at startup via PARM='CONFIG=your.lua.config.dataset'.
	If the server configuration defaults are acceptable, you don't 
	event have to supply a config dataset and can omit the 
	PARM='...' from	the HTTPD EXEC card in your JCL/PROC.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Add LUA CGI support.

	The HTTPD server now supports running Lua scripts to process
	client request.

	Whenever the HTTPD server receives a request in the form:
	'/lua/name?var1=value&var2=value&...' where name is the
	Lua script name and "var..." are the query variables from 
	the URL the client requested.

	The Lua script will receive a 'http' table as a global variable
	that provides data about the request as well as a print 
	function via 'http.print' that sends string data to the
	client of this request.

	Most CGI Lua scripts would use something like this at the start
	of each CGI script:
	  -- Use http.print function if it exist
	  if http.print then                    
	     http.oprint = print                
	     print = http.print                 
	  end                                   

	Example: http table -- for n in pairs(http) do print('  http.'..n) end
	  http.print
	  http.vars
	  http.server_version

	Example: http.vars table -- for n,v in pairs(http.vars) do print('  '..n..'='..v) end
	  HTTP_Connection=keep-alive
	  REQUEST_METHOD=GET
	  HTTP_REQUEST=GET /lua/test HTTP/1.1
	  SERVER_ADDR=192.168.1.25
	  SERVER_SOFTWARE=HTTPD/3.0.0
	  HTTP_User_Agent=Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36
	  HTTP_sec_gpc=1
	  REQUEST_VERSION=HTTP/1.1
	  HTTP_Accept=text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7
	  SERVER_PROTOCOL=HTTP/1.0
	  SERVER_PORT=1080
	  REQUEST_URI=/lua/test
	  HTTP_Accept_Language=en-US,en;q=0.9
	  REQUEST_PATH=/lua/test
	  HTTP_Cookie_Sec_Token=7g6zABAi8D9EMxdtjNqse2Y2jOr/ps7AKRjyEn2H9/w=
	  HTTP_Accept_Encoding=gzip, deflate
	  HTTP_Upgrade_Insecure_Requests=1
	  HTTP_DNT=1
	  HTTP_Host=tk5.rayborn.us:1080

	The Lua language is case sensitive. 
	Example: http.vars.REQUEST_METHOD is the string "GET"
			 http.vars.request_method is nil.

	CGI Request are loaded from the CGILUA DD via:
	"DD:CGILUA(?)" where the ? is replaced by the script name.

	Likewise any "require'name'" members are loaded
	from "DD:CGILUA(?);DD:LUALIB(?)" where ? is the
	'name' from the require statement.

	Suggested JCL/PROC changes for HTTPD with Lua:
	//* LUA Server Config                         
	//HTTPDLUA DD DSN=SYS2.HTTPD.HTTPDLUA,DISP=SHR
	//* LUA CGI scripts                           
	//CGILUA   DD DSN=SYS2.HTTPD.CGILUA,DISP=SHR  
	//* LUA CGI "require'name'"                   
	//LUALIB   DD DSN=SYS2.HTTPD.LUALIB,DISP=SHR  

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Add client statistics (server performance metrics).

	The HTTPD server has always had detailed client statistics
	written to the dataset/sysout allocated to the HTTPSTAT DD.

	However, what we didn't have until now is client statistics
	that showed server performance metrics related to servicing
	client request.

	If a statistics dataset is configured then statistics data
	will be read at server startup from that dataset.
	Likewise, if a statistics dataset is configured then the
	statistics array will be saved to that dataset at server
	shutdown.

	New modify commands for DISPLAY STATS and SET STATS.

	The statistics arrays are divided into one for each area.
	One array for Minutes, one for Hours, one for Days, one for Months.
	Each statistics array can be configured for maximum number of
	records allowed in that array (1 to 255).

	/F HTTPD,DISPLAY STATS nn [MInute|Hour|Day|Month|All]

	Displays statistics for the respective array (All being an exception)
	where the nn number indicates how many of those Minutes, Hours, Days
	or Months should be displayed.

	The ALL option (instead of Minutes, Hours, Days, Months) will
	display statistics from all four arrays using the nn value to
	represent the minutes (for Minutes array) and scaled to the
	number of Hours, Days and Months with a minimum of 1 for the
	Hours, Days and Months values. Handy for testing.

	/F HTTPD,SET DISPLAY ON|OFF [Save|Clear|Free|Reset]

	The gathering of client statistics can be turned ON or OFF.
	If the SAVE option is specified, the statistics arrays are
	written to the statistics dataset if one is configured.
	If the CLEAR, FREE or RESET option is specified then the
	statistics arrays as cleared releasing that memory.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
HTTPD Change Log May 2024 - version 2.1.0

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Add BREXX CGI support for Moshe.

	The HTTPD server now supports running BREXX scripts to
	process client request.

	Whenever the HTTPD server receives a request in the form:
	'/rexx/name?var1=value&var2=value' the 'name' will be passed
	to BREXX running as a CGI program in the HTTPD address space.

	BREXX will receive a parameter line like:
	'name "var1=value&var2=value&..."' where 'name' is the
	script name and "var..." are the query variables from the
	URL the client requested. This parameter line is limited 
	to 512 bytes maximum. For simple request this may be all
	the data the script needs. For more robust request the script
	should process and use the data in the HTTPVARS DD.

	A temp dataset is allocated to HTTPVARS DD and contains
	all of the server variables for this request and looks like:

	HTTP_REQUEST="GET /rexx/test?this=test&that=stuff%20this%20echo HTTP/1.1"
	REQUEST_METHOD="GET"
	REQUEST_URI="/rexx/test?this=test&that=stuff%20this%20echo"
	REQUEST_VERSION="HTTP/1.1"
	HTTP_Host="tk5.rayborn.us:1080"
	HTTP_Connection="keep-alive"
	HTTP_Cache_Control="max-age=0"
	HTTP_DNT="1"
	HTTP_Upgrade_Insecure_Requests="1"
	HTTP_User_Agent="Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/123.0.0.0 Safari/537.36"
	HTTP_Accept="text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7"
	HTTP_Accept_Encoding="gzip, deflate"
	HTTP_Accept_Language="en-US,en;q=0.9"
	HTTP_Cookie_Sec_Token="oEJPLD+p4e5ar7K17jXe9Gx0ydN3nU1NdVzNNHyO1SE="
	HTTP_Cookie_Sec_Uri="YZmFp6dho4Wiow=="
	HTTP_sec_gpc="1"
	QUERY_STRING="this=test&that=stuff%20this%20echo"
	QUERY_this="test"
	QUERY_that="stuff this echo"
	REQUEST_PATH="/rexx/test"
	SERVER_ADDR="192.168.1.25"
	SERVER_PORT="1080"
	SERVER_PROTOCOL="HTTP/1.0"
	SERVER_SOFTWARE="HTTPD/2.1.0"

	Your script can use CALL READALL(HTTPVARS, VARS.) to read all
	variables into the STEM variable 'VARS.' as a quick test. 

	Note: The QUERY_STRING and POST_STRING values are the raw data
	we received from the HTTPD client. These values have escaped
	sequences in them that represent ASCII characters like %20 
	(ASCII SPACE). You would need to transform those escape
	values AFTER you parse the seperators '&' into variable=value
	strings and then decode the escape sequences.

	The other QUERY_xxx and POST_xxx entries in the HTTPVARS file
	are the parsed and decoded variable=value pairs which should
	be easier to use in your scripts.

	BREXX sample script code to read HTTPVARS DD and create global
	variables from the values read:

		/* read all of the server variables into queued buffer */
		call makebuf
		"EXECIO * DISKR HTTPVARS (FIFO "
		/* pull lines from the queue and convert them to global variables */
		do i=1 to queued()
		  parse pull name '=' value
		  /* create global variables */
		  call setg(name,value)
		end
		call desbuf

	BREXX sample to get the REQUEST_PATH variable value:

		SAY '----------------------------------------------------------------'
		SAY 'REQUEST_PATH:' getg('REQUEST_PATH')
		SAY '----------------------------------------------------------------'

	The BREXX scripts can use the SAY verb to send output back to the 
	HTTPD client via the temp dataset allocated to the SYDOUT DD. 

	The BREXX scripts can also use the HTTPSAY program via:

		ADDRESS LINKMVS "HTTPSAY var1 .. var15"

	The HTTPSAY program sends output directly to the HTTPD client without
	using the STDOUT DD that SAY uses. 

	Note: If your script uses both HTTPSAY and the SAY commands you will 
	get all the output from HTTPSAY sent to the HTTPD client first, and 
	then when your script terminates the SAY output will be sent to the 
	HTTPD client.

	BREXX HTTPSAY sample code:

		msg1 = "This is a test message, this is only a test"
		address linkmvs "HTTPSAY msg1"

	Sample HTTPSAY script:

		/* BREXX - HTTPSAY */
		parse arg line
		address linkmvs "HTTPSAY line"

	Sample use of the HTTPSAY sample script:

		call HTTPSAY 'Testing the HTTPSAY script'

	Keep in mind that using HTTPSAY has the overhead of being a LINK 
	program in which MVS has to LINK (LOAD/CALL/DELETE) the HTTPSAY 
	module for each request. But unlike SAY, HTTPSAY output will be 
	sent to the HTTPD client immediately and avoids the SAY write, 
	HTTPD read, HTTPD send cycle.

	The temp output datasets (STDOUT and STDERR) are allocated as 
	RECFM=VB,LRECL=255,BLKSIZE=27998,SPACE=(CYL,(15,15)) which is 
	about ~180 MB of space for your scripts output.

	When your BREXX script (and BREXX itself) ends the HTTPD server 
	reads the temp dataset allocated to STDOUT DD and returns the 
	SAY output to the HTTPD client. 

	Any errors (or trace output) issued by BREXX to the temporary
	dataset allocated to the STDERR DD name will be written to
	the system console via WTO messages and look like:

		HTTPD500I CGI Program BREXX script "badname"
		HTTPD501I Error 58 running "BADNAME": File not found

	The HTTPD JCL must be modified to include the script PDS 
	datasets used by BREXX.	BREXX will attempt to locate the 
	requested script name using the datasets allocated to 
	SYSUEXEC, SYSUPROC, SYSEXEC or SYSPROC DD names. 
	BREXX will search for the script in the order listed.
	https://brexx370.readthedocs.io/en/latest/installation.html#brexx-usage

	BREXX also has a rexx library that should be allocated to
	DD RXLIB. See: https://brexx370.readthedocs.io/en/latest/rxlib.html

	Sample HTTPD JCL changes:

		//SYSEXEC  DD DSN=xxxx.HTTPD.BREXX,DISP=SHR  <== your script dataset
		//         DD DSN=BREXX.V2R5M3.SAMPLES,DISP=SHR
		//RXLIB    DD DSN=BREXX.V2R5M3.RXLIB,DISP=SHR
		//

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

HTTPD Change Log April 2024 - version 2.0.0

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Code changes for 64 bit time values.

	The HTTPD server code now uses 64 bit time values
	as implemented in the new CRENT370/time64 code.

	See CRENT370/include/time64.h for details.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Code changes for Unix "like" File System (UFS).

	The HTTPD server code and the FTPD server code (embedded in
	HTTPD server), make use of improvements to the UFS code
	base.

	The UFS code now supports mounted disk (datasets formatted 
	as a UFS disk). The mounting occurs at HTTPD startup when 
	the UFS system is initialized. The UFS system will attempt 
	to mount disk by reading the '/etc/fstab' file in the root 
	disk (file system root) from the UFSDISK0 dataset.

	The /etc/fstab file may contain:
		# comment lines that start with a '#' character
		ddname=UFSDISK[1-9]		/some/path/name 	[ufs]

	The path name specified must be a directory name that
	already exist. When a disk is mounted on a directory the
	previous contents of that directory (if any) are no longer
	accessable while the disk is mounted on that directory.

	Note: At UFS startup, up to 10 datasets are opened by UFS 
	with dd names UFSDISK0 through UFSDISK9. The dataset on 
	UFSDISK0 is always mounted as the file system root '/' 
	path name.  The other UFSDISK[1-9] are mounted only if
	specified in the '/etc/fstab' file.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Server Side Includes (SSI) processing.

	The HTTPD server now supports Server Side Include (SSI) 
	file processing for HTML files.

	The following SSI directive are supported in HTML files:
		<!--#echo var="var_name" -->
		<!--#include file="path_name" -->
		<!--#printenv -->

	Whenever any of the above are incountered in a HTML file
	they will be replaced with the appropriate variable, file
	or list of all variables by the HTTP server and returned
	as instream data.

	Example HTML file inctest.html:
		<html>
		 <body>
		 This is inctest.html
		 <!--#include file="/inctest1.html" -->
		 <!--#printenv -->
		 </body>
		</html>

	You can have multiple HTML elements on a single line:
		<h1>Welcome <!--#echo var="REQUEST_PATH" --></h1>
	Which would resolve to something like:
		<h1>Welcome /some/path/name</h1>

	Notes:
	1) all SSI directives are HTML comments.
	2) all SSI directives start with "<!--#" followed by the request.
	3) all SSI directives must end with " -->", the space is required.
	4) You can SSI "include" files up to 10 levels deep.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Code changes to ___try() and __try() functions (CRENT370).

	The ___try() function has been enhanced to capture the
	ESTAE ABEND code and save it in the CRT (C Runtime Table).

	The ___try() return value is 0 on success, otherwise ABEND
	code if rc > 0 and rc <= 0x00FFFFFF. 
	The rc value for ABENDS are as follows:
		00...... not used, always 0.
		..SSS... The system ABEND code. ((rc >> 12) & 0xFFF)
		.....UUU the user ABEND code.	(rc & 0xFFF)
	
	A negative rc value indicates ESTAE CREATE failure.
	Note: try() is a C macro for the ___try() function.

	The __try() function has been enhanced to capture only the
	return code (no ABEND code) in the CRT (C Runtime Table).

	The __try() return value is 0 on success, otherwise a negative
	rc value indicates ESTAE CREATE failure.

	New functions ___tryrc() and __tryrc() have been created
	to return the last ___try() or __try() return code from
	the CRT.
	Note: tryrc() is a C macro for the ___tryrc() function.

	See CRENT370/include/clibtry.h and 
	CRENT370/include/clibstae.h for details.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Add "display memory" to the HTTPD server.

	A new "display memory" console command has been added to 
	the HTTPD server. 

	You can now display memory in the HTTPD address space
	using the modify command: /F HTTPD,D M xxxxxx[,nnn]
	which will display address xxxxxx with a default length
	of 256 bytes.

	Note: The memory display is protected via an ESTAE to prevent
	abend of the address space for invalid memory address access.

	Example: /F HTTPD,D M a89f0
	/ 8.37.30 JOB 4895  HTTPD100I CONS(3) "D M A89F0"                                                                      
	/ 8.37.30 JOB 4895  Dump of 000A89F0 "MEMORY" (256 bytes)                                                              
	/ 8.37.30 JOB 4895  +00000 47F0F012 0DA69699 9285996D A3889985 :.00..worker_thre:                                      
	/ 8.37.30 JOB 4895  +00010 818490EC D00C41C0 F00058F0 D04C50D0 :ad..}..{0..0}<&}:                                      
	/ 8.37.30 JOB 4895  +00020 F00450F0 D00818DF 41F0F080 50FD004C :0.&0}....00.&..<:                                      
	/ 8.37.30 JOB 4895  +00030 47F0C038 00000000 05C018B1 58A0C29E :.0{......{....B.:                                      
	/ 8.37.30 JOB 4895  +00040 585B0004 4110D058 58F0C2A2 05EF183F :.$....}..0Bs....:                                      
	/ 8.37.30 JOB 4895  +00050 4110D058 58F0C2A6 05EF1F22 5882021C :..}..0Bw.....b..:                                      
	/ 8.37.30 JOB 4895  +00060 18421233 4780C04E 58430010 47F0C04E :......{+.....0{+:                                      
	/ 8.37.30 JOB 4895  +00070 D203D058 C2AA506D 005C4110 D05858F0 :K.}.B.&_.*..}..0:                                      
	/ 8.37.30 JOB 4895  +00080 C2AE05EF 47F0C256 4110D058 58F0C2B2 :B....0B...}..0B.:                                      
	/ 8.37.30 JOB 4895  +00090 05EF186F 5875000C 1FFF50FD 007850FD :...?......&...&.:                                      
	/ 8.37.30 JOB 4895  +000A0 007C5820 C2B65822 0000D203 D058C2BA :.@..B.....K.}.B.:                                      
	/ 8.37.30 JOB 4895  +000B0 582200B4 4110D058 41F20000 05EF1244 :......}..2......:                                      
	/ 8.37.30 JOB 4895  +000C0 4770C096 4110D058 58F0C2BE 05EF184F :..{o..}..0B.... :                                      
	/ 8.37.30 JOB 4895  +000D0 D203D058 C2C2505D 005C508D 0060504D :K.}.BB&).*&..-&(:                                      
	/ 8.37.30 JOB 4895  +000E0 0064506D 0068507D 006C503D 00704110 :..&_..&'.%&.....:                                      
	/ 8.37.30 JOB 4895  +000F0 D05858F0 C2AE05EF D203D058 C2C6D203 :}..0B...K.}.BFK.:                                      
	/ 8.37.30 JOB 4895  End of memory dump for 0x0A89F0 

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Add "display version" to the HTTPD server.

	A new "display version" console command has been added to
	the HTTPD server.

	Example: /F HTTPD,D V
	/14.03.51 JOB 4910  HTTPD100I CONS(3) "D V"
	/14.03.51 JOB 4910  HTTPD140I HTTPD Server version 2.0.0

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Add "set maxtask n" and "set mintask n" commands to HTTPD server.

	New "set maxtask n" and "set mintask n" commands have been
	added to the HTTPD server.

	The "set maxtask n" command sets the maximum number of
	worker task (threads) that the server can create to service
	HTTP and FTP reqwuest. The valid range of 'n' is 1 to 100.

	The "set mintask n" command sets the minimum number of
	worker task (threads) that the server will start prior
	to any HTTP or FTP request is processed. The valid range
	of 'n' is 1 to 100.

	Both commands adjust the MINTASK and MAXTASK values to insure that
	the minimum is always less than or equal to the maximum task.

	Worker threads are then started or shut down as needed by the dispatcher.

	Example: /F HTTPD,set min 9
	/ 9.10.03 JOB 4941  HTTPD100I CONS(3) "SET MIN 9"
	/ 9.10.03 JOB 4941  HTTPD107I .....EYE CTHDMGR   ....TASK 00165FC8  .WAITECB 40000000
	/ 9.10.03 JOB 4941  HTTPD108I ....FUNC 000A8228  ...UDATA 000E56F8  STACKSZE 00010000
	/ 9.10.03 JOB 4941  HTTPD109I ..WORKER 00145784  ...QUEUE 00156DD4  ...STATE 00000004 WAITING
	/ 9.10.03 JOB 4941  HTTPD110I .MINTASK 00000009  .MAXTASK 00000009
	/ 9.10.03 JOB 4941  HTTPD111I .WORKERS        3  .DISPCNT 00000010 (16)
	/ 9.10.03 JOB 4941  HTTPD199I -------------------------------------------------------
	/ 9.10.03 JOB 4941  HTTPD061I STARTING worker(1450A0) TCB(98CC18) ACEE(9BCB98) TASK(1D7FC8) MGR(145D48) CRT(156E48)
	/ 9.10.03 JOB 4941  HTTPD061I STARTING worker(15AEC8) TCB(98C9C8) ACEE(9BCB98) TASK(1E8FC8) MGR(145D48) CRT(15A088)
	/ 9.10.03 JOB 4941  HTTPD061I STARTING worker(15AD48) TCB(98C6F0) ACEE(9BCB98) TASK(1F9FC8) MGR(145D48) CRT(15A308)
	/ 9.10.03 JOB 4941  HTTPD061I STARTING worker(15A288) TCB(98DD50) ACEE(9BCB98) TASK(20AFC8) MGR(145D48) CRT(1561C8)
	/ 9.10.03 JOB 4941  HTTPD061I STARTING worker(15AC38) TCB(98DA78) ACEE(9BCB98) TASK(21BFC8) MGR(145D48) CRT(15A9C8)
	/ 9.10.03 JOB 4941  HTTPD061I STARTING worker(156100) TCB(98D7A0) ACEE(9BCB98) TASK(22CFC8) MGR(145D48) CRT(15A848)

	Example: /F HTTPD,set max 3
	/ 9.12.52 JOB 4941  HTTPD100I CONS(3) "SET MAX 3"
	/ 9.12.52 JOB 4941  HTTPD107I .....EYE CTHDMGR   ....TASK 00165FC8  .WAITECB 40000000
	/ 9.12.52 JOB 4941  HTTPD108I ....FUNC 000A8228  ...UDATA 000E56F8  STACKSZE 00010000
	/ 9.12.52 JOB 4941  HTTPD109I ..WORKER 00145784  ...QUEUE 00156DD4  ...STATE 00000004 WAITING
	/ 9.12.52 JOB 4941  HTTPD110I .MINTASK 00000003  .MAXTASK 00000003
	/ 9.12.52 JOB 4941  HTTPD111I .WORKERS        9  .DISPCNT 00000010 (16)
	/ 9.12.52 JOB 4941  HTTPD199I -------------------------------------------------------
	/ 9.12.52 JOB 4941  HTTPD060I SHUTDOWN worker(156100) TCB(98D7A0) ACEE(9BCB98) TASK(22CFC8) MGR(145D48) CRT(15A848)
	/ 9.12.53 JOB 4941  HTTPD060I SHUTDOWN worker(15AC38) TCB(98DA78) ACEE(9BCB98) TASK(21BFC8) MGR(145D48) CRT(15A9C8)
	/ 9.12.53 JOB 4941  HTTPD060I SHUTDOWN worker(15A288) TCB(98DD50) ACEE(9BCB98) TASK(20AFC8) MGR(145D48) CRT(1561C8)
	/ 9.12.53 JOB 4941  HTTPD060I SHUTDOWN worker(15AD48) TCB(98C6F0) ACEE(9BCB98) TASK(1F9FC8) MGR(145D48) CRT(15A308)
	/ 9.12.53 JOB 4941  HTTPD060I SHUTDOWN worker(15AEC8) TCB(98C9C8) ACEE(9BCB98) TASK(1E8FC8) MGR(145D48) CRT(15A088)
	/ 9.12.53 JOB 4941  HTTPD060I SHUTDOWN worker(1450A0) TCB(98CC18) ACEE(9BCB98) TASK(1D7FC8) MGR(145D48) CRT(156E48)

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Add "display time" command to HTTPD server.

	A new "display time [-|+][minutes]" command has been added to 
	the HTTPD server.

	For those of us in the USA we have a thing called Daylight 
	Savings Time that we have to adjust for each year. This means
	we have to adjust the system time zone for MVS and the
	TZOFFSET value used by the HTTPD server.

	This new "display time" command provides us with a tool to
	validate the correct TZOFFSET value for the HTTPD server.

	Example: /F HTTPD,d ti
	/14.49.55 JOB 5072  HTTPD100I CONS(3) "D TI"
	/14.49.55 JOB 5072  HTTPD142I time 2024/03/15 19:49:54 GMT
	/14.49.55 JOB 5072  HTTPD143I time 2024/03/15 13:49:54 Local TZOFFSET=-360

	The example above shows that the current TZOFFSET value for
	our HTTPD server is incorrect.

	We can supply a value we want to try as the TZOFFSET with 
	the "display time" command and check the result.

	Example: /F HTTPD,d ti -300
	/14.50.11 JOB 5072  HTTPD100I CONS(3) "D TI -300"
	/14.50.11 JOB 5072  HTTPD142I time 2024/03/15 19:50:11 GMT
	/14.50.11 JOB 5072  HTTPD143I time 2024/03/15 14:50:11 Local TZOFFSET=-300

	We see that the local time is now correct with the TZOFFSET=-300
	value, so we should change the HTTPD PROC or JCL to this value.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Add server parameter to enable/disable login request.

	A new optional "LOGIN" parameter has been added to the HTTPD
	server PARM='...' settings.

	When specified, the "LOGIN" parameter will accept the
	following options:
	ALL		Request login authentication for all request.
	CGI		Request login authentication for CGI request.
	GET		Request login authentication for GET request.
	HEAD	Request login authentication for HEAD request.
	POST	Request login authentication for POST request.
	NONE	Disables login authentcation for any request (default).

	Example JCL PARM:
	//HTTPD    EXEC PGM=HTTPD,
	// PARM=(...,'LOGIN=(CGI,GET)',...)

	When LOGIN for a given HTTP method is enabled AND
	the current user has not been authenticated then
	a Login Required form will be sent to the browser
	requesting Userid and Password.

	Note: RACF/RAKF is required when using the LOGIN paramater.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Add /login and /logout resource names.

	To support the server LOGIN options we have new "/login"
	and "/logout" ressource names so that user can request
	to login or logout from the server.

	When the user request "/login" a login form is sent
	to the browser. The user would then supply their
	Userid and Password values to "login".

	If the login is successful, a Sec-Token is created
	and stored as a HTTP Cookie in the server respponse.

	In the HTTP response headers sent by the server you would see:
	"Set-Cookie: Sec-Token=RIFqr0R1I7MTXyz7t6B7ZbHP+QYX+vIa33dMmD0vttY="

	In the HTTP request headers sent by the browser you would see:
	"Cookie: Sec-Token=RIFqr0R1I7MTXyz7t6B7ZbHP+QYX+vIa33dMmD0vttY="

	When the user request "/logout" the user is logged out of
	the HTTPD server. The Sec-Token is set to "deleted" and 
	expired. The browser will clear the Sec-Token Cookie from 
	future request.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Add "set login (all,cgi,get,head,post,none)" command to HTTPD server.

	A new "set login" command has been added to the HTTPD server.

	The following options can be supplied for "set login":
	ALL		Request login authentication for all request.
	CGI		Request login authentication for CGI request.
	GET		Request login authentication for GET request.
	HEAD	Request login authentication for HEAD request.
	POST	Request login authentication for POST request.
	NONE	No login authentication is required.

	Example: /F HTTPD,SET LOGIN NONE,CGI,GET
	/ 9.55.17 JOB 5973  HTTPD100I CONS(3) "SET LOGIN NONE,CGI,GET"
	/ 9.55.17 JOB 5973  HTTPD048I Login required for (CGI,GET) request

	Note: The NONE value clears all of the login flags. 
	The values LOGIN values that follow set the appropriate 
	login flags.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Add "display login" command to HTTPD server.

	A new "display login" command has been created to display
	the current login settings "HTTPD048I" message and any
	users that are currently logged in to the HTTPD server
	via "HTTPD070I" and "HTTPD071I" messages.

	Example: /F HTTPD,D LOGIN
	/13.08.14 JOB 5984  HTTPD100I CONS(3) "D LOGIN"
	/13.08.14 JOB 5984  HTTPD048I Login required for (ALL) request
	/13.08.14 JOB 5984  HTTPD070I Users Logged In: 1
	/13.08.14 JOB 5984  HTTPD071I User:HERC01   IP:192.168.1.163

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

HTTPD Change Log January 2024 - version 1.1.0

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
The JES2 status page (jesst.html) is showing invalid dates 
"Sun Feb 7 00:28:16 2106" for job Start and End Timestamps.

	This problem was the result of the mktime function
	failing to create a time_t value for years greater
	than 2020.

	The mktime function has been changed to allow year
	values up to 2038. The 2038 limit is due to the
	largest value that can be stored in a 32 bit variable.

	At some point we'll have to switch to using a 64 bit
	mktime function and 64 bit time_t values.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
The JES2 status page (jesst.html) fails to load a job list after
clearing the jes spool.

	This symptom came about after having some problems while
	tesing other fixes. The cause was the server was creating
	a JSON formatted list of jobs that had a ',' comma character 
	following the last job in the list. This "broke" the 
	JSON parser in the browser.

	The fix was to change the conditions in which we place a
	comma in the JSON job list. This avoids the parsing errors
	detected by some strict JSON parsers.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
FTP client using the PASV command fails with invalid host 
address being returned to the FTP client. If the FTP client 
is in passive mode, it will send a PASV command to the FTP
server as part of DIR or LS client side commands.

	The FTP server (within HTTPD) was using the localhost 
	address 127.0.0.1 when creating a passive listener 
	for the PASV command sent by the client. This restricted
	FTP clients to connecting to the FTP server via the 
	localhost address 127.0.0.1 only.

	We will now use the same host address that the client
	connected to when accessing the FTP server. This change
	ensures that we listen for a data connection on the 
	same IP address/adapter that the client used for the 
	initial connection to the FTP server.

	In some cases the FTP client may still fail if the server 
	or the client are behind a NAT or firewall that doesn't 
	implement an ALG (Application Layer Gateway) for FTP 
	connections. In that case using PORT instead of PASV 
	may be prefered.

	PASV is used when the FTP client has passive ON (default 
	on many FTP clients).
	PORT is used when the FTP client has passive OFF.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Some FTP clients using a GET command while in ASCII mode 
complain about extra LF characters. This also occurs with 
DIR and LS commands which are always in ASCII mode.

	When transfering data in ASCII mode, normal end-of-line NL
	character was converted into a single LF character when 
	converting the EBCDIC lines to ASCII lines.

	The FTP server will now convert end-of-line NL character
	into a CRLF sequence when in ASCII mode.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
The FTP server (in the HTTPD server) had a bug in that you could
only use MVS datasets OR UFS (Unix File System) path names.

	Basically if the UFS was initialized at startup of the HTTPD
	server, then you could only use the UFS directory and files.
	If you disabled UFS (NOFS parameter) then you could use MVS
	dataset names and not UFS path names.

	The FTP server has been changed to allow both UFS and MVS 
	datasets at the same time (provided UFS is initialized at 
	startup).

	UFS path names start with a "/" character.
	MVS dataset names start with a single quote "'" character.

	The FTP code received several bug fixes discovered along 
	the way. Really too many to keep up with. I'm sure it's 
	not perfect but I'm also sure it's a lot better than it was.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Sometimes the HTTPD server will fail to bind the listener sockets
for the server (HTTP and FTP) listener ports.

	This symptom occurs when TCPIP detects that a socket port and 
	address is still being used even if that socket has already 
	been closed. (socket is in "Closed Wait" condition) TCPIP
	will eventually clear this problem on its own.

	The server has been changed to detect the EADDRINUSE and retry
	the bind() at 10 second intervals (up to 10 times) before
	aborting the startup.

	12.12.01 STC  502  HTTPD030E bind() failed for HTTP port, rc=-1, error=48
	12.12.01 STC  502  HTTPD030I EADDRINUSE, waiting for TCPIP to release HTTP port=8080
	12.12.11 STC  502  HTTPD030I EADDRINUSE, waiting for TCPIP to release HTTP port=8080
	12.12.21 STC  502  HTTPD030I EADDRINUSE, waiting for TCPIP to release HTTP port=8080
	12.12.31 STC  502  HTTPD030I EADDRINUSE, waiting for TCPIP to release HTTP port=8080
	12.12.41 STC  502  HTTPD030I EADDRINUSE, waiting for TCPIP to release HTTP port=8080
	12.12.51 STC  502  HTTPD030I EADDRINUSE, waiting for TCPIP to release HTTP port=8080
	12.13.01 STC  502  HTTPD032I Listening for HTTP request on port 8080
	12.13.02 STC  502  FTPD0005I Listening for FTP  request on port 8021
	12.13.02 STC  502  HTTPD001I Server is READY

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Inconsistent timer posting of worker threads.

	The HTTPD server uses a thread manager for all the threads 
	that service work request. The thread manager is also
	a thread that runs the dispatcher for the threads
	that process server work request (HTTP and FTP).

	The dispatcher code	for the thread manager was rewritten 
	to insure that all of the worker threads are dispatched
	as specified by each workers option flags.

	If a worker thread wants timer post which occur at 1 second 
	intervals it can set the timer option flag. Otherwise
	the worker thread will only receive queued work
	request for processing (default no timer post).

	If a worker does not want queued work request it can
	set the no work option. It will not receive queued
	work request and it will not receive timer post unless
	it has also set the timer option. The worker thread 
	would only be posted when the server is shutting down.

	By default worker threads:
	.	Get posted for queued work only.
	.	Get posted when the server is shutting down.
	.	Do NOT get posted at 1 second intervals (timer).

