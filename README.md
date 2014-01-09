# Description #

This libpurple PHP binding, which defines a set of internal classes, gives a possibility to use AOL and ICQ (OSCAR), Yahoo, Jabber, IRC and much more protocols directly from PHP. Write your own IM chat client in PHP, as simply as PHP enables it.

# Installation #

- install glib and libpurple development packages
- download and unpack
- phpize
- ./configure --with-phurple
- make && sudo make install

# TODO #

See TODO in the source.

# Resources #

- [http://phurple.php.belski.net/](http://phurple.php.belski.net/)
- [https://sourceforge.net/projects/phurple/](https://sourceforge.net/projects/phurple/)

# Example #

```php
ob_implicit_flush(true);

use Phurple\Client;
use Phurple\Account;
use Phurple\Conversation;
use Phurple\Connection;
use Phurple\Buddy;
use Phurple\BuddyList;

/**
 * @class MyClient
 *
 * Methods must be overloaded to work
 */
class MyClient extends Client
{
 
	private $someVar;
	private $someOtherVar;
 
	private $messageSent;
 
	/**
	 * Initalize internal class stuff
	 * 
	 * @access protected
	 * @return void
	 */
	protected function initInternal()
	{
		$this->someVar = "Hello World";
		$this->someOtherVar = time();
	}
 
	/**
	 * @access protected
	 * @param object $conversation
	 * @param object|string $buddy object if the user exists in our blist
	 * @param string $message
	 * @param int $flags
	 * @param int $time
	 * @return void
	 *
	 */
	protected function writeIM($conversation, $buddy, $message, $flags, $time)
	{
		printf(	"(%s) %s %s: %s\n",
			$conversation->getName() ? $conversation->getName() : $buddy->getName(),
			date("H:i:s", $time),
			!$buddy || is_string($buddy) ? $buddy : $buddy->getAlias(),
			$message
			);
	}
 
	/**
	 * This method is called when an account gets signed on
	 * @access protected
	 * @param object $conversation
	 * @return void
	 */
	protected function onSignedOn($connection)
	{
		$account = $connection->getAccount();
 
		if($account->get("auto-login")) {
 
			$buddy = "example@jabber.org";
 
			$conversation = new Conversation(Client::CONV_TYPE_IM, $account,  $buddy);
			$conversation->sendIM("Hello World");
			$this->messageSent = true;
		}
	}
 
	public function isMessageSent()
	{
		return $this->messageSent;
	}
 
	/**
	 * this method is not inherited from the Phurple\Client
	 *
	 */
	public function justForFun($param)
	{
		return "just for fun, the param is: $param";
	}
 
	/**
	 * Custom heartbeat method which is called in custom main loop
	 * the glibs main loop is integrated in
	 */
	public function myHeartBeat()
	{
		print "my own heartbeat\n";
	}
 
	/**
	 * Authorize remote users, who was added us to their blist. The method must return true,
	 * if the remote user must be authorized, false otherwise.
	 * @access protected
	 * @param object $account
	 * @param string $remote_user
	 * @param string $message
	 * @param boolean $on_list
	 * @return boolean 
	 */
	protected function authorizeRequest($account, $remote_user, $message, $on_list)
	{
		if (!$on_list) {
			$b = new Buddy($account, $remote_user);
			$account->addBuddy($b);
		}

		return true;
	}
}
 
try {
 
	$user_dir = "/tmp/phurple-test";
	if(!file_exists($user_dir) || !is_dir($user_dir)) {
		mkdir($user_dir);
	}
	MyClient::setUserDir($user_dir);
 
	MyClient::setDebug(true);
	MyClient::setUiId("TestUI");
 
	$client = MyClient::getInstance();
 
    $client->addAccount("xmpp://example@gmail.com:some_password@talk.google.com");
 
	$client->connect();
 
	/**
	 * We can't simply break the loop if the message is sent,
	 * because it's going into the queue first and gets sent
	 * a bit later, so lets give the script more $countdown
	 * iterations so that it has a chance to really finish
	 * the job
	 */
	$countdown = 10;
 
	/**
	 * This conains how many ms the script should sleep
	 * until next iteration. Note, that this brought
	 * really good performance anvantage comparing with
	 * the while(true){} construct.
	 */
	$sleep_time = 300;
 
	while($countdown > 0) {
		/**
		 * Do one glib event loop iteration.
		 */
		$client->iterate();
 
		/**
		 * Run a custom heart beat method cause
		 * no main loop is running. Or run whatever we want.
		 */
		$client->myHeartBeat();
 
		/**
		 * Check if the message is sent to start countdown.
		 */
		if($client->isMessageSent()) {
			$countdown--;
		}
 
		usleep($sleep_time);
	}
 
	exit(0);
 
} catch (Exception $e) {
	echo "[Phurple]: " . $e->getMessage() . "\n";
	die();
}
```




Enjoy :)
