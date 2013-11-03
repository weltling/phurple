<?php

ob_implicit_flush(true);

use Phurple\Client;
use Phurple\Account;
use Phurple\Conversation;
use Phurple\Connection;
use Phurple\Buddy;
use Phurple\BuddyList;

class IrcClient extends Client
{
	protected $buddy = NULL;
	protected $channel = NULL;

	protected function initInternal()
	{/*{{{*/
		$this->buddy = "somebuddy";
		$this->channel = "#somechannel";
	}/*}}}*/

	protected function writeIM($conversation, $buddy, $alias, $message, $flags, $time)
	{/*{{{*/
		printf(	"(%s) %s %s: %s\n",
			is_object($buddy) ? $buddy->getName() : $conversation->getName(),
			date("H:i:s", $time),
			is_object($buddy) ? $buddy->getAlias() : $buddy,
			$message
		);

		if(self::MESSAGE_NICK == ($flags & self::MESSAGE_NICK)) {
			$conversation->sendIM("$buddy, why did you say '$message'?");
		} else if(self::MESSAGE_RECV == ($flags & self::MESSAGE_RECV)) {
			$conversation->sendIM("Sorry, I only can answer this ;(");
		}

	}/*}}}*/

	protected function onSignedOn($connection)
	{/*{{{*/
		$account = $connection->getAccount();

		$conversation = new Conversation(self::CONV_TYPE_CHAT, $account,  $this->channel);
	}/*}}}*/

	protected function loopHeartBeat()
	{/*{{{*/
		static $countdown = 0;

		/* Loop for one minute and exit */
		if ($countdown > 60) {
			$this->disconnect();
			$this->quitLoop();
		}

		$countdown++;
	}/*}}}*/

	protected function chatJoined($conversation)
	{/*{{{*/
		if (!$conversation->isUserInChat($this->buddy)) {
			$conversation->inviteUser($this->buddy, "common buddy!");
			$conversation->sendIM("{$this->buddy}, just starting a conversation. hi there");
		} else {
			$conversation->sendIM("{$this->buddy}, got you ;)");
		}
	}/*}}}*/

}

try {/*{{{*/
	$user_dir = "/tmp/phurple-test";
	if(!file_exists($user_dir) || !is_dir($user_dir)) {
		mkdir($user_dir);
	}

	IrcClient::setUserDir($user_dir);
	IrcClient::setUiId("TestUI");

	$client = IrcClient::getInstance();

	$client->addAccount("irc://mybot@irc.freenode.net");

	$client->connect();

	$client->runLoop(1000);
} catch (Exception $e) {
	echo "[Phurple]: " . $e->getMessage() . "\n";
	die();
}/*}}}*/`

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

