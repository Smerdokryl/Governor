<!-- saved from url=(0095)https://web.archive.org/web/20100310081009/http://www.rootkit.com/newsread_print.php?newsid=371 -->
<b>KEEPING BLIZZARD HONEST - Announcing the release of 'The Governor'</b><br>
<i>@ :: worthy ::</i>&nbsp;&nbsp;&nbsp;&nbsp; Oct 18 2005, 03:05 (UTC+0)<br>
<br>
<b>hoglund</b> writes: Blizzard, a subsidary of Vivendi, builds and markets the popular computer game known as World of Warcraft, which claims more than 4.5 million players worldwide.  Unknown to most players is the fact that World of Warcraft includes an embedded spyware(ref: <a href="https://web.archive.org/web/20100310081009/https://www.rootkit.com/newsread.php?newsid=369">is warden spyware?</a>) program which indiscriminantly reads data from all open windows and processes on the users computer.  The purpose of the warden is to verify compliance with the EULA and TOS.  While many welcome the warden as a means to catch cheaters who use 3rd party 'botting' programs, many others may find this a violation of privacy.<br>
<br>
The fact is that the warden client reads information from other processes on the computer.  Regardless of the reasons, this technically counts as 'spying' on a user.  So, reasons aside, the term 'spyware' is fitting.<br>
<br>
Rather than debate the morality of this behavior, I would like to give the consumers the power to make this decision for themselves.  I am releasing a program called 'The Governor'.  The Governor is very simple - it watches the activities of World of Warcraft, and clearly reports which data is being read from other processes.  The Governor makes no attempt to subvert or alter the behavior of the warden client, or World of Warcraft.  The Governor will not assist you in cheating.  The Governor exists for one reason, to tell you the truth.<br>
<br>
Here is the governor, released with FULL SOURCE.  There are no secrets or tricks.  See the warden in action for yourself:<br>
<br>
<a href="https://web.archive.org/web/20100310081009/http://www.rootkit.com/vault/hoglund/Governor.rar" target="_blank">http://www.rootkit.com/vault/hoglund/Governor.rar</a><br>
<br>
and, as a ZIP file,<br>
<br>
<a href="https://web.archive.org/web/20100310081009/http://www.rootkit.com/vault/hoglund/Governor.zip" target="_blank">http://www.rootkit.com/vault/hoglund/Governor.zip</a><br>
<br>
In the screenshot, you can see World of Warcraft reading memory from the processes running on my computer.<br>
<br>
Absolutely no reverse engineering is required to make the Governor work. The Governor monitors fully documented API calls which are offered by the Microsoft Windows operating system.  To monitor these API calls, the Governor uses a documented library called 'Detours', which is available from Microsoft.<br>
<br>
Will Blizzard ban me if I use The Governor?<br>
<br>
I have personally been running The Governor on a test account and there have been no problems.  The Governor does not modify the behavior of WoW.EXE or the warden.  The Governor is not designed to assist cheaters, and offers no mechanism to help cheaters hide their programs.<br>
<br>
But, that being said, Blizzard can choose to ban you for using a 3rd party program.  The Governor is a 3rd party program.  While the Governor poses absolutely no threat from a cheating aspect, it does expose the behavior of their warden client.  In my opinion, banning people for seeking the truth about warden would sink Blizzard to a new all-time low.  But, this isn't my decision.  I cannot guarantee you won't be banned.<br>
<br>
AN OPEN MESSAGE TO BLIZZARD<br>
Blizzard, it is within your right to attempt to make your computer game that way you wish it to be, and to attempt to catch cheaters.  But, reading the memory of other processes and windows that are not part of the World of Warcraft game client is a violation of privacy.  Making a violation of privacy legal in your EULA and TOS does not make it also moral.  It remains a violation of privacy.  Please refactor your policy in regards to scanning memory, and limit the warden to integrity checking of the game client's memory space, and please stop opening other processes and reading windows that do not belong to you.<br>
<br>
-Greg Hoglund
<p></p><hr noshade="" color="black" size="1">
<p>(c) www.rootkit.com /  http://www.rootkit.com/</p>