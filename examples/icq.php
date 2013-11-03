<?php

ob_implicit_flush(true);

use Phurple\Client;
use Phurple\Account;
use Phurple\Conversation;
use Phurple\Connection;
use Phurple\Buddy;
use Phurple\BuddyList;

class ICQClient extends Client
{
	protected $buddy = NULL;

	protected function initInternal()
	{/*{{{*/
		$this->buddy = "buddy_uin";
	}/*}}}*/

	protected function writeIm($conversation, $buddy, $message, $flags, $time)
	{/*{{{*/
		printf(	"(%s) %s %s: %s\n",
			is_object($buddy) ? $buddy->getName() : $conversation->getName(),
			date("H:i:s", $time),
			is_object($buddy) ? $buddy->getAlias() : $buddy,
			$message
		);

		if(self::MESSAGE_RECV == ($flags & self::MESSAGE_RECV)) {
			$conversation->sendIM("random saying " . md5(uniqid()));
		}

	}/*}}}*/

	protected function onSignedOn($connection)
	{/*{{{*/
		$account = $connection->getAccount();

		$conversation = new Conversation(self::CONV_TYPE_IM, $account,  $this->buddy);
		$conversation->sendIM("hello friend");
	}/*}}}*/
}

try {/*{{{*/
	$user_dir = "/tmp/phurple-test";
	if(!file_exists($user_dir) || !is_dir($user_dir)) {
		mkdir($user_dir);
	}

	ICQClient::setUserDir($user_dir);
	ICQClient::setUiId("TestUI");

	$client = ICQClient::getInstance();

	$client->addAccount("icq://my_uin:my_pass");

	$client->connect();

	$client->runLoop();
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

