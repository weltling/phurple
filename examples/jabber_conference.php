<?php

ob_implicit_flush(true);

use Phurple\Client;
use Phurple\Account;
use Phurple\Conversation;
use Phurple\Connection;
use Phurple\Buddy;
use Phurple\BuddyList;

class JabberClient extends Client
{
	protected $buddy = NULL;
	protected $room = NULL;

	protected function initInternal()
	{/*{{{*/
		$this->buddy = "somebuddy@jabber.org";
		$this->room = "someroom@conference.jabber.org";
	}/*}}}*/

	protected function writeConv($conversation, $buddy, $alias, $message, $flags, $time)
	{/*{{{*/
		printf(	"(%s) %s %s: %s\n",
			is_object($buddy) ? $buddy->getName() : $conversation->getName(),
			date("H:i:s", $time),
			is_object($buddy) ? $buddy->getAlias() : $buddy,
			$message
		);

		if ($conversation->getAccount()->getUserName() == $buddy) {
			return;
		}

		if(slf::MESSAGE_NICK == ($flags & self::MESSAGE_NICK)) {
			$conversation->sendIM("$buddy, you said '$message'?");
		} else if(self::MESSAGE_RECV == ($flags & self::MESSAGE_RECV)) {
			$conversation->sendIM("random saying " . md5(uniqid()));
		}

	}/*}}}*/

	protected function onSignedOn($connection)
	{/*{{{*/
		$account = $connection->getAccount();

		$conversation = new Conversation(self::CONV_TYPE_CHAT, $account,  $this->room);

		if (!$conversation->isUserInChat($this->buddy)) {
			$conversation->inviteUser($this->buddy, "common buddy!");
			$conversation->sendIM("{$this->buddy}, just starting a conversation. hi there");
		} else {
			$conversation->sendIM("{$this->buddy}, got you ;)");
		}
	}/*}}}*/

	public function onConnectionError($connection, $code, $description)
	{/*{{{*/
		throw new \Exception($description, $code);
	}/*}}}*/

	protected function loopHeartBeat()
	{/*{{{*/
		static $countdown = 0;

		if ($countdown > 60) {
			$this->disconnect();
			$this->quitLoop();
		}

		$countdown++;
	}/*}}}*/

	protected function requestAction($title, $primary, $secondary, $default_action, $account, $who, $conversation, $actions)
	{/*{{{*/

		if ("Create New Room" == $title) {
			return 1;
		}

		return 0;
	}/*}}}*/
}

try {/*{{{*/
	$user_dir = "/tmp/phurple-test";
	if(!file_exists($user_dir) || !is_dir($user_dir)) {
		mkdir($user_dir);
	}

	JabberClient::setUserDir($user_dir);
	JabberClient::setUiId("TestUI");

	$client = JabberClient::getInstance();

    $client->addAccount("xmpp://myemail@gmail.com:mypassword@talk.google.com");

	$client->connect();

	$client->runLoop(1000);
} catch (Exception $e) {
	echo "[Phurple]: " . $e->getMessage() . "\n";
	die();
}/*}}}*/

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

